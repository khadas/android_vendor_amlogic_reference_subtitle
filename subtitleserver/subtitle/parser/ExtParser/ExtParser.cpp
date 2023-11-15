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

#include <string>
#include <list>
#include <thread>
#include <algorithm>
#include <functional>
#include <utils/Log.h>

#include "ExtParser.h"
#include "ParserFactory.h"
#include "ExtSubFactory.h"


//TODO: move to utils directory
ExtParser::ExtParser(std::shared_ptr<DataSource> source, int trackId) {
    mDataSource = source;
    mParseType = TYPE_SUBTITLE_EXTERNAL;

    mPtsRate = 24;
    mGotPtsRate = true;
    mNoTextPostProcess = 0;  // 1 => do not apply text post-processing
    mState = SUB_INIT;
    mMaxSpuItems = EXTERNAL_MAX_NUMBER_SPU_ITEM;

    mIdxSubTrackId = trackId;

    mSubDecoder = ExtSubFactory::create(source);
    if (mSubDecoder != nullptr) {
        mSubDecoder->decodeSubtitles(mIdxSubTrackId);
    }
}

ExtParser::~ExtParser() {
    ALOGI("%s", __func__);
    // call back may call parser, parser later destroy
    mSubIndex = 0;
}


void ExtParser::resetForSeek() {
    mSubIndex = 0;
}


int ExtParser::getSpu() {
    int ret = -1;

    if (mSubDecoder != nullptr) {
        std::shared_ptr<AML_SPUVAR> spu = mSubDecoder->popDecodedItem();
        if (spu != nullptr) {
            addDecodedItem(spu);
            ret = 0; // has subtitle.
        }
    }
    return ret;
}


int ExtParser::parse() {
    if (!mThreadExitRequested) {
        bool needIdling = true;
        if (mState == SUB_INIT) {
            mState = SUB_PLAYING;
        } else if (mState == SUB_PLAYING && mDecodedSpu.size() < mMaxSpuItems) {
            needIdling = getSpu() != 0;
        }

        if (needIdling) {
            usleep(33*1000LL);
        }
    }
    return 0;
}

void ExtParser::dump(int fd, const char *prefix) {
    dprintf(fd, "\nExtParser: %p at\n", mSubDecoder.get());
    mSubDecoder->dump(fd, "   ");
}
