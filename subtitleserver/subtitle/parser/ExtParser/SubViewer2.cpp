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

#define LOG_TAG "SubViewer2"

#include "SubViewer2.h"

SubViewer2::SubViewer2(std::shared_ptr<DataSource> source): TextSubtitle(source) {
}

SubViewer2::~SubViewer2() {
}

/***************
* VobSub (paired with an .IDX file)--binary format.
* DVDSubtitle format:
{HEAD
DISCID=
DVDTITLE=Disney's Dinosaur
CODEPAGE=1250
FORMAT=ASCII
LANG=English
TITLE=1
ORIGINAL=ORIGINAL
AUTHOR=McPoodle
WEB=http://www.geocities.com/mcpoodle43/subs/
INFO=Extended Edition
LICENSE=
}
{T 00:00:50:28 This is the Earth at a time when the dinosaurs roamed...}
{T 00:00:54:08 a lush and fertile planet.}
****/
std::shared_ptr<ExtSubItem> SubViewer2::decodedItem() {
    char * line = (char *)MALLOC(LINE_LEN+1);
    char * text = (char *)MALLOC(LINE_LEN+1);
    int a1, a2, a3, a4;
    memset(line, 0, LINE_LEN+1);
    memset(text, 0, LINE_LEN+1);

    while (mReader->getLine(line)) {
        if (sscanf(line, "{T %d:%d:%d:%d %[^\n\r]", &a1, &a2, &a3, &a4, text) < 5) {
            if (sscanf(line, "{T %d:%d:%d:%d", &a1, &a2, &a3, &a4) < 4)
            {
                continue;
            }
            mReader->getLine(text);
        }

        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        item->start = a1 * 360000 + a2 * 6000 + a3 * 100 + a4 / 10;
        item->end = item->start + 200;
        item->lines.push_back(std::string(text));

        if (mReader->getLine(line)) {
            if (sscanf(line, "{T %d:%d:%d:%d", &a1, &a2, &a3, &a4) < 4) {
                if (mReader->getLine(line)) {
                    if (sscanf(line, "{T %d:%d:%d:%d", &a1, &a2, &a3, &a4) == 4) {
                        item->end = a1 * 360000 + a2 * 6000 + a3 * 100 + a4 / 10;
                    }
                    mReader->backtoLastLine();
                }
            }else {
                item->end = a1 * 360000 + a2 * 6000 + a3 * 100 + a4 / 10;
                mReader->backtoLastLine();
            }
        }

        free(line);
        free(text);

        // TODO: multi line support
        return item;
    }
    free(line);
    free(text);

    return nullptr;
}


