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

#ifndef __SUBTITLE_DATASOURCE_H__
#define __SUBTITLE_DATASOURCE_H__
#include <list>
#include <mutex>

#include "SubtitleLog.h"

#include "InfoChangeListener.h"


typedef enum {
    E_SUBTITLE_FMQ = 0,
    E_SUBTITLE_DEV,
    E_SUBTITLE_FILE,
    E_SUBTITLE_SOCK, /*deprecated*/
    E_SUBTITLE_DEMUX,
    E_SUBTITLE_VBI,
} SubtitleIOType;

typedef enum {
    E_SOURCE_INV,
    E_SOURCE_STARTED,
    E_SOURCE_STOPPED,
} SubtitleIOState;


class DataListener {
public:
    virtual int onData(const char*buffer, int len) = 0;
    virtual ~DataListener() {};
};

class DataSource : public DataListener {

public:
    DataSource() = default;
    DataSource& operator=(const DataSource&) = delete;
    virtual ~DataSource() {
            SUBTITLE_LOGI("%s", __func__);
    }

    virtual SubtitleIOType type() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;

    virtual size_t availableDataSize() = 0;
    virtual size_t read(void *buffer, size_t bufSize) = 0;
    virtual size_t lseek(int offSet, int whence) = 0;

    virtual void updateParameter(int type, void *data) {
        (void) data;
        return;
    }

    virtual void setPipId (int mode, int id) {
        (void)mode;
        (void)id;
        return;
    }

    virtual bool isFileAvailable() {
        return false;
    }

    virtual int64_t getSyncTime() {
        return mSyncPts;
    }


    // If we got information from the data source, we notify these info to listener
    virtual void registerInfoListener(std::shared_ptr<InfoChangeListener>listener) {
        std::unique_lock<std::mutex> autolock(mLock);
        mInfoListeners.push_back(listener);
    }

    virtual void unregisteredInfoListener(std::shared_ptr<InfoChangeListener>listener) {
        std::unique_lock<std::mutex> autolock(mLock);
        for (auto it = mInfoListeners.begin(); it != mInfoListeners.end(); it++) {
            auto wk_listener = (*it);
            if (wk_listener.expired()) {
                mInfoListeners.erase(it);
                return;
            }
            auto lsn = wk_listener.lock();
            if (lsn == listener) {
                mInfoListeners.erase(it);
                return;
            }
        }
    }

    std::list<std::weak_ptr<InfoChangeListener>> mInfoListeners;

    int64_t mSyncPts;

    // TODO: dump interface.

    void enableSourceDump(bool enable) {
        mNeedDumpSource = enable;
    }

    virtual void dump(int fd, const char *prefix) = 0;
    int mDumpFd;

    // for idx sub to parse sub picture.
    int getExtraFd() {return mExtraFd;}
protected:

    std::mutex mLock;
    bool mNeedDumpSource;
    int mPlayerId;
    int mMediaSyncId;

    int mExtraFd;
};

#endif
