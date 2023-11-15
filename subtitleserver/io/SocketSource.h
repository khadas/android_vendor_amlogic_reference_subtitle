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

#ifndef __SUBTITLE_SOCKET_SOURCE_H__
#define __SUBTITLE_SOCKET_SOURCE_H__

#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <mutex>

#include "DataSource.h"
#include "InfoChangeListener.h"
#include "Segment.h"

// re-design later
// still don't know how to impl this
// currently, very simple wrap of previous impl


class SocketSource : public DataSource {

public:
    SocketSource();
    SocketSource(const std::string url);
    virtual ~SocketSource();

    virtual bool start();
    virtual bool stop();
    virtual SubtitleIOType type();
    virtual size_t availableDataSize() {
        int size = 0;
        if (mSegment != nullptr) size += mSegment->availableDataSize();
        if (mCurrentItem != nullptr) size += mCurrentItem->getRemainSize();
        return size;
    }
    virtual size_t lseek(int offSet, int whence) {return 0;}
    virtual size_t read(void *buffer, size_t size);
    virtual int onData(const char*buffer, int len);
    virtual void dump(int fd, const char *prefix);

private:
    bool notifyInfoChange_l(int type);

    // subtitle infos, here
    int mTotalSubtitle;
    int mSubtitleType;

    uint64_t mRenderTimeUs;
    uint64_t mStartPts;
    std::string mSubInfo;

    std::shared_ptr<BufferSegment> mSegment;
    // for poping and parsing
    std::shared_ptr<BufferItem> mCurrentItem;
    SubtitleIOState mState;
};


#endif

