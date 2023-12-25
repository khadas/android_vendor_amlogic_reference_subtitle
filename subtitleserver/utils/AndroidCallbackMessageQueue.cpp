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

#define LOG_TAG "SubtitleServer"
#include <thread>
#include "AndroidCallbackMessageQueue.h"
#include "SubtitleLog.h"

#define EVENT_ON_SUBTITLEDATA_CALLBACK       0xA00000
#define EVENT_ON_SUBTITLEAVAILABLE_CALLBACK  0xA00001
#define EVENT_ON_VIDEOAFDCHANGE_CALLBACK     0xA00002
#define EVENT_ON_MIXVIDEOEVENT_CALLBACK      0xA00003
#define EVENT_ON_SUBTITLE_DIMENSION_CALLBACK  0xA00004
#define EVENT_ON_SUBTITLE_LANGUAGE_CALLBACK  0xA00005
#define EVENT_ON_SUBTITLE_INFO_CALLBACK      0xA00006


namespace amlogic {

static const int MSG_CHECK_SUBDATA = 0;

// This hal never recreate
sp<AndroidCallbackMessageQueue> AndroidCallbackMessageQueue::sInstance = nullptr;
sp<AndroidCallbackMessageQueue> AndroidCallbackMessageQueue::Instance() {
    return sInstance;
}

AndroidCallbackMessageQueue::AndroidCallbackMessageQueue(sp<AndroidCallbackHandler> proxy) {
    android::AutoMutex _l(mLock);
    mStopped = false;
    mProxyHandler = proxy;
    sInstance = this;
    mCallbackLooperThread = std::thread(&AndroidCallbackMessageQueue::looperLoop, this);
}

AndroidCallbackMessageQueue::~AndroidCallbackMessageQueue() {
    mStopped = true;
    if (mLooper != nullptr) {
        mLooper->wake();
    }
    // post a message to wakeup and exit.
    if (mLooper != nullptr) {
        mLooper->sendMessage(this, Message(MSG_CHECK_SUBDATA));
    }
    mCallbackLooperThread.join();
    sInstance = nullptr;
}

void AndroidCallbackMessageQueue::looperLoop() {
    mLooper = new Looper(false);
    mLooper->sendMessageDelayed(100LL, this, Message(MSG_CHECK_SUBDATA));
    // never exited
    while (!mStopped && mLooper != nullptr) {
        mLooper->pollAll(-1);
        //If mLooper is set to nullptr at some point within the loop, ensure proper handling and exit the loop
        if (mLooper == nullptr) {
            SUBTITLE_LOGE("looperLoop mLooper is Null.");
            break;
        }
    }
    mLooper = nullptr;
}

void AndroidCallbackMessageQueue::handleMessage(const Message& message) {
    std::unique_ptr<SubtitleData> data(nullptr);

    mLock.lock();
    if (mSubtitleData.size() > 0) {
        auto iter = mSubtitleData.begin();
        std::unique_ptr<SubtitleData> items = std::move(*iter);
        data.swap(items);
        mSubtitleData.erase(iter);
    }
    if (mSubtitleData.size() > 0) {
        mLooper->removeMessages(this, MSG_CHECK_SUBDATA);
        mLooper->sendMessage(this, Message(MSG_CHECK_SUBDATA));
    } else {
        mLooper->removeMessages(this, MSG_CHECK_SUBDATA);
        mLooper->sendMessageDelayed(s2ns(2), this, Message(MSG_CHECK_SUBDATA));
    }
    mLock.unlock();

    // processing, outside lock.
    if (mProxyHandler != nullptr && data != nullptr) {
        SubtitleHidlParcel parcel;
        parcel.msgType = data->type;

        switch (parcel.msgType) {
            case EVENT_ON_SUBTITLEDATA_CALLBACK:
            {
                parcel.bodyInt.resize(2);
                parcel.bodyInt[0] = data->x;
                parcel.bodyInt[1] = data->y;
                SUBTITLE_LOGI("onSubtitleDataEvent EVENT_ON_SUBTITLEDATA_CALLBACK: event:%d, id:%d", data->x, data->y);
                mProxyHandler->onSubtitleEventNotify(parcel) ;
                break;
            }
            case EVENT_ON_SUBTITLEAVAILABLE_CALLBACK:
            {
                parcel.bodyInt.resize(1);
                parcel.bodyInt[0] = data->x;
                SUBTITLE_LOGI("onSubtitleDataEvent EVENT_ON_SUBTITLEAVAILABLE_CALLBACK: available:%d", data->x);
                mProxyHandler->onSubtitleEventNotify(parcel) ;
                break;
            }
            case EVENT_ON_VIDEOAFDCHANGE_CALLBACK:
            {
                parcel.bodyInt.resize(2);
                parcel.bodyInt[0] = data->x;
                parcel.bodyInt[1] = data->y;
                SUBTITLE_LOGI("onSubtitleDataEvent EVENT_ON_VIDEOAFDCHANGE_CALLBACK:afd:%d, playerid = %d", data->x, data->y);
                mProxyHandler->onSubtitleEventNotify(parcel) ;
                break;
            }
            case EVENT_ON_SUBTITLE_DIMENSION_CALLBACK:
            {
                parcel.bodyInt.resize(2);
                parcel.bodyInt[0] = data->x;
                parcel.bodyInt[1] = data->y;
                SUBTITLE_LOGI("onSubtitleDataEvent EVENT_ON_SUBTITLE_DIMENSION_CALLBACK: width:%d, height:%d", data->x, data->y);
                mProxyHandler->onSubtitleEventNotify(parcel) ;
                break;
            }
            case EVENT_ON_MIXVIDEOEVENT_CALLBACK:
            {
                parcel.bodyInt.resize(1);
                parcel.bodyInt[0] = data->x;
                SUBTITLE_LOGI("onSubtitleDataEvent EVENT_ONMIXVIDEOEVENT_CALLBACK:%d", data->x);
                mProxyHandler->onSubtitleEventNotify(parcel) ;
                break;
            }
            case EVENT_ON_SUBTITLE_LANGUAGE_CALLBACK:
            {
                std::string lang;
                lang = data->lang;
                parcel.bodyString.resize(1);
                parcel.bodyString[0] = lang.c_str();
                SUBTITLE_LOGI("onSubtitleDataEvent EVENT_ONSUBTITLE_LANGUAGE_CALLBACK:lang:%s", lang.c_str());
                mProxyHandler->onSubtitleEventNotify(parcel) ;
                break;
            }
            case EVENT_ON_SUBTITLE_INFO_CALLBACK:
            {
                parcel.bodyInt.resize(2);
                parcel.bodyInt[0] = data->x;
                parcel.bodyInt[1] = data->y;
                SUBTITLE_LOGI("onSubtitleDataEvent EVENT_ONSUBTITLE_INFO_CALLBACK:what:%d, extra:%d", data->x, data->y);
                mProxyHandler->onSubtitleEventNotify(parcel) ;
                break;

            }
            default:
            {
                parcel.bodyInt.resize(9);
                parcel.bodyInt[0] = data->width;
                parcel.bodyInt[1] = data->height;
                parcel.bodyInt[2] = data->size;
                parcel.bodyInt[3] = data->isShow;
                parcel.bodyInt[4] = data->x;
                parcel.bodyInt[5] = data->y;
                parcel.bodyInt[6] = data->videoWidth;
                parcel.bodyInt[7] = data->videoHeight;
                parcel.bodyInt[8] = data->objectSegmentId; //objectSegmentId: the current number object segment object.
                parcel.mem = *(data->mem);
                SUBTITLE_LOGI("onSubtitleDataEvent, type:%d width:%d, height:%d, size:%d", data->type, data->width, data->height, data->size);
                mProxyHandler->onSubtitleDisplayNotify(parcel);

                // some customer do not want the whole subtitle data
                // but want to know the subtitle dimension. here report it to make they happy!
                /*{
                    SubtitleHidlParcel parcel;
                    parcel.msgType = EVENT_ON_SUBTITLE_DIMENSION_CALLBACK;
                    parcel.bodyInt.resize(2);
                    parcel.bodyInt[0] = data->width;
                    parcel.bodyInt[1] = data->height;
                    SUBTITLE_LOGI("onSubtitleDataEvent EVENT_ON_SUBTITLE_DIMENSION_CALLBACK:");
                    mProxyHandler->onSubtitleEventNotify(parcel) ;
                }*/

                break;
             }
        }
    }
}

void AndroidCallbackMessageQueue::onSubtitleDataEvent(int event, int channelId) {
    android::AutoMutex _l(mLock);
    std::unique_ptr<SubtitleData> subtitleData = std::make_unique<SubtitleData>();
    subtitleData->type = EVENT_ON_SUBTITLEDATA_CALLBACK;
    subtitleData->x = event;
    subtitleData->y = channelId;
    mSubtitleData.push_back(std::move(subtitleData));
    mLooper->sendMessage(this, Message(MSG_CHECK_SUBDATA));
}
void AndroidCallbackMessageQueue::onSubtitleDimension(int width, int height) {
    android::AutoMutex _l(mLock);
    std::unique_ptr<SubtitleData> subtitleData = std::make_unique<SubtitleData>();
    subtitleData->type = EVENT_ON_SUBTITLE_DIMENSION_CALLBACK;
    subtitleData->x = width;
    subtitleData->y = height;
    mSubtitleData.push_back(std::move(subtitleData));
    mLooper->sendMessage(this, Message(MSG_CHECK_SUBDATA));
}

void AndroidCallbackMessageQueue::onSubtitleAvailable(int available) {
    android::AutoMutex _l(mLock);
    std::unique_ptr<SubtitleData> subtitleData = std::make_unique<SubtitleData>();
    subtitleData->type = EVENT_ON_SUBTITLEAVAILABLE_CALLBACK;
    subtitleData->x = available;
    mSubtitleData.push_back(std::move(subtitleData));
    mLooper->sendMessage(this, Message(MSG_CHECK_SUBDATA));
}

void AndroidCallbackMessageQueue::onVideoAfdChange(int afd, int playerid) {
    android::AutoMutex _l(mLock);
    std::unique_ptr<SubtitleData> subtitleData = std::make_unique<SubtitleData>();
    subtitleData->type = EVENT_ON_VIDEOAFDCHANGE_CALLBACK;
    subtitleData->x = afd;
    subtitleData->y = playerid;
    mSubtitleData.push_back(std::move(subtitleData));
    mLooper->sendMessage(this, Message(MSG_CHECK_SUBDATA));
}

void AndroidCallbackMessageQueue::onMixVideoEvent(int val) {
    android::AutoMutex _l(mLock);
    std::unique_ptr<SubtitleData> subtitleData = std::make_unique<SubtitleData>();
    subtitleData->type = EVENT_ON_MIXVIDEOEVENT_CALLBACK;
    subtitleData->x = val;
    mSubtitleData.push_back(std::move(subtitleData));
    mLooper->sendMessage(this, Message(MSG_CHECK_SUBDATA));
}

void AndroidCallbackMessageQueue::onSubtitleLanguage(char *lang) {
    android::AutoMutex _l(mLock);
    std::unique_ptr<SubtitleData> subtitleData = std::make_unique<SubtitleData>();
    subtitleData->type = EVENT_ON_SUBTITLE_LANGUAGE_CALLBACK;
    subtitleData->lang = lang;
    mSubtitleData.push_back(std::move(subtitleData));
    mLooper->sendMessage(this, Message(MSG_CHECK_SUBDATA));
}

void AndroidCallbackMessageQueue::onSubtitleInfo(int what, int extra) {
    android::AutoMutex _l(mLock);
    std::unique_ptr<SubtitleData> subtitleData = std::make_unique<SubtitleData>();
    subtitleData->type = EVENT_ON_SUBTITLE_INFO_CALLBACK;
    subtitleData->x = what;
    subtitleData->y = extra;
    mSubtitleData.push_back(std::move(subtitleData));
    mLooper->sendMessage(this, Message(MSG_CHECK_SUBDATA));
}


bool AndroidCallbackMessageQueue::postDisplayData(const char *data,  int type,
        int x, int y, int width, int height,
        int videoWidth, int videoHeight, int size, int cmd, int objectSegmentId) {

    sp<HidlMemory> mem;
    sp<IAllocator> ashmemAllocator = IAllocator::getService("ashmem");

    bool allocRes = false;

    Return<void> r = ashmemAllocator->allocate(size+1024, [&](bool success, const hidl_memory& _mem) {
            if (success) {
                mem  = HidlMemory::getInstance(_mem);
                sp<IMemory> memory = mapMemory(_mem);
                if (memory == nullptr) {
                    SUBTITLE_LOGE("map Memory Failed!!");
                    return; // we may need  status!
                }

                memory->update();
                if (data != nullptr) memcpy(memory->getPointer(), data, size);
                memory->commit();
                allocRes = true;
            } else {
                SUBTITLE_LOGE("alloc memory Fail");
            }
        });

    if (r.isOk() && allocRes) {
        android::AutoMutex _l(mLock);
        std::unique_ptr<SubtitleData> subtitleData = std::make_unique<SubtitleData>();
        subtitleData->type = type;
        subtitleData->x = x;
        subtitleData->y = y;
        subtitleData->width = width;
        subtitleData->height = height;
        subtitleData->videoWidth = videoWidth;
        subtitleData->videoHeight = videoHeight;
        subtitleData->size = size;
        subtitleData->isShow = cmd;
        subtitleData->mem = mem;
        subtitleData->objectSegmentId = objectSegmentId; //objectSegmentId: the current number object segment object.
        mSubtitleData.push_back(std::move(subtitleData));
        mLooper->sendMessage(this, Message(MSG_CHECK_SUBDATA));

    SUBTITLE_LOGI(" in postDisplayData:%s type:%d, width=%d, height=%d size=%d objectSegmentId=%d",
        __func__, type,  width, height, size, objectSegmentId);

    } else {
        SUBTITLE_LOGE("Fail to process hidl memory!!");
    }


    return true;
}

}
