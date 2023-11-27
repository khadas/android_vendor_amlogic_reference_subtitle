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
#define LOG_TAG "SubtitleService"

#include <list>
#include <memory>
#include <mutex>

#include "SubtitleLog.h"

#include "Segment.h"


std::shared_ptr<BufferItem> BufferSegment::pop() {
    std::shared_ptr<BufferItem> item;

    std::unique_lock<std::mutex> autolock(mMutex);
    while (mSegments.size() == 0 && mEnabled) {
        mCv.wait(autolock);
    }

    if (mSegments.size() == 0) return nullptr;

    item = mSegments.front();
    mSegments.pop_front();
    mAvailableDataSize -= item->getSize();

    return item;
}

bool BufferSegment::push(std::shared_ptr<char> buf, size_t size) {
    if (size <= 0) return false;

    std::shared_ptr<BufferItem> item = std::shared_ptr<BufferItem>(new BufferItem(buf, size));

    std::unique_lock<std::mutex> autolock(mMutex);
    SUBTITLE_LOGI("BufferSegment:current size: %d", mSegments.size());
    mSegments.push_back(item);
    mAvailableDataSize += size;
    mCv.notify_all();

    return true;
}


