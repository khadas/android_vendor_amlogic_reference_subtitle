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

#define LOG_TAG "SubViewer3"

#include "SubViewer3.h"

SubViewer3::SubViewer3(std::shared_ptr<DataSource> source): TextSubtitle(source) {
}

SubViewer3::~SubViewer3() {
}

std::shared_ptr<ExtSubItem> SubViewer3::decodedItem() {
    char line[LINE_LEN + 1];
    while (mReader->getLine(line)) {
        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());

        int a0, a1, a2, a3, a4, b1, b2, b3, b4;
        if (sscanf(line, "%d  %d:%d:%d,%d  %d:%d:%d,%d",
                   &a0, &a1, &a2, &a3, &a4, &b1, &b2, &b3, &b4) < 9) {
            continue;
        }

        item->start = a1 * 360000 + a2 * 6000 + a3 * 100 + a4;
        item->end = b1 * 360000 + b2 * 6000 + b3 * 100 + b4;

        while (mReader->getLine(line)) {
            if (isEmptyLine(line)) {
                break;
            }

            std::string sub(line);
            //TODO: post process sub

            item->lines.push_back(std::string(line));
            return item;
        }
    }
    return nullptr;
}

