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

#define LOG_TAG "SubtitleSource"
#include <unistd.h>
#include <fcntl.h>
#include <string>

#include "SubtitleLog.h"
#include <utils/CallStack.h>

#include "SocketSource.h"
#include "SocketServer.h"

static inline unsigned int make32bitValue(const char *buffer) {
    return buffer[0] | (buffer[1]<<8) | (buffer[2]<<16) | (buffer[3]<<24);
}

static inline std::shared_ptr<char>
makeNewSpBuffer(const char *buffer, int size) {
    char *copyBuffer = new char[size];
    memcpy(copyBuffer, buffer, size);
    return std::shared_ptr<char>(copyBuffer, [](char *buf) { delete [] buf; });
}

SocketSource::SocketSource() : mTotalSubtitle(-1),
                mSubtitleType(-1), mRenderTimeUs(0), mStartPts(0), mState(E_SOURCE_INV){
    SocketSource("");
}

SubtitleIOType SocketSource::type() {
    return E_SUBTITLE_SOCK;
}

SocketSource::SocketSource(const std::string url) : mTotalSubtitle(-1),
                mSubtitleType(-1), mRenderTimeUs(0), mStartPts(0) {
    (void) url;
    mNeedDumpSource = false;
    mDumpFd = -1;
    mSegment = std::shared_ptr<BufferSegment>(new BufferSegment());

    // register listener: mSegment.
    // mSgement register onData, SocketSource::onData.
    SubSocketServer::registClient(this);
    mState = E_SOURCE_INV;

}

SocketSource::~SocketSource() {
    SUBTITLE_LOGI("%s", __func__);

    // TODO: no ring ref?
    SubSocketServer::unregisterClient(this);
}

bool SocketSource::notifyInfoChange_l(int type) {
    for (auto it = mInfoListeners.begin(); it != mInfoListeners.end(); it++) {
        auto wk_listener = (*it);
        if (auto lstn = wk_listener.lock()) {
            if (lstn == nullptr) return false;

            switch (type) {
                case eTypeSubtitleRenderTime:
                    lstn->onRenderTimeChanged(mRenderTimeUs);
                    break;

                case eTypeSubtitleTotal:
                    lstn->onSubtitleChanged(mTotalSubtitle);
                    break;

                case eTypeSubtitleStartPts:
                    lstn->onRenderStartTimestamp(mStartPts);
                    break;

                case eTypeSubtitleType:
                    lstn->onTypeChanged(mSubtitleType);
                    break;
            }
        }
    }

    return true;
}

int SocketSource::onData(const char *buffer, int len) {
    int type = make32bitValue(buffer);
    //SUBTITLE_LOGI("subtype=%x eTypeSubtitleRenderTime=%x size=%d", type, eTypeSubtitleRenderTime, len);
    switch (type) {
        case eTypeSubtitleRenderTime: {
            static int count = 0;
            mRenderTimeUs = (((int64_t)make32bitValue(buffer+8))<<32) | make32bitValue(buffer+4);//make32bitValue(buffer + 4);

            if (count++ %100 == 0)
                SUBTITLE_LOGI("mRenderTimeUs:%x(updated %d times) time:%llu %llx", type, count, mRenderTimeUs, mRenderTimeUs);
            break;
        }
        case eTypeSubtitleTotal: {
            mTotalSubtitle = make32bitValue(buffer + 4);
            SUBTITLE_LOGI("eTypeSubtitleTotal:%x total:%d", type, mTotalSubtitle);
            break;
        }
        case eTypeSubtitleStartPts: {
            mStartPts = (((int64_t)make32bitValue(buffer+8))<<32) | make32bitValue(buffer+4);
            SUBTITLE_LOGI("eTypeSubtitleStartPts:%x time:%llx", type, mStartPts);
            break;
        }
        case eTypeSubtitleType: {
            mSubtitleType = make32bitValue(buffer + 4);
            SUBTITLE_LOGI("eTypeSubtitleType:%x %x len=%d", type, mSubtitleType, len);
            break;
        }

        case eTypeSubtitleData:
            SUBTITLE_LOGI("eTypeSubtitleData: len=%d", len);
            if (mTotalSubtitle != -1 || mSubtitleType != -1)
                mSegment->push(makeNewSpBuffer(buffer+4, len-4), len-4);
            break;

        case eTypeSubtitleTypeString:
        case eTypeSubtitleLangString:
        case eTypeSubtitleResetServ:
        case eTypeSubtitleExitServ:
            SUBTITLE_LOGI("not handled message: %s", buffer+4);
            break;

        default: {
            SUBTITLE_LOGI("!!!!!!!!!SocketSource:onData(subtitleData): %d", /*buffer,*/ len);
            mSegment->push(makeNewSpBuffer(buffer, len), len);
            break;
        }
    }

    notifyInfoChange_l(type);
    return 0;
}

bool SocketSource::start() {
    mState = E_SOURCE_STARTED;
    return true;
}

bool SocketSource::stop() {
    mState = E_SOURCE_STOPPED;
    mSegment->notifyExit();
    return true;
}


size_t SocketSource::read(void *buffer, size_t size) {
    int read = 0;

    // Current design of Parser Read, do not need add lock protection.
    // because all the read, is in parser thread.
    // We only need add lock here, is for protect access the mCurrentItem's
    // member function multi-thread.
    // Current Impl do not need lock, if required, then redesign the SegmentBuffer.
    // std::unique_lock<std::mutex> autolock(mLock);

    //in case of applied size more than 1024*2, such as dvb subtitle,
    //and data process error for two reads.
    //so add until read applied data then exit.
    while (read != size && mState == E_SOURCE_STARTED) {
        if (mCurrentItem != nullptr && !mCurrentItem->isEmpty()) {
            read += mCurrentItem->read_l(((char *)buffer+read), size-read);
            //SUBTITLE_LOGI("read:%d,size:%d", read, size);
            if (read == size) break;
        } else {
            SUBTITLE_LOGI("mCurrentItem null, pop next buffer item");
            mCurrentItem = mSegment->pop();
        }
    }

    if (mNeedDumpSource) {
        if (mDumpFd == -1) {
            mDumpFd = ::open("/data/local/traces/cur_sub.dump", O_RDWR | O_CREAT, 0666);
            SUBTITLE_LOGI("need dump Source: mDumpFd=%d %d", mDumpFd, errno);
        }

        if (mDumpFd > 0) {
            write(mDumpFd, buffer, size);
        }
    }

    return read;
}


void SocketSource::dump(int fd, const char *prefix) {
    dprintf(fd, "\n%s SocketSource:\n", prefix);
    {
        std::unique_lock<std::mutex> autolock(mLock);
        for (auto it = mInfoListeners.begin(); it != mInfoListeners.end(); it++) {
            auto wk_listener = (*it);
            if (auto lstn = wk_listener.lock())
                dprintf(fd, "%s   InfoListener: %p\n", prefix, lstn.get());
        }
    }
    dprintf(fd, "%s   state:%d\n\n", prefix, mState);

    dprintf(fd, "%s   Total Subtitle Tracks: %d\n", prefix, mTotalSubtitle);
    dprintf(fd, "%s   Current Subtitle type: %d\n", prefix, mSubtitleType);
    dprintf(fd, "%s   VideoStart PTS       : %lld\n", prefix, mStartPts);
    dprintf(fd, "%s   Current Video PTS    : %lld\n", prefix, mRenderTimeUs);

    dprintf(fd, "\n%s   Current Buffer Size: %d\n", prefix, mSegment->availableDataSize());
}

