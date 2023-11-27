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

#define LOG_TAG "UserDataAfd"
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include "SubtitleLog.h"

#include <UserDataAfd.h>
#include "VideoInfo.h"


UserDataAfd *UserDataAfd::sInstance = nullptr;
int UserDataAfd::sNewAfdValue = -1;
void UserDataAfd::notifyCallerAfdChange(int afd) {
    //SUBTITLE_LOGI("afd_evt_callback, afdValue:%x", afd);
    if (mNotifier != nullptr) {
        mNotifier->onVideoAfdChange((afd&0x7), mPlayerId);
    }
}

void UserDataAfd:: setPipId(int mode, int id) {
    SUBTITLE_LOGI("setPipId mode = %d, id = %d\n", mode, id);

    if (sInstance == nullptr) {
       SUBTITLE_LOGI("Error: setPlayerId sInstance is null");
       return;
    }

    if (PIP_PLAYER_ID== mode) {
        if (id == mPlayerId) {
            return;
        }
        if (-1 == id) return;

        mPlayerId = id;
        stop();
        start(mNotifier);
    } else if (PIP_MEDIASYNC_ID == mode) {
        if (id >= 0 && id != mMediasyncId) {
            mMediasyncId = id;

            AM_USERDATA_SetParameters(USERDATA_DEVICE_NUM, id);
        }
    }

}

UserDataAfd *UserDataAfd::getCurrentInstance() {
    return UserDataAfd::sInstance;
}


//afd callback
void afd_evt_callback(long devno, int eventType, void *param, void *userdata) {
    (void)devno;
    (void)eventType;
    (void)userdata;
    int afdValue;
    AM_USERDATA_AFD_t *afd = (AM_USERDATA_AFD_t *)param;
    afdValue = afd->af;
    UserDataAfd *instance = UserDataAfd::getCurrentInstance();
    if (instance != nullptr && afdValue != UserDataAfd::sNewAfdValue) {
        instance->notifyCallerAfdChange(afdValue);
        UserDataAfd::sNewAfdValue = afdValue;
        SUBTITLE_LOGI("AFD callback, value:0x%x", UserDataAfd::sNewAfdValue);
    }
}

UserDataAfd::UserDataAfd() {
    mNotifier = nullptr;
    mPlayerId = -1;
    mMediasyncId = -1;
    mMode = -1;
    SUBTITLE_LOGI("creat UserDataAfd");
    sInstance = this;
    mThread = nullptr;

}

UserDataAfd::~UserDataAfd() {
    SUBTITLE_LOGI("~UserDataAfd");
    sInstance = nullptr;
    mPlayerId = -1;
    stop();

}
int UserDataAfd::start(ParserEventNotifier *notify)
{
    SUBTITLE_LOGI("startUserData mPlayerId = %d", mPlayerId);

    // TODO: should impl a real status/notify manner
    std::unique_lock<std::mutex> autolock(mMutex);
    mNotifier = notify;
    if (-1 == mPlayerId) {
        return 1;
    }
    //get Video Format need some time, will block main thread, so start a thread to open userdata.
    mThread = std::shared_ptr<std::thread>(new std::thread(&UserDataAfd::run, this));
    return 1;
}

void UserDataAfd::run() {
    //int mode;
    AM_USERDATA_OpenPara_t para;
    memset(&para, 0, sizeof(para));
    para.vfmt = VideoInfo::Instance()->getVideoFormat();

    if (mPlayerId != -1) {
        para.playerid = mPlayerId;
    }
    para.mediasyncid = mMediasyncId;
    UserDataAfd::sNewAfdValue = -1;
    if (AM_USERDATA_Open(USERDATA_DEVICE_NUM, &para) != AM_SUCCESS) {
         SUBTITLE_LOGI("Cannot open userdata device %d", USERDATA_DEVICE_NUM);
         return;
    }

    //add notify afd change
    SUBTITLE_LOGI("start afd running mPlayerId = %d",mPlayerId);
    AM_USERDATA_GetMode(USERDATA_DEVICE_NUM, &mMode);
    AM_USERDATA_SetMode(USERDATA_DEVICE_NUM, mMode | AM_USERDATA_MODE_AFD);
    AM_EVT_Subscribe(USERDATA_DEVICE_NUM, AM_USERDATA_EVT_AFD, afd_evt_callback, NULL);
}


int UserDataAfd::stop() {
    SUBTITLE_LOGI("stopUserData");
    // TODO: should impl a real status/notify manner
    // this is too simple...
    std::unique_lock<std::mutex> autolock(mMutex);
    if (mThread != nullptr) {
        mThread->join();
        mThread = nullptr;
    } else {
        return -1;
    }
    AM_EVT_Unsubscribe(USERDATA_DEVICE_NUM, AM_USERDATA_EVT_AFD, afd_evt_callback, NULL);
    if ((mMode & AM_USERDATA_MODE_CC) ==  AM_USERDATA_MODE_CC)
        AM_USERDATA_Close(USERDATA_DEVICE_NUM);
    return 0;
}

