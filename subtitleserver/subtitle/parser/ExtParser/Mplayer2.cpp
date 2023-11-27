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

#define LOG_TAG "Mplayer2"

#include "Mplayer2.h"
#include "SubtitleLog.h"


Mplayer2::Mplayer2(std::shared_ptr<DataSource> source): TextSubtitle(source) {
}

Mplayer2::~Mplayer2() {
}

std::shared_ptr<ExtSubItem> Mplayer2::decodedItem() {
    char *line = (char *)MALLOC(LINE_LEN+1);
    if (!line) {
        SUBTITLE_LOGE("[%s::%d] line malloc error!\n", __FUNCTION__, __LINE__);
        return nullptr;
    }
    char *line2 = (char *)MALLOC(LINE_LEN);
    if (!line2) {
        SUBTITLE_LOGE("[%s::%d] line2 malloc error!\n", __FUNCTION__, __LINE__);
        free(line);
        return nullptr;
    }
    memset(line, 0, LINE_LEN+1);
    memset(line2, 0, LINE_LEN);
    while (mReader->getLine(line)) {
        int start =0, end = 0;
        SUBTITLE_LOGI(" read: %s", line);
        if (sscanf(line, "[%d][%d]%[^\r\n]", &start, &end, line2) < 3) {
                continue;
        }


        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        item->start = start * 10;
        item->end = end * 10;
        std::string s(line2);
        item->lines.push_back(s);
        free(line);
        free(line2);
        return item;
    }
    free(line);
    free(line2);
    return nullptr;
}

