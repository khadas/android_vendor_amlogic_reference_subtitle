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

#define LOG_TAG "FileSource"

#include <unistd.h>
#include <fcntl.h>
#include <string>

#include "SubtitleLog.h"
#include <utils/CallStack.h>

#include "FileSource.h"
#include "IpcDataTypes.h"


FileSource::FileSource(int fd, int extFd) {
    SUBTITLE_LOGI("%s fd:%d", __func__, fd);
    mFd = fd;
    if (mFd > 0) {
        ::lseek(mFd, 0, SEEK_SET);
    }

    if (extFd > 0) {
        mExtraFd = extFd;
    }
}

FileSource::~FileSource() {
    SUBTITLE_LOGI("%s mFd:%d", __func__, mFd);
    if (mFd > 0) {
        ::close(mFd);
        mFd = -1;
    }

    if (mExtraFd > 0) {
        ::close(mExtraFd);
    }
}

int FileSource::onData(const char *buffer, int len) {
    return 0;
}

bool FileSource::start() {
    SUBTITLE_LOGI("%s", __func__);
    return true;
}

bool FileSource::stop() {
    SUBTITLE_LOGI("%s mFd:%d", __func__, mFd);
    return true;
}

SubtitleIOType FileSource::type() {
    return E_SUBTITLE_FILE;
}
bool FileSource::isFileAvailable() {
    SUBTITLE_LOGI("%s", __func__);
    return (mDumpFd > 0);
}

size_t FileSource::lseek(int offSet, int whence) {
    SUBTITLE_LOGI("%s", __func__);
    if (mFd > 0) {
        return ::lseek(mFd, offSet, whence);
    } else {
        return 0;
    }
}


size_t FileSource::availableDataSize() {
    int len = 0;
    if (mFd > 0) {
        len = ::lseek(mFd, 0L, SEEK_END);
        lseek(0, SEEK_SET);
    }
    return len;
}


size_t FileSource::read(void *buffer, size_t size) {
    int data_size = size, r = 0, read_done = 0;
    char *buf = (char *)buffer;
    do {
        errno = 0;
        r = ::read(mFd, buf + read_done, data_size);
    } while (r <= 0 && (errno == EINTR || errno == EAGAIN));
    SUBTITLE_LOGI("have read r=%d, mRdFd:%d, size:%d errno:%d(%s)", r, mFd, size, errno, strerror(errno));
    return r;
}

void FileSource::dump(int fd, const char *prefix) {
    dprintf(fd, "\nFileSource:\n");
}

