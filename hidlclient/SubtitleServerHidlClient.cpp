/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *  @author   Amlogic
 *  @version  1.0
 *  @date     2019/08/15
 *  @par function description:
 *  - 1 subtitle server hwbinder client for CTC
 */
#define LOG_TAG "SubtitleServerHidlClient"

#include "SubtitleServerHidlClient.h"
//#include <binder/Binder.h>
#include <utils/Atomic.h>
#include "SubtitleLog.h"
#include <utils/CallStack.h>

#include <cutils/properties.h>

//#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/String16.h>
#include <unistd.h>

namespace android {
static Mutex amgLock;
int mTotal = -1;
int mRetry = RETRY_MAX;
pthread_t mSubtitleThread;
bool mThreadStop;
int mpos = 0;
static sp<ISubtitleServer> service;

static const int IOTYPE_AMSTREAM = 1;

sp<ISubtitleServer> SubtitleServerHidlClient::getSubtitleService() {
    SUBTITLE_LOGE("SubtitleServerHidlClient getSubtitleService ...");
    Mutex::Autolock _l(amgLock);
    if (service == nullptr) {
        service = ISubtitleServer::tryGetService();
        while (service == nullptr) {
            amgLock.unlock();
            usleep(200*1000);//sleep 200ms
            amgLock.lock();
            service = ISubtitleServer::tryGetService();
            SUBTITLE_LOGE("tryGet ISubtitleServer daemon Service");
        };
    }
    return service;
}

SubtitleServerHidlClient::SubtitleServerHidlClient()
{
    SUBTITLE_LOGE("SubtitleServerHidlClient creat ...");

    char value[PROPERTY_VALUE_MAX] = {0};
    mDisabled = false;
    if (property_get("sys.ctcsubtitle.disabled", value, "false") > 0) {
        if (!strcmp(value, "true")) {
            mDisabled = true;
        }
    }

    // TODO: how to get and share the sessionId with playerUI api??
    mSessionId = -1;
    // TODO: ?? multi-instance??

    getSubtitleService();
}

SubtitleServerHidlClient::~SubtitleServerHidlClient()
{
    SUBTITLE_LOGI("%s", __func__);
}

int SubtitleServerHidlClient::subtitleGetTypeDetail()
{
    if (mDisabled) return 0;
    int ret = 0;
    //const sp<ISubtitleServer>& subser = getSubtitleService();
    //if (subser != 0) {
        //ret = subser->getTypeDetail();
    //}
    CallStack stk(LOG_TAG);
    SUBTITLE_LOGE("Error, should not use!");
    return ret;
}

void SubtitleServerHidlClient::subtitleShowSub(int pos)
{
    if (mDisabled) return;
    /*const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        SUBTITLE_LOGE("subtitleShowSub pos:%d\n", pos);
     if (pos > 0) {
            subser->showSub(pos);
     }
    }
    return;*/
    CallStack stk(LOG_TAG);
    SUBTITLE_LOGE("Error, should not use!");

}

void *SubtitleServerHidlClient::thrSubtitleShow(void *pthis)
{
/*    int pos = 0;
    SubtitleServerHidlClient *ssh = (SubtitleServerHidlClient *) pthis;
    //SUBTITLE_LOGE("thrSubtitleShow pos:%d\n",pos);

    do {
        SUBTITLE_LOGE("1thrSubtitleShow pos:%d\n", (ssh->gsubtitleCtx.ptscb)());
        ssh->subtitleShowSub((ssh->gsubtitleCtx.ptscb)());
        usleep((300 - (pos % 300)) * 1000);
    }
    while (!mThreadStop);
*/
    CallStack stk(LOG_TAG);
    SUBTITLE_LOGE("Error, should not use!");
    return NULL;
}

void SubtitleServerHidlClient::subtitleShow()
{
    CallStack stk(LOG_TAG);
    SUBTITLE_LOGE("Error, should not use!");
//    pthread_create(&mSubtitleThread, NULL, thrSubtitleShow, (void *)this);
}

void SubtitleServerHidlClient::switchSubtitle(SUB_Para_t *para)
{
    if (mDisabled) return;

    SUBTITLE_LOGE("switchSubtitle tvType:%d\n", para->tvType);
    const sp<ISubtitleServer>& subser = getSubtitleService();
    subser->close(mSessionId);
    initSubtitle(para);
    subser->open(mSessionId, nullptr, IOTYPE_AMSTREAM, OpenType::TYPE_MIDDLEWARE);
}

void SubtitleServerHidlClient::initSubtitle(SUB_Para_t *para)
{
    if (mDisabled) return;
    SUBTITLE_LOGE("initSubtitle tvType:%d\n", para->tvType);
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        subser->setSubType(mSessionId, para->tvType);
        if ((para->tvType == CODEC_ID_SCTE27) ||(para->tvType == CODEC_ID_DEMUX_SCTE27)) {
            subser->setSubPid(mSessionId, para->pid, -1, -1);
               subser->setChannelId(mSessionId, 15);
        } else if ((para->tvType == CODEC_ID_DVB)||(para->tvType == CODEC_ID_DEMUX_DVB)) {
               subser->setSubPid(mSessionId, para->pid, -1, -1);
               subser->setPageId(mSessionId, para->pageId);
               subser->setAncPageId(mSessionId, para->ancPageId);
       } else if(para->tvType == CODEC_ID_DEMUX_TELETEXT) {
            subser->setSubPid(mSessionId, para->pid, -1, -1);
       } else if (para->vfmt == CODEC_ID_CLOSEDCAPTION_MPEG) {
              subser->setClosedCaptionVfmt(mSessionId, CODEC_ID_CLOSEDCAPTION_MPEG);
              subser->setChannelId(mSessionId, para->channelId);
       } else if (para->vfmt == CODEC_ID_CLOSEDCAPTION_H264) {
              subser->setClosedCaptionVfmt(mSessionId, CODEC_ID_CLOSEDCAPTION_H264);
              subser->setChannelId(mSessionId, para->channelId);
       } else if (para->vfmt == CODEC_ID_CLOSEDCAPTION_H265) {
              subser->setClosedCaptionVfmt(mSessionId, CODEC_ID_CLOSEDCAPTION_H265);
              subser->setChannelId(mSessionId, para->channelId);
       }
     }
}

void SubtitleServerHidlClient::subtitleOpen(const std::string& path, getPtsCb cb, SUB_Para_t * para)
{
    if (mDisabled) return;
    SUBTITLE_LOGI("%s", __func__);
    //gsubtitleCtx = new subtitlectx();
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        initSubtitle(para);
        subser->open(mSessionId, nullptr, IOTYPE_AMSTREAM, OpenType::TYPE_MIDDLEWARE); // CTC here, always use device IO(1).
        mRetry = RETRY_MAX;
        mThreadStop = false;
    }
   gsubtitleCtx.ptscb = cb;
    //ctc_mp = static_cast<CTsPlayer *>(pthis);
    SUBTITLE_LOGE("subtitleOpen pos2:%d\n", gsubtitleCtx.ptscb());
    return;
}

void SubtitleServerHidlClient::subtitleOpenIdx(int idx)
{
    if (mDisabled) return;
    CallStack stk(LOG_TAG);
    SUBTITLE_LOGE("Error, should not use!");
}

void SubtitleServerHidlClient::subtitleClose()
{
    if (mDisabled) return;
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        SUBTITLE_LOGI("%s", __func__);
        subser->close(mSessionId);
    }
    mThreadStop = true;
    pthread_join(mSubtitleThread, NULL);
    return;
}

int SubtitleServerHidlClient::subtitleGetTotal()
{
    if (mDisabled) return 0;
    int count = -1;
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        auto r = subser->getTotalTracks(mSessionId, [&] (const Result &ret, const int& v) {
            if (ret == Result::OK) {
                count = v;
            }
        });
        if (!r.isOk()) return -1;
    }
    return count;
}

void SubtitleServerHidlClient::subtitleNext()
{
    CallStack stk(LOG_TAG);
    SUBTITLE_LOGE("Error, should not use!");
}

void SubtitleServerHidlClient::subtitlePrevious()
{
    CallStack stk(LOG_TAG);
    SUBTITLE_LOGE("Error, should not use!");
}

void SubtitleServerHidlClient::subtitleOption()
{
    CallStack stk(LOG_TAG);
    SUBTITLE_LOGE("Error, should not use!");
}

int SubtitleServerHidlClient::subtitleGetType()
{
    int type = 0;
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        auto r = subser->getType(mSessionId, [&] (const Result &ret, const int& v) {
            if (ret == Result::OK) {
                type = v;
            }
        });
        if (!r.isOk()) return 0;
    }
    return type;
}

std::string SubtitleServerHidlClient::subtitleGetTypeStr()
{
    std::string ret;
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        //@@ret = String8(subser->getTypeStr()).string();
    }
    return ret;
}

void SubtitleServerHidlClient::subtitleSetTextColor(int color)
{
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        subser->setTextColor(mSessionId, color);
    }
    return;
}

void SubtitleServerHidlClient::subtitleSetTextSize(int size)
{
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        subser->setTextSize(mSessionId, size);
    }
    return;
}

void SubtitleServerHidlClient::subtitleSetGravity(int gravity)
{
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        subser->setGravity(mSessionId, gravity);
    }
    return;
}

void SubtitleServerHidlClient::subtitleSetTextStyle(int style)
{
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        subser->setTextStyle(mSessionId, style);
    }
    return;
}

void SubtitleServerHidlClient::subtitleSetPosHeight(int height)
{
    SUBTITLE_LOGI("%s", __func__);
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        subser->setPosHeight(mSessionId, height);
    }
    return;
}

void SubtitleServerHidlClient::subtitleSetImgRatio(float ratioW, float ratioH, int maxW, int maxH)
{
    SUBTITLE_LOGI("%s", __func__);
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        subser->setImgRatio(mSessionId, ratioW, ratioH, maxW, maxH);
    }
    return;
}

void SubtitleServerHidlClient::subtitleSetSurfaceViewParam(int x, int y, int w, int h)
{
    const sp<ISubtitleServer>& subser = getSubtitleService();
    SUBTITLE_LOGE("subtitleSetSurfaceViewParam 00\n");
    if (subser != 0) {
        SUBTITLE_LOGE("subtitleSetSurfaceViewParam 01\n");
        subser->setSurfaceViewRect(mSessionId, x, y, w, h);
    }
    return;
}

void SubtitleServerHidlClient::subtitleClear()
{
    CallStack stk(LOG_TAG);
    SUBTITLE_LOGE("Error, should not use!");
}

void SubtitleServerHidlClient::subtitleResetForSeek()
{
    SUBTITLE_LOGI("%s", __func__);
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        subser->resetForSeek(mSessionId);
    }
    return;
}

void SubtitleServerHidlClient::subtitleHide()
{
    SUBTITLE_LOGI("%s", __func__);
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        subser->hide(mSessionId);
    }
    return;
}

void SubtitleServerHidlClient::subtitleDisplay()
{
    SUBTITLE_LOGI("%s", __func__);
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        subser->show(mSessionId);
    }
    return;
}

std::string SubtitleServerHidlClient::subtitleGetCurName()
{
    CallStack stk(LOG_TAG);
    SUBTITLE_LOGE("Error, should not use!");
    std::string curName;
    return curName;
}

std::string SubtitleServerHidlClient::subtitleGetName(int idx)
{
    CallStack stk(LOG_TAG);
    SUBTITLE_LOGE("Error, should not use!");
    std::string name;
    return name;
}

std::string SubtitleServerHidlClient::subtitleGetLanguage(int idx)
{
    SUBTITLE_LOGE("[subtitleGetLanguage]");
    const sp<ISubtitleServer>& subser = getSubtitleService();
    std::string language;
    if (subser != 0) {
        subser->getLanguage(mSessionId, [&](const Result &ret, const hidl_string& subLanguage){
            if (ret == Result::OK) {
                language = subLanguage;
            }
        });
    }
    //SUBTITLE_LOGE("[subtitleGetLanguage]language:%s", language.c_str());
    return language;
}

void SubtitleServerHidlClient::subtitleLoad(const std::string& path)
{
    CallStack stk(LOG_TAG);
    SUBTITLE_LOGE("Error, should not use!");
}

void SubtitleServerHidlClient::subtitleCreat()
{
    SUBTITLE_LOGE("subtitleCreate: \n\n\n\n\n\n Not Impl\nNeed impl for get sessionId\n\n\n\n");
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        //subser->creat();

        //TODO here, need impl how to get session ID.
    }
    //usleep(500*1000);
    return;
}

void SubtitleServerHidlClient::subtitleSetSubPid(int32_t pid, int32_t onid, int32_t tsid)
{
    SUBTITLE_LOGI("%s", __func__);
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        subser->setSubPid(mSessionId, pid, onid, tsid);
    }
    return;
}

int SubtitleServerHidlClient::subtitleGetSubHeight()
{
    CallStack stk(LOG_TAG);
    SUBTITLE_LOGE("Error, should not use!");
    return -1;
}

int SubtitleServerHidlClient::subtitleGetSubWidth()
{
    CallStack stk(LOG_TAG);
    SUBTITLE_LOGE("Error, should not use!");
    return -1;
}

void SubtitleServerHidlClient::subtitleSetSubType(int type)
{
    SUBTITLE_LOGI("%s", __func__);
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        subser->setSubType(mSessionId, type);
    }
    return;
}

void SubtitleServerHidlClient::subtitleDestroy()
{
    SUBTITLE_LOGE("subtitleDestroy: need impl according to subtitleCreate");
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        //subser->destroy();
    }
    return;
}

int SubtitleServerHidlClient::ttGoHome()
{
    SUBTITLE_LOGE("ttGoHome");
    int ret = -1;
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        auto r = subser->ttGoHome(mSessionId);
        if (r.isOk()) return 0;
    }
    return ret;
}

int SubtitleServerHidlClient::ttNextPage(int dir)
{
    SUBTITLE_LOGE("ttNextPage");
    int ret = -1;
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        auto r = subser->ttNextPage(mSessionId, dir);
        if (r.isOk()) return 0;
    }
    return ret;
}

int SubtitleServerHidlClient::ttNextSubPage(int dir)
{
    SUBTITLE_LOGE("ttNextSubPage");
    int ret = -1;
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        auto r = subser->ttNextSubPage(mSessionId, dir);
        if (r.isOk()) return 0;
    }
    return ret;
}

int SubtitleServerHidlClient::ttGotoPage(int pageNo, int subPageNo)
{
    SUBTITLE_LOGE("ttGotoPage");
    int ret = -1;
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (subser != 0) {
        auto r = subser->ttGotoPage(mSessionId, pageNo, subPageNo);
        if (r.isOk()) return 0;
    }
    return 1;
}



void SubtitleServerHidlClient::setListener(const sp<SubtitleMiddleListener> &listener)
{
    if (mDisabled) return;
    SUBTITLE_LOGI("SubtitleServerHidlClient setListener");
    mListener = listener;
    const sp<ISubtitleServer>& subser = getSubtitleService();
    if (listener != NULL) {
        if (mSubtitleServerHidlCallback == NULL) {
            mSubtitleServerHidlCallback = new SubtitleServerHidlCallback(this);
            SUBTITLE_LOGI("SubtitleServerHidlClient setCallback");
            subser->setCallback(mSubtitleServerHidlCallback, static_cast<ConnectType>(1)/*ConnectType:TYPE_EXTEND*/);
        }
    }
}

Return<void> SubtitleServerHidlClient::SubtitleServerHidlCallback::eventNotify(const SubtitleHidlParcel &hidlParcel) {
    SUBTITLE_LOGI("notifyCallback event type:%d", hidlParcel.msgType);

    if (hidlParcel.msgType == EVENT_ON_SUBTITLEDATA_CALLBACK) {
         SUBTITLE_LOGI("notifyCallback notify parcel.msgType = %d, event:%d, id:%d", hidlParcel.msgType,hidlParcel.bodyInt[0], hidlParcel.bodyInt[1]);
    } else if (hidlParcel.msgType == EVENT_ON_SUBTITLEAVAILABLE_CALLBACK) {
         SUBTITLE_LOGI("notifyCallback notify parcel.msgType = %d, available:%d", hidlParcel.msgType, hidlParcel.bodyInt[0]);
    } else {
         return Void();
    }

    if (subtitleserverClient->mListener == NULL) {
        SUBTITLE_LOGI("listener is  null");
        return Void();
    } else {
        SUBTITLE_LOGI("listener not  null");
    }

    sp<SubtitleMiddleListener> listener;
    {
        Mutex::Autolock _l(amgLock);
        listener = subtitleserverClient->mListener;
    }

    subtitle_parcel_t parcel;
    parcel.msgType = hidlParcel.msgType;
    for (int i = 0; i < hidlParcel.bodyInt.size(); i++) {
        parcel.bodyInt.push_back(hidlParcel.bodyInt[i]);
    }

    for (int j = 0; j < hidlParcel.bodyString.size(); j++) {
        parcel.bodyString.push_back(hidlParcel.bodyString[j]);
    }

    if (listener != NULL) {
        listener->notify(parcel);
    }
    return Void();
}
/*void SubtitleServerHidlClient::SubtitleServerDaemonDeathRecipient::serviceDied(uint64_t cookie __unused,
        const ::android::wp<::android::hidl::base::V1_0::IBase>& who __unused)
{
    SUBTITLE_LOGE("tvserver daemon died.");
    Mutex::Autolock _l(amgLock);

    //usleep(200*1000);//sleep 200ms
    //subtitleserverClient->reconnect();
}*/
}

