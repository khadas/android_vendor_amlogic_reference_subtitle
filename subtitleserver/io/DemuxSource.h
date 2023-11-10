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

#ifndef __SUBTITLE_DEMUXDEVICE_SOURCE_H__
#define __SUBTITLE_DEMUXDEVICE_SOURCE_H__

#include <string>
#include <memory>
#include <mutex>
#include <utils/Thread.h>
#include <thread>

#include "Segment.h"
#include "SubtitleTypes.h"
#include "DataSource.h"

#define DEMUX_SOURCE_ID 0//0
#define SCTE27_TID 0xC6

class DemuxSource : public DataSource {

public:
    DemuxSource();
    virtual ~DemuxSource();
    size_t totalSize();
    SubtitleIOType type();
    bool start();
    bool stop();

    bool isFileAvailable();
    virtual size_t availableDataSize();
    virtual size_t read(void *buffer, size_t size);
    virtual void dump(int fd, const char *prefix);

    virtual int onData(const char*buffer, int len) {
        return -1;
    }
    virtual void updateParameter(int type, void *data) ;
    virtual void setPipId (int mode, int id);
    static inline DemuxSource *getCurrentInstance();
    std::shared_ptr<BufferSegment> mSegment;
    int mSubType;
    virtual size_t lseek(int offSet, int whence) {return 0;}
    bool mDumpSub;
private:
    void loopRenderTime();
    void pes_data_cb(int dev_no, int fhandle, const uint8_t *data, int len, void *user_data);

    bool notifyInfoChange();
    void checkDebug();
    int mRdFd;
    std::shared_ptr<std::thread> mRenderTimeThread;
    std::shared_ptr<std::thread> mReadThread;
    // std::shared_ptr<BufferSegment> mSegment;
    // for poping and parsing
    std::shared_ptr<BufferItem> mCurrentItem;

    SubtitleIOState mState;
    bool mExitRequested;
    TVSubtitleData *mDemuxContext;

    int mPid = -1;
    int mDemuxId = -1;
    int mSecureLevelFlag = 0;
    int mParam1;
    int mParam2; //ancillary_id
    static DemuxSource *sInstance;
    void* mMediaSync;
    bool mMediaSyncDestroyFlag;

};

#endif

