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

#define LOG_TAG "Subrip"

#include "Subrip.h"

Subrip::Subrip(std::shared_ptr<DataSource> source): TextSubtitle(source) {
    ALOGD("Subrip subtitle!");
    mSubData.format = "Sub Rip";
}

Subrip::~Subrip() {}

std::shared_ptr<ExtSubItem> Subrip::decodedItem() {
    char line[LINE_LEN + 1];
    int a1, a2, a3, a4, b1, b2, b3, b4;
    char *p = NULL, *q = NULL;
    int len;

    while (mReader->getLine(line)) {
        if (sscanf(line, "%d:%d:%d.%d,%d:%d:%d.%d",
                   &a1, &a2, &a3, &a4, &b1, &b2, &b3, &b4) < 8) {
            continue;
        }

       std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());

        item->start = a1 * 360000 + a2 * 6000 + a3 * 100 + a4;
        item->end = b1 * 360000 + b2 * 6000 + b3 * 100 + b4;

        // next line, is subtitle
        if (!mReader->getLine(line)) {
            return NULL;
        }
        p = q = line;
        while (true) {
            for (q = p, len = 0; *p && *p != '\r' && *p != '\n' && *p != '|'
                    && strncmp(p, "[br]", 4); p++, len++);

            std::string s(q, len);
            //strncpy(current->text.text[current->text.lines - 1], q,
            //         len);
            //current->text.text[current->text.lines - 1][len] = '\0';
             item->lines.push_back(s);
            if (!*p || *p == '\r' || *p == '\n') {
                break;
            }
            if (*p == '|') {
                p++;
            } else {
                while (*p++ != ']') ;
            }
        }
        return item;
    }
    return nullptr;
}

