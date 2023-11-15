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

#define LOG_TAG "Mpsub"

#include "Mpsub.h"

Mpsub::Mpsub(std::shared_ptr<DataSource> source): TextSubtitle(source) {
    mPosition = 0.0f;
}

Mpsub::~Mpsub() {
}

std::shared_ptr<ExtSubItem> Mpsub::decodedItem() {
    char line[LINE_LEN+1];
    float a, b;

    while (mReader->getLine(line)) {
        ALOGD(" read: %s", line);
        if (sscanf(line, "%f %f", &a, &b) != 2) {
                continue;
        }

        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());

        mPosition += a*100;
        item->start = mPosition;
        mPosition += b*100;
        item->end = mPosition;

        while (mReader->getLine(line)) {
            if (isEmptyLine(line)) break;

            std::string s(line);
            item->lines.push_back(s);
        }

        return item;
    }
    return nullptr;
}


