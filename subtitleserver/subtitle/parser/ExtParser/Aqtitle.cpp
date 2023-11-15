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

#define LOG_TAG "Aqtitle"

#include "Aqtitle.h"

Aqtitle::Aqtitle(std::shared_ptr<DataSource> source) : TextSubtitle(source) {
    ALOGE("Aqtitle Subtitle");
}


Aqtitle::~Aqtitle() {

}

/**
**************************************************************
-->> 000004
Don't move

-->> 000028


-->> 000120
Look at that

-->> 000150


-->> 000153
Those are kids.

-->> 000182

*************************************************************

*/


std::shared_ptr<ExtSubItem> Aqtitle::decodedItem() {
    char line[LINE_LEN + 1] = {0};
    int startPts = 0;
    int endPts = 0;

    while (mReader->getLine(line)) {

        //ALOGD("%d %d %d", __LINE__, strlen(line), line[strlen(line)-1]);
        //ALOGD("%d %s is EmptyLine?%d", __LINE__, line, isEmptyLine(line));
        if (sscanf(line, "-->> %d", &startPts) < 1) {
            continue;
        }

        do {
            if (!mReader->getLine(line)) {
                return nullptr;
            }
        } while (strlen(line) == 0);

        if (isEmptyLine(line)) {
            ALOGD("?%d %d %d %d [%d]? why new line??", __LINE__, line[0], line[1], line[2], strlen(line));
            continue;
        }
        // Got Start Pts, Got Subtitle String...
        std::string sub(line);
        // poll all subtitles
        while (mReader->getLine(line)) {
            if (isEmptyLine(line)) {
                break;
            }
            sub.append(line);
        }

        do {
            if (!mReader->getLine(line)) {
                return nullptr;
            }
        } while (isEmptyLine(line));

        if (sscanf(line, "-->> %d", &endPts) < 1) {
            // invalid. try next
            continue;
        }

        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        item ->start = startPts;
        item ->end = endPts;
        item->lines.push_back(sub);
        ALOGD("Add Item: %d:%d %s", startPts, endPts, sub.c_str());
        return item;
    }

    return nullptr;
}
