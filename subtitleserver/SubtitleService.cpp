#define LOG_TAG "SubtitleService"

#include <stdlib.h>
#include <utils/Log.h>
#include <utils/CallStack.h>
#include "SocketServer.h"
#include "DataSourceFactory.h"


//#include "AndroidDisplay.h"

#include "SubtitleService.h"


SubtitleService::SubtitleService() {
    mIsInfoRetrieved = false;
    ALOGD("%s", __func__);
    mStarted = false;
    mIsInfoRetrieved = false;
    mFmqReceiver = nullptr;
    mDumpMaps = 0;
    mSubtiles = nullptr;
    mDataSource = nullptr;
    mUserDataAfd = nullptr;
}

SubtitleService::~SubtitleService() {
    ALOGD("%s", __func__);
    //android::CallStack here(LOG_TAG);
    if (mFmqReceiver != nullptr) {
        mFmqReceiver->unregistClient(mDataSource);
        mFmqReceiver = nullptr;
    }
}


void SubtitleService::setupDumpRawFlags(int flagMap) {
    mDumpMaps = flagMap;
}

bool SubtitleService::startFmqReceiver(std::unique_ptr<FmqReader> reader) {
    if (mFmqReceiver != nullptr) {
        ALOGD("ALOGE error! why still has a reference?");
        mFmqReceiver = nullptr;
    }

    mFmqReceiver = std::make_unique<FmqReceiver>(std::move(reader));
    ALOGD("add :%p", mFmqReceiver.get());

    if (mDataSource != nullptr) {
        mFmqReceiver->registClient(mDataSource);
    }

    return true;
}
bool SubtitleService::stopFmqReceiver() {
    if (mFmqReceiver != nullptr && mDataSource != nullptr) {
        ALOGD("release :%p", mFmqReceiver.get());
        mFmqReceiver->unregistClient(mDataSource);
        mFmqReceiver = nullptr;
        return true;
    }

    return false;
}

bool SubtitleService::startSubtitle(std::vector<int> fds, int trackId, SubtitleIOType type, ParserEventNotifier *notifier) {
    ALOGD("%s  type:%d", __func__, type);
    std::unique_lock<std::mutex> autolock(mLock);
    if (mStarted) {
        ALOGD("Already started, exit");
        return false;
    } else {
        mStarted = true;
    }

    bool hasExtSub = fds.size() > 0;
    bool hasExtraFd = fds.size() > 1;
    std::shared_ptr<Subtitle> subtitle(new Subtitle(hasExtSub, trackId, notifier));

    std::shared_ptr<DataSource> datasource = DataSourceFactory::create(
            hasExtSub ? fds[0] : -1,
            hasExtraFd ? fds[1] : -1,
            type);

    if (mDumpMaps & (1<<SUBTITLE_DUMP_SOURCE)) {
        datasource->enableSourceDump(true);
    }
    if (nullptr == datasource) {
        ALOGD("Error, %s data Source is null!", __func__);
        return false;
    }

    mDataSource = datasource;
    if (TYPE_SUBTITLE_DTVKIT_DVB == mSubParam.subType) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.dtvkitDvbParam);
    } else if (TYPE_SUBTITLE_DTVKIT_TELETEXT == mSubParam.subType) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.ttParam);
    } else if (TYPE_SUBTITLE_DTVKIT_SCTE27== mSubParam.subType) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.scteParam);
    }
   // schedule subtitle
    subtitle->scheduleStart();

   // schedule subtitle
    //subtitle.attach input source
    subtitle->attachDataSource(datasource, subtitle);

    // subtitle.attach display

   // schedule subtitle
//    subtitle->scheduleStart();
    //we only update params for DTV. because we do not wait dtv and
    // CC parser run the same time

    ALOGD("setParameter on start: %d, dtvSubType=%d",
        mSubParam.isValidDtvParams(), mSubParam.dtvSubType);
    // TODO: revise,
    if (!hasExtSub && (mSubParam.isValidDtvParams() || mSubParam.dtvSubType <= 0)) {
        ALOGD("setParameter on start");
        subtitle->setParameter(&mSubParam);
    }

    mSubtiles = subtitle;
    mDataSource = datasource;
    if (mFmqReceiver != nullptr) {
        mFmqReceiver->registClient(mDataSource);
    }
    return true;
}

bool SubtitleService::resetForSeek() {

    if (mSubtiles != nullptr) {
//        return mSubtiles->resetForSeek();
    }

    return false;
}



int SubtitleService::updateVideoPosAt(int timeMills) {
    static int test = 0;
    if (test++ %100 == 0)
        ALOGD("%s: %d(called %d times)", __func__, timeMills, test);

    if (mSubtiles) {
        return mSubtiles->onMediaCurrentPresentationTime(timeMills);
    }

    return -1;
;
}


//when play by ctc, this operation is  before startSubtitle,
//so keep struct mSub_param_t and when startSubtitle setParameter
void SubtitleService::setSubType(int type) {
    mSubParam.dtvSubType = (DtvSubtitleType)type;
    mSubParam.update();
    //return 0;
}

void SubtitleService::setDemuxId(int demuxId) {
    switch (mSubParam.dtvSubType) {
        case  DTV_SUB_DTVKIT_SCTE27:
            mSubParam.scteParam.demuxId = demuxId;
        break;
        case DTV_SUB_DTVKIT_DVB:
            mSubParam.dtvkitDvbParam.demuxId = demuxId;
        break;
        case DTV_SUB_DTVKIT_TELETEXT:
            mSubParam.ttParam.demuxId = demuxId;
        break;
        default:
        break;
    }
    if (NULL == mDataSource )
      return;
    if (mSubParam.subType == TYPE_SUBTITLE_DTVKIT_DVB)  {
        mDataSource->updateParameter(mSubParam.subType, &mSubParam.dtvkitDvbParam);
    } else if (mSubParam.subType == TYPE_SUBTITLE_DTVKIT_TELETEXT) {
        mDataSource->updateParameter(mSubParam.subType, &mSubParam.ttParam);
    } else if (mSubParam.subType == TYPE_SUBTITLE_DTVKIT_SCTE27) {
        mDataSource->updateParameter(mSubParam.subType, &mSubParam.scteParam);
    }
}

void SubtitleService::setSecureLevel(int flag) {
    switch (mSubParam.dtvSubType) {
        case  DTV_SUB_DTVKIT_SCTE27:
            mSubParam.scteParam.flag = flag;
        break;
        case DTV_SUB_DTVKIT_DVB:
            mSubParam.dtvkitDvbParam.flag = flag;
        break;
        case DTV_SUB_DTVKIT_TELETEXT:
            mSubParam.ttParam.flag = flag;
        break;
        default:
        break;
    }
    if (NULL == mDataSource )
      return;
    if (mSubParam.subType == TYPE_SUBTITLE_DTVKIT_DVB)  {
        mDataSource->updateParameter(mSubParam.subType, &mSubParam.dtvkitDvbParam);
    } else if (mSubParam.subType == TYPE_SUBTITLE_DTVKIT_TELETEXT) {
        mDataSource->updateParameter(mSubParam.subType, &mSubParam.ttParam);
    } else if (mSubParam.subType == TYPE_SUBTITLE_DTVKIT_SCTE27) {
        mDataSource->updateParameter(mSubParam.subType, &mSubParam.scteParam);
    }
}

void SubtitleService::setSubPid(int pid, int onid, int tsid) {
    switch (mSubParam.dtvSubType) {
        case  DTV_SUB_SCTE27:
            [[fallthrough]];
        case  DTV_SUB_DTVKIT_SCTE27:
            mSubParam.scteParam.SCTE27_PID = pid;
        break;
        case DTV_SUB_DTVKIT_DVB:
            mSubParam.dtvkitDvbParam.pid = pid;
        break;
        case DTV_SUB_DTVKIT_TELETEXT:
            mSubParam.ttParam.pid = pid; // for demux

            // startSubtitle may use these.
            mSubParam.ttParam.onid = onid;
            mSubParam.ttParam.tsid = tsid;
        break;
        default:
        break;
    }
    if (NULL == mDataSource )
      return;
    if (mSubParam.subType == TYPE_SUBTITLE_DTVKIT_DVB)  {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.dtvkitDvbParam);
    } else if (mSubParam.subType == TYPE_SUBTITLE_DTVKIT_TELETEXT) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.ttParam);
    }
}

void SubtitleService::setSubPageId(int pageId) {
    switch (mSubParam.dtvSubType) {
        case DTV_SUB_DTVKIT_DVB:
            mSubParam.dtvkitDvbParam.compositionId = pageId;
            if (mSubtiles != nullptr) {
                mSubtiles->setParameter(&mSubParam);
            }
        break;
        case DTV_SUB_DTVKIT_TELETEXT:
            mSubParam.ttParam.magazine = pageId;
        break;
        default:
        break;
    }
}
void SubtitleService::setSubAncPageId(int ancPageId) {
    switch (mSubParam.dtvSubType) {
        case DTV_SUB_DTVKIT_DVB:
            mSubParam.dtvkitDvbParam.ancillaryId = ancPageId;
            if (mSubtiles != nullptr) {
                mSubtiles->setParameter(&mSubParam);
            }
        break;
        case DTV_SUB_DTVKIT_TELETEXT:
            mSubParam.ttParam.page = ancPageId;
        break;
        default:
        break;
    }
}

void SubtitleService::setChannelId(int channelId) {
    mSubParam.ccParam.ChannelID = channelId;
}


void SubtitleService::setClosedCaptionVfmt(int vfmt) {
     mSubParam.ccParam.vfmt = vfmt;
}

void SubtitleService::setClosedCaptionLang(const char *lang) {
    ALOGD("lang=%s", lang);
    if (lang != nullptr && strlen(lang)<64) {
        strcpy(mSubParam.ccParam.lang, lang);
    }
}

/*
    mode: 1 for the player id setting;
             2 for the media id setting.
*/
void SubtitleService::setPipId(int mode, int id) {
    ALOGD("setPipId mode = %d, id = %d\n", mode, id);
    bool same = true;
    if (PIP_PLAYER_ID== mode) {
        mSubParam.playerId = id;
        if (mUserDataAfd != nullptr) {
            mUserDataAfd->setPlayerId(id);
        }
    } else if (PIP_MEDIASYNC_ID == mode) {
        if (mSubParam.mediaId == id) {
            same = true;
        } else {
            same = false;
        }
        mSubParam.mediaId = id;
    }
    if (NULL == mDataSource )
        return;
    mDataSource->setPipId(mode, id);
   if (/*(DTV_SUB_DTVKIT_SCTE27 == mSubParam.dtvSubType) && */(mode == PIP_MEDIASYNC_ID) && (!same)) {
       mSubtiles->setParameter(&mSubParam);
   }
}

bool SubtitleService::ttControl(int cmd, int magazine, int page, int regionId, int param) {
    // DO NOT update the entire parameter.
    ALOGD("ttControl cmd:%d, magazine=%d, page:%d, regionId:%d",cmd, magazine, page, regionId);

    switch (cmd) {
        case TT_EVENT_GO_TO_PAGE:
        case TT_EVENT_GO_TO_SUBTITLE:
            mSubParam.ttParam.pageNo = magazine;
            mSubParam.ttParam.subPageNo = page;
            break;
        case TT_EVENT_SET_REGION_ID:
            mSubParam.ttParam.regionId = regionId;
            break;
    }
    mSubParam.ttParam.event = (TeletextEvent)cmd;
    mSubParam.subType = TYPE_SUBTITLE_DVB_TELETEXT;
    ALOGD("event=%d", mSubParam.ttParam.event);
    if (mSubtiles != nullptr) {
        return mSubtiles->setParameter(&mSubParam);
    }
    ALOGD("%s mSubtiles null", __func__);

    return false;
}

int SubtitleService::ttGoHome() {
    ALOGD("%s ", __func__);
    return mSubtiles->setParameter(&mSubParam);
}

int SubtitleService::ttGotoPage(int pageNo, int subPageNo) {
    ALOGD("debug##%s ", __func__);
    mSubParam.ttParam.ctrlCmd = CMD_GO_TO_PAGE;
    mSubParam.ttParam.pageNo = pageNo;
    mSubParam.ttParam.subPageNo = subPageNo;
    mSubParam.subType = TYPE_SUBTITLE_DVB_TELETEXT;
    return mSubtiles->setParameter(&mSubParam);

}

int SubtitleService::ttNextPage(int dir) {
    ALOGD("debug##%s ", __func__);
    mSubParam.ttParam.ctrlCmd = CMD_NEXT_PAGE;
    mSubParam.subType = TYPE_SUBTITLE_DVB_TELETEXT;
    mSubParam.ttParam.pageDir = dir;
    return mSubtiles->setParameter(&mSubParam);
}


int SubtitleService::ttNextSubPage(int dir) {
    mSubParam.ttParam.ctrlCmd = CMD_NEXT_SUB_PAGE;
    mSubParam.subType = TYPE_SUBTITLE_DVB_TELETEXT;
    mSubParam.ttParam.subPageDir = dir;
    return mSubtiles->setParameter(&mSubParam);
}

bool SubtitleService::userDataOpen(ParserEventNotifier *notifier) {
    ALOGD("%s ", __func__);
    //default start userdata monitor afd
    if (mUserDataAfd == nullptr) {
        mUserDataAfd = std::shared_ptr<UserDataAfd>(new UserDataAfd());
        mUserDataAfd->start(notifier);
    }
    return true;
}

bool SubtitleService::userDataClose() {
    ALOGD("%s ", __func__);
    if (mUserDataAfd != nullptr) {
        mUserDataAfd->stop();
        mUserDataAfd = nullptr;
    }

    return true;
}

bool SubtitleService::stopSubtitle() {
    ALOGD("%s", __func__);
    std::unique_lock<std::mutex> autolock(mLock);
    if (!mStarted) {
        ALOGD("Already stopped, exit");
        return false;
    } else {
        mStarted = false;
    }

    if (mSubtiles != nullptr) {
        mSubtiles->dettachDataSource(mSubtiles);
    }

    if (mFmqReceiver != nullptr && mDataSource != nullptr) {
        mFmqReceiver->unregistClient(mDataSource);
        //mFmqReceiver = nullptr;
    }

    mDataSource = nullptr;
    mSubtiles = nullptr;
    mSubParam.subType = TYPE_SUBTITLE_INVALID;
    mSubParam.dtvSubType = DTV_SUB_INVALID;
    return true;
}

int SubtitleService::totalSubtitles() {
    if (mSubtiles == nullptr) {
        ALOGE("Not ready or exited, ignore request!");
        return -1;
    }
    // TODO: impl a state of subtitle, throw error when call in wrong state
    // currently we simply wait a while and return
    if (mIsInfoRetrieved) {
        return mSubtiles->getTotalSubtitles();
    }
    for (int i=0; i<5; i++) {
        if ((mSubtiles != nullptr) && (mSubtiles->getTotalSubtitles() >= 0)) {
            return mSubtiles->getTotalSubtitles();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    mIsInfoRetrieved = true;
    return -1;
}

int SubtitleService::subtitleType() {
    // TODO: impl a state of subtitle, throw error when call in wrong state
    return mSubtiles->getSubtitleType();
}

std::string SubtitleService::currentLanguage() {
    if (mSubtiles != nullptr) {
        return mSubtiles->currentLanguage();
    }

    return std::string("N/A");
}


void SubtitleService::dump(int fd) {
    dprintf(fd, "\n\n SubtitleService:\n");
    dprintf(fd, "--------------------------------------------------------------------------------------\n");

    dprintf(fd, "SubParams:\n");
    dprintf(fd, "    isDtvSub: %d", mSubParam.isValidDtvParams());
    mSubParam.dump(fd, "    ");

    if (mFmqReceiver != nullptr) {
        dprintf(fd, "\nFastMessageQueue: %p\n", mFmqReceiver.get());
        mFmqReceiver->dump(fd, "    ");
    }

    dprintf(fd, "\nDataSource: %p\n", mDataSource.get());
    if (mDataSource != nullptr) {
        mDataSource->dump(fd, "    ");
    }

    if (mSubtiles != nullptr) {
        return mSubtiles->dump(fd);
    }
}

