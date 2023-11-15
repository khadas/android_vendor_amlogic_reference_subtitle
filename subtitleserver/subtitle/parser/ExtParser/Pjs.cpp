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

#define LOG_TAG "Pjs"

#include "Pjs.h"
#include <utils/Log.h>


Pjs::Pjs(std::shared_ptr<DataSource> source): TextSubtitle(source) {
    mPtsRate = 15;
}
Pjs::~Pjs() {
}

/*
 * PJS subtitles reader.
 * That's the "Phoenix japanimation Society" format.
 * I found some of them in http://www.scriptsclub.org/ (used for anime).
 * The time is in tenths of second.
 * almost useless now!
 */
// TODO: C++ style
std::shared_ptr<ExtSubItem> Pjs::decodedItem() {
    char *line = (char *)MALLOC(LINE_LEN+1);
    if (!line) {
        ALOGE("[%s::%d] line malloc error!\n", __FUNCTION__, __LINE__);
        return nullptr;
    }
    char *text = (char *)MALLOC(LINE_LEN+1);
    if (!text) {
        ALOGE("[%s::%d] text malloc error!\n", __FUNCTION__, __LINE__);
        free(line);
        return nullptr;
    }
    memset(line, 0, LINE_LEN+1);
    memset(text, 0, LINE_LEN+1);
    while (mReader->getLine(line)) {
        ALOGD(" read: %s", line);
        int start, end;
        if (sscanf(line, "%d,%d,%[^\n\r]", &start, &end, text) != 3) {
            continue;
        }
        char *p = trim(text, "\t ");
        p = triml(p, "\t ");
        int len = strlen(p);
        if (p[0] == '"' && p[len-1] == '"') {
            p[len-1] = 0;
            p = p + 1;
        }

        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        item->start = start * 100 / mPtsRate;
        item->end = end * 100 / mPtsRate;;
        item->lines.push_back(std::string(p));
        free(line);
        free(text);
        return item;
    }
    free(line);
    free(text);
    return nullptr;
}

