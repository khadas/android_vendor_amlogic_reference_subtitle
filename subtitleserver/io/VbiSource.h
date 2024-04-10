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

#ifndef __SUBTITLE_VBI_SOURCE_H__
#define __SUBTITLE_VBI_SOURCE_H__

#include <string>
#include <memory>
#include <mutex>
#include <utils/Thread.h>
#include <thread>

#include "Segment.h"
#include "DataSource.h"
#include "VbiDriver.h"

class VbiSource : public DataSource {

public:
    VbiSource();
    virtual ~VbiSource();
    size_t totalSize();

    bool start();
    bool stop();
    SubtitleIOType type();

    bool isFileAvailable();
    virtual size_t availableDataSize();
    virtual size_t read(void *buffer, size_t size);
    virtual void dump(int fd, const char *prefix);

    virtual int onData(const char*buffer, int len) {
        return -1;
    }
    virtual void updateParameter(int type, void *data) ;
    virtual size_t lseek(int offSet, int whence) {return 0;}
    bool mDumpSub;

private:
    void loopRenderTime();
    void loopDriverData();
    size_t readDriverData(void *buffer, size_t size);

    bool notifyInfoChange();
    void checkDebug();
    int mRdFd;
    std::shared_ptr<std::thread> mRenderTimeThread;

    std::shared_ptr<std::thread> mReadThread;
    std::shared_ptr<BufferSegment> mSegment;
    // for poping and parsing
    std::shared_ptr<BufferItem> mCurrentItem;

    SubtitleIOState mState;
    bool mExitRequested;

};

#endif
