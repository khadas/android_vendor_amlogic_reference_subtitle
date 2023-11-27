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

#define LOG_TAG "WebVtt"
#include <regex>

#include "WebVtt.h"



SimpleWebVtt::SimpleWebVtt(std::shared_ptr<DataSource> source): TextSubtitle(source) {
    SUBTITLE_LOGE("SimpleWebVtt Subtitle");
}

SimpleWebVtt::~SimpleWebVtt() {
}

/**
********************* Web VTT as bellow ***********************
WEBVTT

00:01.000 --> 00:04.000
Never drink liquid nitrogen.

00:05.000 --> 00:09.000
- It will perforate your stomach.
- You could die.

NOTE This is the last line in the file
***************************************************************
*/
std::shared_ptr<ExtSubItem> SimpleWebVtt::decodedItem() {
    char line[LINE_LEN + 1];
    int len = 0;

    while (mReader->getLine(line)) {

        int a1=0, a2=0, a3=0, a4=0, b1=0, b2=0, b3=0, b4=0;
        if ((len = sscanf(line, "%d:%d:%d.%d --> %d:%d:%d.%d", &a1, &a2, &a3, &a4, &b1, &b2, &b3, &b4)) < 8) {
            if ((len = sscanf(line, "%d:%d.%d --> %d:%d.%d",
                &a2, &a3, &a4,  &b2, &b3, &b4)) < 6) {
                continue;
            }
        }

        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        item->start = a1 * 360000 + a2 * 6000 + a3 * 100 + a4 / 10;
        item->end = b1 * 360000 + b2 * 6000 + b3 * 100 + b4 / 10;

        for (int i = 0; i < SUB_MAX_TEXT;) {
            // next line, is subtitle
            if (!mReader->getLine(line)) {
                return NULL;
            }

            char *p = NULL;
            len = 0;
            for (p = line; *p != '\n' && *p != '\r' && *p;
                    p++, len++);

            if (len) {
                int skip = 0;

                std::string s;
                p = line;
                if (len >2 && line[0] == '-' && line[1] == ' ') {
                    p += 2;
                    len -= 2;
                }
                s.append(p, len);
                removeHtmlToken(s);
                item->lines.push_back(s);
            } else {
                break;
            }
        }
        return item;
    }
    return nullptr;
}

