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

#define LOG_TAG "Lyrics"

#include "Lyrics.h"

Lyrics::Lyrics(std::shared_ptr<DataSource> source): TextSubtitle(source) {
    //mBuffer = new char[LINE_LEN + 1]();
    mBuffer = (char *)MALLOC(LINE_LEN+1);
    memset(mBuffer, 0, LINE_LEN+1);
    mReuseBuffer = false;
    ALOGD("Lyrics");
}

Lyrics::~Lyrics() {
    ALOGD("~Lyrics--");
    free(mBuffer);
}

std::shared_ptr<ExtSubItem> Lyrics::decodedItem() {
    int64_t a1, a2, a3;
    //char text[LINE_LEN + 1];
    char * text = (char *)MALLOC(LINE_LEN+1);
    char *text1 = (char *)MALLOC(LINE_LEN+1);
    memset(text, 0, LINE_LEN+1);
    memset(text1, 0, LINE_LEN+1);
    int pattenLen;

    while (true) {
        if (!mReuseBuffer) {
            if (mReader->getLine(mBuffer) == nullptr) {
                ALOGD("return null");
                free(text);
                free(text1);
                return nullptr;
            }
        }

        // parse start and text
        //TODO for coverity
        if (sscanf(mBuffer, "[%lld:%lld.%lld]%[^\n\r]", &a1, &a2, &a3, text) < 4) {
            mReuseBuffer = false;
            // fail, check again.
            continue;
        }

        //check the text
        int64_t b1;
        int cnt = 0;
        while (sscanf(text, "[%lld:%lld.%lld]%[^\n\r]", &b1, &b1, &b1, text1) == 4) {
            if (cnt == 0) {
                MEMCPY(mBuffer, text, LINE_LEN+1);//use for next decodeItem
            }
            MEMCPY(text, text1, LINE_LEN+1);
            cnt ++;
        }

        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        item->start = a1 * 6000 + a2 * 100 + a3;
        item->end = item->start + 400;
        item->lines.push_back(std::string(text));
        //ALOGD("item start:%lld,end:%lld",item->start, item->end);
        //maybe such as this: [03:37.00][03:02.00][01:31.00] hello world
        if (cnt > 0) {
            //ALOGD("mBuffer after copy:%s", mBuffer);
            mReuseBuffer = true;
            free(text);
            free(text1);
            return item;
        } else {
        // get time End, maybe has end time, maybe not, handle this case.
            if (mReader->getLine(mBuffer) == nullptr) {
                ALOGD("file end, read null");
                free(text);
                free(text1);
                mReuseBuffer = false;//no this, may can't break while loop
                return item;
            }
            // has end??
            pattenLen = sscanf(mBuffer, "[%lld:%lld.%lld]%[^\n\r]", &a1, &a2, &a3, text);
            if (pattenLen == 4) {
                mReuseBuffer = true;
                //ALOGD("reusebuffer,text:%s",text);
                free(text);
                free(text1);
                return item;
            } else if (pattenLen == 3) {
                item->end = a1 * 6000 + a2 * 100 + a3;
                //ALOGD("item end:%lld",item->end);
            }

            mReuseBuffer = false;
            free(text);
            free(text1);
            return item;
        }
    }
    free(text);
    free(text1);
    return nullptr;
}

