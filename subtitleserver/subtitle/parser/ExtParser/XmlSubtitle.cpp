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

#define LOG_TAG "XmlSubtitle"

#include "XmlSubtitle.h"
#include "tinyxml2.h"

XmlSubtitle::XmlSubtitle(std::shared_ptr<DataSource> source): TextSubtitle(source) {
    mDataSource = source;
    parseXml();
}

XmlSubtitle::~XmlSubtitle() {
    mDataSource = nullptr;
}

void XmlSubtitle::parseXml() {
    int size = mDataSource->availableDataSize();
    char *rdBuffer = new char[size];
    mDataSource->read(rdBuffer, size);
    // no need to keep reference here! one shot parse.
    mDataSource = nullptr;

    tinyxml2::XMLDocument doc;
    doc.Parse(rdBuffer);

    tinyxml2::XMLElement* root = doc.RootElement();
    if (root == NULL) {
        ALOGD("Failed to load file: No root element.\n");
        doc.Clear();
        delete[] rdBuffer;
        return;
    }

    tinyxml2::XMLElement *parag = root->FirstChildElement("Paragraph");
    while (parag != nullptr) {
        const tinyxml2::XMLAttribute *attribute = parag->FirstAttribute();
        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        tinyxml2::XMLElement *e = parag->FirstChildElement("Number");
        //if (e != nullptr) ALOGD("parseXml num:%s\n", e->GetText());

        e = parag->FirstChildElement("StartMilliseconds");
        if (e!= nullptr) {
            item->start = e->Int64Text() / 10; // subtitle pts multiply
            //ALOGD("parseXml StartMilliseconds:%s  %lld, %lld\n", e->GetText(), item->start, e->Int64Text());
        }

        e = parag->FirstChildElement("EndMilliseconds");
        if (e!= nullptr) {
            //ALOGD("parseXml EndMilliseconds:%s\n", e->GetText());
            item->end = e->Int64Text() / 10; // subtitle pts multiply
        }

        e = parag->FirstChildElement("Text");
        while (e != nullptr) {
            //ALOGD("parseXml Text:%s\n", e->GetText());
            item->lines.push_back(std::string(e->GetText()));
            e = e->NextSiblingElement("Text");
        }
        parag = parag->NextSiblingElement();
        mSubtitles.push_back(item);
    }

    doc.Clear();
    delete[] rdBuffer;
}

std::shared_ptr<ExtSubItem> XmlSubtitle::decodedItem() {
    if (mSubtitles.size() > 0) {
        std::shared_ptr<ExtSubItem> item = mSubtitles.front();
        mSubtitles.pop_front();
        if (item->start < 0) return nullptr;

        if (item->end <= item->start) {
            item->end = item->start + 2000;
        }

        return item;
    }
    return nullptr;
}

