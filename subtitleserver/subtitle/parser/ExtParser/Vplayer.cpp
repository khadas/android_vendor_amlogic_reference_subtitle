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

#define LOG_TAG "Vplayer"

#include "Vplayer.h"

// Parse similar as Lyrics.
Vplayer::Vplayer(std::shared_ptr<DataSource> source): TextSubtitle(source) {
    mBuffer = new char[LINE_LEN + 1]();
    mReuseBuffer = false;
}

Vplayer::~Vplayer() {
    delete[] mBuffer;
}
std::shared_ptr<ExtSubItem> Vplayer::decodedItem() {
    int a1, a2, a3;
    char text[LINE_LEN + 1];
    int pattenLen;

    while (true) {
        if (!mReuseBuffer) {
            if (mReader->getLine(mBuffer) == nullptr) {
                return nullptr;
            }
        }

        // parse start and text
        if (sscanf(mBuffer, "%d:%d:%d:%[^\n\r]", &a1, &a2, &a3, text) < 4) {
            mReuseBuffer = false;
            // fail, check again.
            continue;
        }

        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        item->start =  a1 * 360000 + a2 * 6000 + a3 * 100;
        item->end = item->start + 200;
        item->lines.push_back(std::string(text));

        // get time End, maybe has end time, maybe not, handle this case.
        if (mReader->getLine(mBuffer) == nullptr) {
            return item;
        }
        // has end??
        pattenLen = sscanf(mBuffer, "%d:%d:%d:%[^\n\r]", &a1, &a2, &a3, text);
        if (pattenLen == 4) {
            mReuseBuffer = true;
            return item;
        } else if (pattenLen == 3) {
            item->end = a1 * 360000 + a2 * 6000 + a3 * 100;
        }

        mReuseBuffer = false;
        return item;
    }
    return nullptr;
}

