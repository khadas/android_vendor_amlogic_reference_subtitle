/*
 * Copyright (C) 2014-2019 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "SubtitleService"

#include <stdlib.h>

#include "SubtitleLog.h"
#include <utils/CallStack.h>

#include "SocketServer.h"
#include "DataSourceFactory.h"
#include "SubtitleService.h"


SubtitleService::SubtitleService() {
    mIsInfoRetrieved = false;
    SUBTITLE_LOGI("%s", __func__);
    mStarted = false;
    mIsInfoRetrieved = false;
    mFmqReceiver = nullptr;
    mDumpMaps = 0;
    mSubtiles = nullptr;
    mDataSource = nullptr;
    mUserDataAfd = nullptr;
}

SubtitleService::~SubtitleService() {
    SUBTITLE_LOGI("%s", __func__);
    //android::CallStack here(LOG_TAG);
    if (mFmqReceiver != nullptr) {
        mFmqReceiver->unregisterClient(mDataSource);
        mFmqReceiver = nullptr;
    }
}


void SubtitleService::setupDumpRawFlags(int flagMap) {
    mDumpMaps = flagMap;
}

bool SubtitleService::startFmqReceiver(std::unique_ptr<FmqReader> reader) {
    if (mFmqReceiver != nullptr) {
        SUBTITLE_LOGI("SUBTITLE_LOGE error! why still has a reference?");
        mFmqReceiver = nullptr;
    }

    mFmqReceiver = std::make_unique<FmqReceiver>(std::move(reader));
    SUBTITLE_LOGI("add :%p", mFmqReceiver.get());

    if (mDataSource != nullptr) {
        mFmqReceiver->registClient(mDataSource);
    }

    return true;
}
bool SubtitleService::stopFmqReceiver() {
    if (mFmqReceiver != nullptr && mDataSource != nullptr) {
        SUBTITLE_LOGI("release :%p", mFmqReceiver.get());
        mFmqReceiver->unregisterClient(mDataSource);
        mFmqReceiver = nullptr;
        return true;
    }

    return false;
}

bool SubtitleService::startSubtitle(std::vector<int> fds, int trackId, SubtitleIOType type, ParserEventNotifier *notifier) {
    SUBTITLE_LOGI("%s  type:%d", __func__, type);
    std::unique_lock<std::mutex> autolock(mLock);
    if (mStarted) {
        SUBTITLE_LOGI("Already started, exit");
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
        SUBTITLE_LOGI("Error, %s data Source is null!", __func__);
        return false;
    }

    SUBTITLE_LOGI("%s  subType:%d", __func__,mSubParam.subType);
    mDataSource = datasource;
    if (TYPE_SUBTITLE_DVB == mSubParam.subType) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.dvbParam);
    } else if (TYPE_SUBTITLE_DVB_TELETEXT == mSubParam.subType) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.teletextParam);
    } else if (TYPE_SUBTITLE_DVB_TTML== mSubParam.subType) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.ttmlParam);
    } else if (TYPE_SUBTITLE_SCTE27== mSubParam.subType) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.scte27Param);
    } else if (TYPE_SUBTITLE_ARIB_B24== mSubParam.subType) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.arib24Param);
    } else if (TYPE_SUBTITLE_SMPTE_TTML== mSubParam.subType) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.smpteTtmlParam);
    }
    // schedule subtitle
    subtitle->scheduleStart();

    // schedule subtitle
    //subtitle.attach input source
    subtitle->attachDataSource(datasource, subtitle);

    // subtitle.attach display

    // schedule subtitle
    // subtitle->scheduleStart();
    //we only update params for DTV. because we do not wait dtv and
    // CC parser run the same time

    SUBTITLE_LOGI("setParameter on start: %d, dtvSubType=%d",
        mSubParam.isValidDtvParams(), mSubParam.dtvSubType);
    // TODO: revise,
    if (!hasExtSub && (mSubParam.isValidDtvParams() || mSubParam.dtvSubType <= 0)) {
        SUBTITLE_LOGI("setParameter on start");
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
        //return mSubtiles->resetForSeek();
    }

    return false;
}



int SubtitleService::updateVideoPosAt(int timeMills) {
    static int test = 0;
    if (test++ %100 == 0)
        SUBTITLE_LOGI("%s: %d(called %d times)", __func__, timeMills, test);

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
        case DTV_SUB_DTVKIT_DVB:
            mSubParam.dvbParam.demuxId = demuxId;
        break;
        case DTV_SUB_DTVKIT_TELETEXT:
            mSubParam.teletextParam.demuxId = demuxId;
        break;
        case DTV_SUB_DTVKIT_TTML:
            mSubParam.ttmlParam.demuxId = demuxId;
        break;
        case DTV_SUB_DTVKIT_SCTE27:
            mSubParam.scte27Param.demuxId = demuxId;
        break;
        case DTV_SUB_DTVKIT_ARIB24:
            mSubParam.arib24Param.demuxId = demuxId;
        break;
        case DTV_SUB_DTVKIT_SMPTE_TTML:
            mSubParam.smpteTtmlParam.demuxId = demuxId;
        break;
        default:
        break;
    }
    if (NULL == mDataSource )
      return;
    if (mSubParam.subType == TYPE_SUBTITLE_DVB)  {
        mDataSource->updateParameter(mSubParam.subType, &mSubParam.dvbParam);
    } else if (mSubParam.subType == TYPE_SUBTITLE_DVB_TELETEXT) {
        mDataSource->updateParameter(mSubParam.subType, &mSubParam.teletextParam);
    } else if (mSubParam.subType == TYPE_SUBTITLE_DVB_TTML) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.ttmlParam);
    } else if (mSubParam.subType == TYPE_SUBTITLE_SCTE27) {
        mDataSource->updateParameter(mSubParam.subType, &mSubParam.scte27Param);
    } else if (mSubParam.subType == TYPE_SUBTITLE_ARIB_B24) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.arib24Param);
    } else if (mSubParam.subType == TYPE_SUBTITLE_SMPTE_TTML) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.smpteTtmlParam);
    }
}

void SubtitleService::setSecureLevel(int flag) {
    switch (mSubParam.dtvSubType) {
        case DTV_SUB_DTVKIT_SCTE27:
            mSubParam.scte27Param.flag = flag;
        break;
        case DTV_SUB_DTVKIT_DVB:
            mSubParam.dvbParam.flag = flag;
        break;
        case DTV_SUB_DTVKIT_TELETEXT:
            mSubParam.teletextParam.flag = flag;
        break;
        case DTV_SUB_DTVKIT_ARIB24:
            mSubParam.arib24Param.flag = flag;
        break;
        case DTV_SUB_DTVKIT_TTML:
            mSubParam.ttmlParam.flag = flag;
        break;
        case DTV_SUB_DTVKIT_SMPTE_TTML:
            mSubParam.smpteTtmlParam.flag = flag;
        break;
        default:
        break;
    }
    if (NULL == mDataSource )
      return;
    if (mSubParam.subType == TYPE_SUBTITLE_DVB)  {
        mDataSource->updateParameter(mSubParam.subType, &mSubParam.dvbParam);
    } else if (mSubParam.subType == TYPE_SUBTITLE_DVB_TELETEXT) {
        mDataSource->updateParameter(mSubParam.subType, &mSubParam.teletextParam);
    } else if (mSubParam.subType == TYPE_SUBTITLE_DVB_TTML) {
        mDataSource->updateParameter(mSubParam.subType, &mSubParam.ttmlParam);
    } else if (mSubParam.subType == TYPE_SUBTITLE_SCTE27) {
        mDataSource->updateParameter(mSubParam.subType, &mSubParam.scte27Param);
    } else if (mSubParam.subType == TYPE_SUBTITLE_ARIB_B24) {
        mDataSource->updateParameter(mSubParam.subType, &mSubParam.arib24Param);
    } else if (mSubParam.subType == TYPE_SUBTITLE_SMPTE_TTML) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.smpteTtmlParam);
    }
}

void SubtitleService::setSubPid(int pid, int onid, int tsid) {
    switch (mSubParam.dtvSubType) {
        case DTV_SUB_DTVKIT_SCTE27:
            mSubParam.scte27Param.SCTE27_PID = pid;
        break;
        case DTV_SUB_DTVKIT_DVB:
            mSubParam.dvbParam.pid = pid;
        break;
        case DTV_SUB_DTVKIT_TELETEXT:
            mSubParam.teletextParam.pid = pid; // for demux

            // startSubtitle may use these.
            mSubParam.teletextParam.onid = onid;
            mSubParam.teletextParam.tsid = tsid;
        break;
        case DTV_SUB_DTVKIT_ARIB24:
            mSubParam.arib24Param.pid = pid;
        break;
        case DTV_SUB_DTVKIT_TTML:
            mSubParam.ttmlParam.pid = pid;
        break;
        case DTV_SUB_DTVKIT_SMPTE_TTML:
            mSubParam.smpteTtmlParam.pid = pid;
        break;
        default:
        break;
    }
    if (NULL == mDataSource )
      return;
    if (mSubParam.subType == TYPE_SUBTITLE_DVB)  {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.dvbParam);
    } else if (mSubParam.subType == TYPE_SUBTITLE_DVB_TELETEXT) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.teletextParam);
    } else if (mSubParam.subType == TYPE_SUBTITLE_DVB_TTML) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.ttmlParam);
    } else if (mSubParam.subType == TYPE_SUBTITLE_ARIB_B24) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.arib24Param);
    } else if (mSubParam.subType == TYPE_SUBTITLE_SMPTE_TTML) {
           mDataSource->updateParameter(mSubParam.subType, &mSubParam.smpteTtmlParam);
    }
}

void SubtitleService::setSubPageId(int pageId) {
    switch (mSubParam.dtvSubType) {
        case DTV_SUB_DTVKIT_DVB:
            mSubParam.dvbParam.compositionId = pageId;
            if (mSubtiles != nullptr) {
                mSubtiles->setParameter(&mSubParam);
            }
        break;
        case DTV_SUB_DTVKIT_TELETEXT:
            mSubParam.teletextParam.magazine = pageId;
        break;
        default:
        break;
    }
}
void SubtitleService::setSubAncPageId(int ancPageId) {
    switch (mSubParam.dtvSubType) {
        case DTV_SUB_DTVKIT_DVB:
            mSubParam.dvbParam.ancillaryId = ancPageId;
            if (mSubtiles != nullptr) {
                mSubtiles->setParameter(&mSubParam);
            }
        break;
        case DTV_SUB_DTVKIT_TELETEXT:
            mSubParam.teletextParam.pageNo = ancPageId;
        break;
        default:
        break;
    }
}

void SubtitleService::setChannelId(int channelId) {
    mSubParam.closedCaptionParam.ChannelID = channelId;
}


void SubtitleService::setClosedCaptionVfmt(int vfmt) {
     mSubParam.closedCaptionParam.vfmt = vfmt;
}

void SubtitleService::setClosedCaptionLang(const char *lang) {
    SUBTITLE_LOGI("lang=%s", lang);
    if (lang != nullptr && strlen(lang)<64) {
        strcpy(mSubParam.closedCaptionParam.lang, lang);
    }
}

/*
    mode: 1 for the player id setting;
             2 for the media id setting.
*/
void SubtitleService::setPipId(int mode, int id) {
    SUBTITLE_LOGI("setPipId mode = %d, id = %d\n", mode, id);
    bool same = true;
    if (PIP_PLAYER_ID== mode) {
        mSubParam.playerId = id;
        if (mUserDataAfd != nullptr) {
            mUserDataAfd->setPipId(mode, id);
        }
    } else if (PIP_MEDIASYNC_ID == mode) {
        if (mSubParam.mediaId == id) {
            same = true;
        } else {
            same = false;
        }
        mSubParam.mediaId = id;
        if (mUserDataAfd != nullptr) {
            mUserDataAfd->setPipId(mode, id);
        }
    }
    if (NULL == mDataSource )
        return;
    mDataSource->setPipId(mode, id);
   if ((mode == PIP_MEDIASYNC_ID) && (!same)) {
       mSubtiles->setParameter(&mSubParam);
   }
}

bool SubtitleService::ttControl(int cmd, int magazine, int pageNo, int regionId, int param) {
    // DO NOT update the entire parameter.
    SUBTITLE_LOGI("ttControl cmd:%d, magazine=%d, page:%d, regionId:%d",cmd, magazine, pageNo, regionId);

    switch (cmd) {
        case TT_EVENT_GO_TO_PAGE:
        case TT_EVENT_GO_TO_SUBTITLE:
            mSubParam.teletextParam.magazine = magazine;
            mSubParam.teletextParam.pageNo = pageNo;
            mSubParam.teletextParam.regionId = regionId;
            break;
        case TT_EVENT_SET_REGION_ID:
            mSubParam.teletextParam.regionId = regionId;
            break;
    }
    mSubParam.teletextParam.event = (TeletextEvent)cmd;
    mSubParam.subType = TYPE_SUBTITLE_DVB_TELETEXT;
    SUBTITLE_LOGI("ttControl event=%d subType=%d", mSubParam.teletextParam.event,mSubParam.subType);
    if (mSubtiles != nullptr) {
        return mSubtiles->setParameter(&mSubParam);
    }
    SUBTITLE_LOGI("%s mSubtiles null", __func__);

    return false;
}

int SubtitleService::ttGoHome() {
    SUBTITLE_LOGI("%s ", __func__);
    return mSubtiles->setParameter(&mSubParam);
}

int SubtitleService::ttGotoPage(int pageNo, int subPageNo) {
    SUBTITLE_LOGI("debug##%s ", __func__);
    mSubParam.teletextParam.pageNo = pageNo;
    mSubParam.teletextParam.subPageNo = subPageNo;
    mSubParam.subType = TYPE_SUBTITLE_DVB_TELETEXT;
    return mSubtiles->setParameter(&mSubParam);

}

int SubtitleService::ttNextPage(int dir) {
    SUBTITLE_LOGI("debug##%s ", __func__);
    mSubParam.subType = TYPE_SUBTITLE_DVB_TELETEXT;
    mSubParam.teletextParam.pageDir = dir;
    return mSubtiles->setParameter(&mSubParam);
}


int SubtitleService::ttNextSubPage(int dir) {
    mSubParam.subType = TYPE_SUBTITLE_DVB_TELETEXT;
    mSubParam.teletextParam.subPageDir = dir;
    return mSubtiles->setParameter(&mSubParam);
}

bool SubtitleService::userDataOpen(ParserEventNotifier *notifier) {
    SUBTITLE_LOGI("%s ", __func__);
    //default start userdata monitor afd
    if (mUserDataAfd == nullptr) {
        mUserDataAfd = std::shared_ptr<UserDataAfd>(new UserDataAfd());
        mUserDataAfd->start(notifier);
    }
    return true;
}

bool SubtitleService::userDataClose() {
    SUBTITLE_LOGI("%s ", __func__);
    if (mUserDataAfd != nullptr) {
        mUserDataAfd->stop();
        mUserDataAfd = nullptr;
    }

    return true;
}

bool SubtitleService::stopSubtitle() {
    SUBTITLE_LOGI("%s", __func__);
    std::unique_lock<std::mutex> autolock(mLock);
    if (!mStarted) {
        SUBTITLE_LOGI("Already stopped, exit");
        return false;
    } else {
        mStarted = false;
    }

    if (mSubtiles != nullptr) {
        mSubtiles->dettachDataSource(mSubtiles);
        mSubtiles = nullptr;
    }

    if (mFmqReceiver != nullptr && mDataSource != nullptr) {
        mFmqReceiver->unregisterClient(mDataSource);
        //mFmqReceiver = nullptr;
    }

    mDataSource = nullptr;
    mSubtiles = nullptr;
    mSubParam.subType = TYPE_SUBTITLE_INVALID;
    mSubParam.dtvSubType = DTV_SUB_INVALID;
    mSubParam.dvbParam.ancillaryId = 0;
    mSubParam.dvbParam.compositionId = 0;
    mSubParam.arib24Param.languageCodeId = 0;
    return true;
}

int SubtitleService::totalSubtitles() {
    if (mSubtiles == nullptr) {
        SUBTITLE_LOGE("Not ready or exited, ignore request!");
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

void SubtitleService::setLanguage(std::string lang) {
       if (mSubtiles != nullptr) {
        mSubtiles->onGetLanguage(lang);
    }
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
