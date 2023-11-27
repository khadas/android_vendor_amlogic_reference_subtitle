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

// TODO: implement

#ifndef __SUBTITLE_SEGMENT_H__
#define __SUBTITLE_SEGMENT_H__
#include <list>
#include <memory>
#include <mutex>

#include "../subtitle/parser/Parser.h"

class BufferSegment;

class BufferItem {
public:
    BufferItem() {mSize = 0;}
    BufferItem(std::shared_ptr<char> buf, size_t size) {
        mBuffer = buf;
        mSize = size;
        mPtr = 0;
        mRemainSize = mSize;
    }

    int read_l(char *buf, int size) {
        int needRead = 0;
        if (size == 0 || mRemainSize == 0) {
            return 0;
        }

        char *ptr = mBuffer.get();
        if (size > mRemainSize){
            needRead = mRemainSize;
        } else {
            needRead = size;
        }

        memcpy(buf, ptr+mPtr, needRead);
        mPtr += needRead;
        mRemainSize -= needRead;
        return needRead;
    }

    int read_check(char *buf, int size, int *isReadItemEnd, int type) {
        int needRead = 0;
        if (size == 0 || mRemainSize == 0) {
            return 0;
        }

        char *ptr = mBuffer.get();

        if (size > mRemainSize){
            needRead = mRemainSize;
        } else {
            needRead = size;
        }

        if(needRead >=4){
            for (int i = 0; i < needRead - 4; i++) {
                char *p = ptr+mPtr + i;
                if (E_SUBTITLE_DEMUX == type &&(p[0] == 0) && (p[1] == 0) && (p[2] == 1) && (p[3] == 0xbd)){
                    SUBTITLE_LOGI("read_l i:%d type:%d needRead:%d", i, type, needRead);
                    *isReadItemEnd = -1;
                    needRead = i;
                    break;
                }
            }
        }

        memcpy(buf, ptr+mPtr, needRead);
        mPtr += needRead;
        mRemainSize -= needRead;
        return needRead;
    }

    bool isEmpty() {
        return mRemainSize == 0;
    }
    size_t getRemainSize() {return mRemainSize;}
    size_t getSize() {return mSize;}

private:
    friend class BufferSegment;


    size_t mSize;
    // TODO: unique_ptr?
    std::shared_ptr<char> mBuffer;
    int mPtr;
    int mRemainSize;
};


class BufferSegment {
public:
    BufferSegment() {
        mEnabled = true;
        mAvailableDataSize = 0;
    }
    ~BufferSegment() {
        notifyExit();
    }

    std::shared_ptr<BufferItem> pop();
    bool push(std::shared_ptr<char> buf, size_t size);

    void notifyExit() {
        mEnabled = false;
        std::unique_lock<std::mutex> autolock(mMutex);
        mAvailableDataSize = 0;
        mCv.notify_all();
    }

    size_t availableDataSize() {return mAvailableDataSize;}

private:
    std::list< std::shared_ptr<BufferItem>> mSegments;
    int mAvailableDataSize;
    std::mutex mMutex;
    std::condition_variable mCv;
    bool mEnabled;
};

#endif

