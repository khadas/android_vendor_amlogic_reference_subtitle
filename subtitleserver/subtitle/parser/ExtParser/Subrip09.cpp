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

#define LOG_TAG "Subrip09"

#include "Subrip09.h"

Subrip09::Subrip09(std::shared_ptr<DataSource> source): TextSubtitle(source) {
}

Subrip09::~Subrip09() {
}
/**
**       Very simple, only parse the content **
[TITLE]
The Island
[AUTHOR]

[SOURCE]

[PRG]

[FILEPATH]

[DELAY]
0
[CD TRACK]
0
[BEGIN]
******** START SCRIPT ********
[00:00:00]
Don't move
[00:00:01]

[00:00:08]
Look at that
[00:00:10]

[00:00:10]
Those are kids.
[00:00:12]

[00:00:21]
Come here. The train is non-stop to LA. You need these to get on.
[00:00:24]

**/
std::shared_ptr<ExtSubItem> Subrip09::decodedItem() {
    char line[LINE_LEN + 1];

    while (mReader->getLine(line)) {
        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());

        int a1, a2, a3;
        if (sscanf(line, "[%d:%d:%d]", &a1, &a2, &a3) < 3) {
            continue;
        }

        item->start = a1 * 360000 + a2 * 6000 + a3 * 100;

        while (mReader->getLine(line)) {
            if (isEmptyLine(line)) {
                break;
            }


            if (sscanf(line, "[%d:%d:%d]", &a1, &a2, &a3) == 3) {
                item->end = a1 * 360000 + a2 * 6000 + a3 * 100;
            } else {
                item->lines.push_back(std::string(line));
            }
        }
        return item;
    }

    return nullptr;
}


