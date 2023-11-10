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

#define LOG_TAG "ClosedCaptionParser"
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <list>
#include <thread>
#include <algorithm>
#include <functional>
#include <stack>
#include <mutex>
#include <future>

#include "SubtitleLog.h"
#include <utils/CallStack.h>

#include "StreamUtils.h"
#include "VideoInfo.h"
#include "SubtitleTypes.h"
#include "ParserFactory.h"

#include "ClosedCaptionParser.h"

#define CC_EVENT_VCHIP_AUTH -1
#define CC_EVENT_VCHIP_FLAG -2
#define CC_EVENT_CHANNEL_ADD 1
#define CC_EVENT_CHANNEL_REMOVE 0

// we want access Parser in little-dvb callback.
// must use a global lock
static std::mutex gLock;


static void cc_report_cb(ClosedCaptionHandleType handle, int error) {
    (void)handle;
    ClosedCaptionParser *parser = ClosedCaptionParser::getCurrentInstance();
    if (parser != nullptr) {
        parser->notifyAvailable(__notifierErrorRemap(error));
    }
}

struct ChannelHandle {
    int sendNoSig(int timeout) {
        while (!mReqStop && timeout-->0) {
            usleep(100);
        }
        {   // here, send notify
            std::unique_lock<std::mutex> autolock(gLock);
            ClosedCaptionParser *parser = ClosedCaptionParser::getCurrentInstance();
            if (parser != nullptr) {
                parser->notifyChannelState(0, 0);
            }
        }
        return 0;
    }
    bool mReqStop;
    int mBackupMask;
};

ChannelHandle gHandle = {true, 0};
std::thread gTimeoutThread;

static void cc_rating_cb (ClosedCaptionHandleType handle, vbi_rating *rating) {
    (void)handle;
    std::unique_lock<std::mutex> autolock(gLock);
    ClosedCaptionParser *parser = ClosedCaptionParser::getCurrentInstance();
    if (parser != nullptr) {
        int data = 0;
        data = rating->auth<<16|rating->id<<8| rating->dlsv;
        SUBTITLE_LOGI("CC_RATING_CB: auth: %d id:%d dlsv:%d", rating->auth, rating->id, rating->dlsv);
        parser->notifyChannelState(CC_EVENT_VCHIP_AUTH, data);
    }
}

static void q_tone_data_cb(ClosedCaptionHandleType handle, char *buffer, int size) {
    SUBTITLE_LOGI("q_tone_data_cb data:%s, size:%d", buffer ,size);
    std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());
    spu->spu_data = (unsigned char *)malloc(size);
    memset(spu->spu_data, 0, size);
    memcpy(spu->spu_data, buffer, size);
    spu->buffer_size = size;
    spu->isQtoneData = true;
    spu->subtitle_type = TYPE_SUBTITLE_CLOSED_CAPTION;

    // report data:
    {
        std::unique_lock<std::mutex> autolock(gLock);
        ClosedCaptionParser *parser = ClosedCaptionParser::getCurrentInstance();
        if (parser != nullptr) {
            // CC and scte use little dvb, render&presentation already handled.
            spu->isImmediatePresent = true;
            parser->addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
        } else {
            SUBTITLE_LOGI("Report json string to a deleted cc parser!");
        }
    }


}

static void cc_data_cb(ClosedCaptionHandleType handle, int mask) {
    (void)handle;
    if (mask > 0) {
        gHandle.mReqStop = true; // todo: CV notify
        ClosedCaptionParser *parser = ClosedCaptionParser::getCurrentInstance();
        if (parser != nullptr) {
            parser->notifyChannelState(CC_EVENT_VCHIP_FLAG, mask);
        }
        SUBTITLE_LOGI("CC_DATA_CB: mask: %d lastMask:%d", mask, gHandle.mBackupMask);
        for (int i = 0; i<15; i++) {
            unsigned int curr = (mask >> i) & 0x1;
            unsigned int last = (gHandle.mBackupMask >> i) & 0x1;
            SUBTITLE_LOGI("CC_DATA_CB: curr: %d last:%d,index:%d", curr, last,i);

            if (curr != last) {
                std::unique_lock<std::mutex> autolock(gLock);
                if (parser != nullptr) {
                    parser->notifyChannelState((curr == 1) ? CC_EVENT_CHANNEL_ADD:CC_EVENT_CHANNEL_REMOVE, i);
                }
            }
        }
        gHandle.mBackupMask = mask;
    } else {
        gHandle.mReqStop = false;
        gTimeoutThread = std::thread(&ChannelHandle::sendNoSig, gHandle, 50000);
        gTimeoutThread.detach();
    }
}



void json_update_cb(ClosedCaptionHandleType handle) {
    (void)handle;

    SUBTITLE_LOGI("@@@@@@ cc json string: %s", ClosedCaptionParser::sJsonStr);
    int mJsonLen = strlen(ClosedCaptionParser::sJsonStr);
    std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());
    spu->spu_data = (unsigned char *)malloc(mJsonLen);
    memset(spu->spu_data, 0, mJsonLen);
    memcpy(spu->spu_data, ClosedCaptionParser::sJsonStr, mJsonLen);
    spu->buffer_size = mJsonLen;
    spu->subtitle_type = TYPE_SUBTITLE_CLOSED_CAPTION;

    // report data:
    {
        std::unique_lock<std::mutex> autolock(gLock);
        ClosedCaptionParser *parser = ClosedCaptionParser::getCurrentInstance();
        if (parser != nullptr) {
            // CC and scte use little dvb, render&presentation already handled.
            spu->isImmediatePresent = true;
            parser->addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
        } else {
            SUBTITLE_LOGI("Report json string to a deleted cc parser!");
        }
    }
    //saveJsonStr(ClosedCaptionParser::gJsonStr);
}

void ClosedCaptionParser::notifyAvailable(int avil) {
    if (mNotifier != nullptr) {
        mNotifier->onSubtitleAvailable(avil);
    }
}

void ClosedCaptionParser::notifyChannelState(int stat, int channelId) {
    if (mNotifier != nullptr) {
        SUBTITLE_LOGI("CC_DATA_CB: %d %d", stat, channelId);
        mNotifier->onSubtitleDataEvent(stat, channelId);
    }
}


ClosedCaptionParser *ClosedCaptionParser::sInstance = nullptr;

ClosedCaptionParser *ClosedCaptionParser::getCurrentInstance() {
    return ClosedCaptionParser::sInstance;
}

ClosedCaptionParser::ClosedCaptionParser(std::shared_ptr<DataSource> source) {
    SUBTITLE_LOGI("creat ClosedCaption parser");
    mDataSource = source;
    mParseType = TYPE_SUBTITLE_CLOSED_CAPTION;

    std::unique_lock<std::mutex> autolock(gLock);
    sInstance = this;
    mCcContext = new TVSubtitleData();
}

ClosedCaptionParser::~ClosedCaptionParser() {
    SUBTITLE_LOGI("%s", __func__);
    {
        std::unique_lock<std::mutex> autolock(gLock);
        sInstance = nullptr;
    }
    stopAmlCC();
    // call back may call parser, parser later destroy
    stopParser();
    if (mLang) free(mLang);
    delete mCcContext;
}

bool ClosedCaptionParser::updateParameter(int type, void *data) {
    (void)type;
  ClosedCaptionParam *cc_param = (ClosedCaptionParam *) data;
  mChannelId = cc_param->ChannelID;
  if (strlen(cc_param->lang) > 0) {
      mLang = strdup(cc_param->lang);
  }
  //mVfmt = cc_param->vfmt;
  SUBTITLE_LOGI("@@@@@@ updateParameter mChannelId: %d, mVfmt:%d", mChannelId, mVfmt);
  return true;
}

void ClosedCaptionParser::setPipId (int mode, int id) {
    if (PIP_PLAYER_ID == mode) {
        if (id == mPlayerId)
            return;
        mPlayerId = id;
    } else if (PIP_MEDIASYNC_ID == mode) {
        mMediaSyncId = id;
    }
}

int ClosedCaptionParser::startAtscCc(int source, int vfmt, int caption, int fg_color,
        int fg_opacity, int bg_color, int bg_opacity, int font_style, int font_size)
{
    ClosedCaptionCreatePara_t cc_para;
    ClosedCaptionStartPara_t spara;
    int ret;
    int mode;
    SUBTITLE_LOGI("start cc: vfmt %d caption %d, fgc %d, bgc %d, fgo %d, bgo %d, fsize %d, fstyle %d mMediaSyncId:%d",
            vfmt, caption, fg_color, bg_color, fg_opacity, bg_opacity, font_size, font_style, mMediaSyncId);

    memset(&cc_para, 0, sizeof(cc_para));
    memset(&spara, 0, sizeof(spara));

    cc_para.bmp_buffer = mCcContext->buffer;
    cc_para.pitch = mCcContext->bmp_pitch;
    //cc_para.draw_begin = cc_draw_begin_cb;
    //cc_para.draw_end = cc_draw_end_cb;
    cc_para.user_data = (void *)mCcContext;
    cc_para.input = (ClosedCaptionInput)source;
    //cc_para.rating_cb = cc_rating_cb;
    cc_para.data_cb = cc_data_cb;
    cc_para.report = cc_report_cb;
    cc_para.rating_cb = cc_rating_cb;
    cc_para.data_timeout = 5000;//5s
    cc_para.switch_timeout = 3000;//3s
    cc_para.json_update = json_update_cb;
    cc_para.json_buffer = sJsonStr;
#ifdef SUPPORT_KOREA
    cc_para.q_tone_cb = q_tone_data_cb;
    if (mLang != nullptr) {
        strncpy(cc_para.lang, mLang, sizeof(cc_para.lang));
        cc_para.lang[ sizeof(cc_para.lang) -1 ] = '\0';
    }
#endif

    spara.vfmt = vfmt;
    spara.player_id = mPlayerId;
    spara.mediaysnc_id = mMediaSyncId;
    spara.caption1                 = (ClosedCaptionMode)caption;
    spara.caption2                 = CLOSED_CAPTION_NONE;
    spara.user_options.bg_color    = (ClosedCaptionColor)bg_color;
    spara.user_options.fg_color    = (ClosedCaptionColor)fg_color;
    spara.user_options.bg_opacity  = (ClosedCaptionOpacity)bg_opacity;
    spara.user_options.fg_opacity  = (ClosedCaptionOpacity)fg_opacity;
    spara.user_options.font_size   = (ClosedCaptionFontSize)font_size;
    spara.user_options.font_style  = (ClosedCaptionFontStyle)font_style;

    SUBTITLE_LOGI("%s %s", mLang, cc_para.lang);
    ret = ClosedCaptionCreate(&cc_para, &mCcContext->cc_handle);
    if (ret != AM_SUCCESS) {
        goto error;
    }

    ret = ClosedCaptionStart(mCcContext->cc_handle, &spara);
    if (ret != AM_SUCCESS) {
        goto error;
    }
    SUBTITLE_LOGI("start cc successfully!");
    return 0;
error:
    if (mCcContext->cc_handle != NULL) {
        ClosedCaptionDestroy(mCcContext->cc_handle);
    }
    SUBTITLE_LOGI("start cc failed!");
    return -1;
}

int ClosedCaptionParser::stopAmlCC() {
    SUBTITLE_LOGI("stop cc");
    ClosedCaptionDestroy(mCcContext->cc_handle);
    pthread_mutex_lock(&mCcContext->lock);
    pthread_mutex_unlock(&mCcContext->lock);

    mCcContext->cc_handle = NULL;
    gHandle = {true, 0};
    return 0;
}



int ClosedCaptionParser::startAmlCC() {
    int source = mChannelId>>8;
    int channel = mChannelId&0xff;
    if (source != CLOSED_CAPTION_INPUT_VBI) {
        mVfmt = VideoInfo::Instance()->getVideoFormat();
    }

    if (sInstance == nullptr) {
        sInstance = this;
    }

    SUBTITLE_LOGI(" start cc source:%d, channel:%d, mvfmt:%d", source, channel, mVfmt);
    startAtscCc(source, mVfmt, channel, 0, 0, 0, 0, 0, 0);

    return 0;
}


int ClosedCaptionParser::parse() {
    // CC use little dvb, no need common parser here
    if (!mThreadExitRequested) {
        if (mState == SUB_INIT) {
            /*this have multi thread issue.
            startAmlCC have delay, if this mState after startAmlCC,
            then resumeplay will cause stopParser first, mState will SUB_STOP to
            SUB_PLAYING, which cause while circle, and cc thread will not release*/
            mState = SUB_PLAYING;

            startAmlCC();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return 0;
    }
    return 0;
}

void ClosedCaptionParser::dump(int fd, const char *prefix) {
    dprintf(fd, "%s Closed Caption Parser\n", prefix);
    dumpCommon(fd, prefix);
    dprintf(fd, "\n");
    dprintf(fd, "%s   Channel Id: %d\n", prefix, mChannelId);
    dprintf(fd, "%s   Device  No: %d\n", prefix, mDevNo);
    dprintf(fd, "%s   Video Format: %d\n", prefix, mVfmt);
    dprintf(fd, "\n");
    if (mCcContext != nullptr) {
        mCcContext->dump(fd, prefix);
    }
}



