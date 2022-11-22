#define LOG_TAG "Subtitle"

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <utils/CallStack.h>
#include <utils/Log.h>

#include <chrono>
#include <thread>

#include "Subtitle.h"
#include "Parser.h"
#include "ParserFactory.h"

//#include "BitmapDisplay.h"

Subtitle::Subtitle() :
    mSubtitleTracks(-1),
    mCurrentSubtitleType(-1),
    mRenderTime(-1),
    mParserNotifier(nullptr),
    mExitRequested(false),
    mThread(nullptr),
    mPendingAction(-1),
    mIsExtSub(false),
    mIdxSubTrack(-1)
 {
    ALOGD("%s", __func__);
}

Subtitle::Subtitle(bool isExtSub, int trackId, ParserEventNotifier *notifier) :
        mSubtitleTracks(-1),
        mCurrentSubtitleType(-1),
        mRenderTime(-1),
        mExitRequested(false),
        mThread(nullptr),
        mPendingAction(-1)
{
    ALOGD("%s", __func__);
    mParserNotifier = notifier;
    mIsExtSub = isExtSub;
    mIdxSubTrack = trackId;

    mSubPrams = std::shared_ptr<SubtitleParamType>(new SubtitleParamType());
    mSubPrams->dtvkitDvbParam.demuxId = 0;
    mSubPrams->dtvkitDvbParam.pid= 0;
    mSubPrams->dtvkitDvbParam.ancillaryId = 0;
    mSubPrams->dtvkitDvbParam.compositionId= 0;

    mPresentation = std::shared_ptr<Presentation>(new Presentation(nullptr));
}


Subtitle::~Subtitle() {
    ALOGD("%s", __func__);
    //android::CallStack(LOG_TAG);

    mExitRequested = true;
    mCv.notify_all();
    if (mThread != nullptr) {

        mThread->join();
    }

    if (mDataSource != nullptr) {
        mDataSource->stop();
    }

    if (mParser != nullptr) {
        mParser->stopParser();
        mPresentation->stopPresent();
        mParser = nullptr;
    }
}

void Subtitle::attachDataSource(std::shared_ptr<DataSource> source, std::shared_ptr<InfoChangeListener>listener) {
     ALOGD("%s", __func__);
    mDataSource = source;
    mDataSource->registerInfoListener(listener);
    mDataSource->start();
}

void Subtitle::dettachDataSource(std::shared_ptr<InfoChangeListener>listener) {
    mDataSource->unregisteredInfoListener(listener);
}


void Subtitle::onSubtitleChanged(int newTotal) {
    if (newTotal == mSubtitleTracks) return;
    ALOGD("onSubtitleChanged:%d", newTotal);
    mSubtitleTracks = newTotal;
}

void Subtitle::onRenderStartTimestamp(int64_t startTime) {
    //mRenderTime = renderTime;
    if (mPresentation != nullptr) {
        mPresentation->notifyStartTimeStamp(startTime);
    }
}

void Subtitle::onRenderTimeChanged(int64_t renderTime) {
    //ALOGD("onRenderTimeChanged:%lld", renderTime);
    mRenderTime = renderTime;
    if (mPresentation != nullptr) {
        mPresentation->syncCurrentPresentTime(mRenderTime);
    }
}

void Subtitle::onGetLanguage(std::string &lang) {
    mCurrentLanguage = lang;
}

void Subtitle::onTypeChanged(int newType) {

    std::unique_lock<std::mutex> autolock(mMutex);

    if (newType == mCurrentSubtitleType) return;

    ALOGD("onTypeChanged:%d", newType);
    if (newType <= TYPE_SUBTITLE_INVALID || newType >= TYPE_SUBTITLE_MAX) {
        ALOGD("Error! invalid type!%d", newType);
        return;
    }
    if (newType == TYPE_SUBTITLE_DTVKIT_SCTE27) {
        return;
    }
    mCurrentSubtitleType = newType;
    mSubPrams->subType = static_cast<SubtitleType>(newType);

    // need handle
    mPendingAction = ACTION_SUBTITLE_RECEIVED_SUBTYPE;
    mCv.notify_all(); // let handle it
}

int Subtitle::onMediaCurrentPresentationTime(int ptsMills) {
    unsigned int pts = (unsigned int)ptsMills;

    if (mPresentation != nullptr) {
        mPresentation->syncCurrentPresentTime(pts);
    }
    return 0;
}

int Subtitle::getTotalSubtitles() {
    return mSubtitleTracks;
}


int Subtitle::getSubtitleType() {
    return mCurrentSubtitleType;
}
std::string Subtitle::currentLanguage() {
    return mCurrentLanguage;
}

/* currently, we  */
bool Subtitle::setParameter(void *params) {
    std::unique_lock<std::mutex> autolock(mMutex);
    SubtitleParamType *p = new SubtitleParamType();
    *p = *(static_cast<SubtitleParamType*>(params));

    //android::CallStack here("here");

    mSubPrams = std::shared_ptr<SubtitleParamType>(p);
    mSubPrams->update();// need update before use

    //process ttx skip page func.
    if ((mSubPrams->subType == TYPE_SUBTITLE_DVB_TELETEXT) || (mSubPrams->subType == TYPE_SUBTITLE_DTVKIT_TELETEXT)
        || (mSubPrams->subType == TYPE_SUBTITLE_DTVKIT_DVB)) {
        mPendingAction = ACTION_SUBTITLE_SET_PARAM;
        mCv.notify_all();
        return true;
    } else if ((mSubPrams->subType == TYPE_SUBTITLE_SCTE27) || (mSubPrams->subType == TYPE_SUBTITLE_DTVKIT_SCTE27)) {
        mPendingAction = ACTION_SUBTITLE_RESET_MEDIASYNC;
        mCv.notify_all();
        return true;
    }
    mPendingAction = ACTION_SUBTITLE_RECEIVED_SUBTYPE;
    mCv.notify_all();
    return true;
}

bool Subtitle::resetForSeek() {

    mPendingAction = ACTION_SUBTITLE_RESET_FOR_SEEK;
    mCv.notify_all();
    return true;
}

// TODO: actually, not used now
void Subtitle::scheduleStart() {
    ALOGD("scheduleStart:%d", mSubPrams->subType);
    if (nullptr != mDataSource) {
        mDataSource->start();
    }

    mThread = std::shared_ptr<std::thread>(new std::thread(&Subtitle::run, this));
}

// Run in a new thread. any access to this object's member field need protected by lock
void Subtitle::run() {
    // check exit
    ALOGD("run mExitRequested:%d, mSubPrams->subType:%d", mExitRequested, mSubPrams->subType);

    while (!mExitRequested) {
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::unique_lock<std::mutex> autolock(mMutex);
        //mCv.wait(autolock);
        mCv.wait_for(autolock, std::chrono::milliseconds(100));

        if (mIsExtSub && mParser == nullptr) {
            mSubPrams->subType = TYPE_SUBTITLE_EXTERNAL;// if mFd > 0 is Ext sub
            mSubPrams->idxSubTrackId = mIdxSubTrack;
            mParser = ParserFactory::create(mSubPrams, mDataSource);
            if (mParser == nullptr) {
                ALOGE("Parser creat failed, break!");
                break;
            }
            mParser->startParser(mParserNotifier, mPresentation.get());
            mPresentation->startPresent(mParser);
            mPendingAction = -1; // No need handle
        }else if(mIsExtSub && mParser->getParseType() == TYPE_SUBTITLE_CLOSED_CAPTION){
            /*
             * When ccparser is created by default,
             * it can be changed to the correct parser type
             * when specifying the parser type subsequently
             */
            mParser->stopParser();
            mPresentation->stopPresent();
            mParser = nullptr;
            mSubPrams->subType = TYPE_SUBTITLE_EXTERNAL;// if mFd > 0 is Ext sub
            mSubPrams->idxSubTrackId = mIdxSubTrack;
            mParser = ParserFactory::create(mSubPrams, mDataSource);
            if (mParser == nullptr) {
                ALOGE("Parser creat failed, break!");
                break;
            }
            mParser->startParser(mParserNotifier, mPresentation.get());
            mPresentation->startPresent(mParser);
            mPendingAction = -1; // No need handle
        }

        switch (mPendingAction) {
            case ACTION_SUBTITLE_SET_PARAM: {
                bool createAndStart = false;
                if (mParser == nullptr) {
                    // when the first time start subtitle, some parameter may affect the behavior
                    // such as cached teletext. we use tsid/onid/pid to check need use cached ttx or not
                    createAndStart = true;
                    mParser = ParserFactory::create(mSubPrams, mDataSource);
                } else if(mParser->getParseType() == TYPE_SUBTITLE_CLOSED_CAPTION){
                    /*
                     * When ccparser is created by default,
                     * it can be changed to the correct parser type
                     * when specifying the parser type subsequently
                     */
                    mParser->stopParser();
                    mPresentation->stopPresent();
                    mParser = nullptr;
                    createAndStart = true;
                    mParser = ParserFactory::create(mSubPrams, mDataSource);
                }
                ALOGD("run ACTION_SUBTITLE_SET_PARAM %d %d", mSubPrams->subType, TYPE_SUBTITLE_CLOSED_CAPTION);
                if (mSubPrams->subType == TYPE_SUBTITLE_DTVKIT_DVB) {
                    mParser->updateParameter(TYPE_SUBTITLE_DTVKIT_DVB, &mSubPrams->dtvkitDvbParam);
                } else if (mSubPrams->subType == TYPE_SUBTITLE_DTVKIT_TELETEXT
                         || mSubPrams->subType == TYPE_SUBTITLE_DVB_TELETEXT) {
                    mParser->updateParameter(TYPE_SUBTITLE_DVB_TELETEXT, &mSubPrams->ttParam);
                } else if (mSubPrams->subType == TYPE_SUBTITLE_DTVKIT_SCTE27) {
                    mParser->updateParameter(TYPE_SUBTITLE_DTVKIT_SCTE27, &mSubPrams->scteParam);
                } else if (mSubPrams->subType == TYPE_SUBTITLE_CLOSED_CAPTION) {
                    mParser->updateParameter(TYPE_SUBTITLE_CLOSED_CAPTION, &mSubPrams->ccParam);
                }

                if (createAndStart) {
                    mParser->startParser(mParserNotifier, mPresentation.get());
                    mPresentation->startPresent(mParser);
                }
            }
            break;
            case ACTION_SUBTITLE_RECEIVED_SUBTYPE: {
                ALOGD("ACTION_SUBTITLE_RECEIVED_SUBTYPE, type:%d", mSubPrams->subType);
                if (mSubPrams->subType == TYPE_SUBTITLE_CLOSED_CAPTION || mSubPrams->subType == TYPE_SUBTITLE_INVALID) {
                    ALOGD("CC type or invalid type, break, do nothings!");
                    break;
                } else if (mSubPrams->subType == TYPE_SUBTITLE_DVB_TELETEXT && mParser != nullptr && mParser->getParseType() == TYPE_SUBTITLE_DVB_TELETEXT) {
                    ALOGD("Already create parser,break do nothing");
                    break;
                }
                if (mParser != nullptr) {
                    mParser->stopParser();
                    mPresentation->stopPresent();
                    mParser = nullptr;
                }
                mParser = ParserFactory::create(mSubPrams, mDataSource);
                mParser->startParser(mParserNotifier, mPresentation.get());
                mPresentation->startPresent(mParser);
                if (mSubPrams->subType == TYPE_SUBTITLE_DTVKIT_DVB) {
                    mParser->updateParameter(TYPE_SUBTITLE_DTVKIT_DVB, &mSubPrams->dtvkitDvbParam);
                } else if (mSubPrams->subType == TYPE_SUBTITLE_DTVKIT_TELETEXT) {
                    mParser->updateParameter(TYPE_SUBTITLE_DVB_TELETEXT, &mSubPrams->ttParam);
                } else if (mSubPrams->subType == TYPE_SUBTITLE_DTVKIT_SCTE27) {
                    mParser->updateParameter(TYPE_SUBTITLE_DTVKIT_SCTE27, &mSubPrams->scteParam);
                }
            }
            break;
            case ACTION_SUBTITLE_RESET_MEDIASYNC:
                if (mParser != nullptr) {
                    mParser->setPipId(2, mSubPrams->mediaId);
            }
            break;
            case ACTION_SUBTITLE_RESET_FOR_SEEK:
                if (mParser != nullptr) {
                    mParser->resetForSeek();
                }

                if (mPresentation != nullptr) {
                    mPresentation->resetForSeek();
                }
            break;
        }
        // handled
        mPendingAction = -1;

        // wait100ms, still no parser, then start default CC
        if (mParser == nullptr) {
            ALOGD("No parser found, create default!");
            // start default parser, normally, this is CC
            mParser = ParserFactory::create(mSubPrams, mDataSource);
            if (mParser == nullptr) {
                ALOGE("Parser creat failed, break!");
                break;
            }
            mParser->startParser(mParserNotifier, mPresentation.get());
            mPresentation->startPresent(mParser);
        }

    }
}

void Subtitle::dump(int fd) {
    dprintf(fd, "\n\n Subtitle:\n");
    dprintf(fd, "--------------------------------------------------------------------------------------\n");
    dprintf(fd, "isExitedRequested? %s\n", mExitRequested?"Yes":"No");
    dprintf(fd, "PendingAction: %d\n", mPendingAction);
    dprintf(fd, "\n");
    dprintf(fd, "DataSource  : %p\n", mDataSource.get());
    dprintf(fd, "Presentation: %p\n", mPresentation.get());
    dprintf(fd, "Parser      : %p\n", mParser.get());
    dprintf(fd, "\n");
    dprintf(fd, "Total Subtitle Tracks: %d\n", mSubtitleTracks);
    dprintf(fd, "Current Subtitle type: %d\n", mCurrentSubtitleType);
    dprintf(fd, "Current Video PTS    : %lld\n", mRenderTime);

    if (mSubPrams != nullptr) {
        dprintf(fd, "\nSubParams:\n");
        mSubPrams->dump(fd, "  ");
    }

    if (mPresentation != nullptr) {
        dprintf(fd, "\nPresentation: %p\n", mPresentation.get());
        mPresentation->dump(fd, "   ");
    }

   // std::shared_ptr<Parser> mParser;
    if (mParser != nullptr) {
        dprintf(fd, "\nParser: %p\n", mParser.get());
        mParser->dump(fd, "   ");
    }


}

