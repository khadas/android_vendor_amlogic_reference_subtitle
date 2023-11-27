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

#define LOG_TAG "Ttml"

#include "Ttml.h"
#include "tinyxml2.h"

TTML::TTML(std::shared_ptr<DataSource> source): TextSubtitle(source) {
    mDataSource = source;
    SUBTITLE_LOGI("TTML");
    parseXml();
}

TTML::~TTML() {
}

void TTML::parseXml() {
    int size = mDataSource->availableDataSize();
    char *rdBuffer = new char[size];
    mDataSource->read(rdBuffer, size);
    // no need to keep reference here! one shot parse.
    mDataSource = nullptr;

    tinyxml2::XMLDocument doc;
    doc.Parse(rdBuffer);

    tinyxml2::XMLElement* root = doc.RootElement();
    if (root == NULL) {
        SUBTITLE_LOGI("Failed to load file: No root element.");
        doc.Clear();
        delete[] rdBuffer;
        return;
    }

    // TODO: parse header, if we support style metadata and layout property

    // parse body
    tinyxml2::XMLElement *body = root->FirstChildElement("body");
    if (body == nullptr) body = root->FirstChildElement("tt:body");
    if (body == nullptr) {
        SUBTITLE_LOGI("Error. no body found!");
        doc.Clear();
        delete[] rdBuffer;
        return;
    }

    tinyxml2::XMLElement *div = body->FirstChildElement("div");
    if (div == nullptr) div = body->FirstChildElement("tt:div");
    tinyxml2::XMLElement *parag;
    tinyxml2::XMLElement *spanarag;
    if (div == nullptr) {
        SUBTITLE_LOGI("%d", __LINE__);
        parag = root->FirstChildElement("p");
        if (parag == nullptr) parag = root->FirstChildElement("tt:p");
    } else {
        SUBTITLE_LOGI("%d", __LINE__);
        parag = div->FirstChildElement("p");
        if (parag == nullptr) parag = div->FirstChildElement("tt:p");
    }

    while (parag != nullptr) {
        const tinyxml2::XMLAttribute *attr = parag->FindAttribute("xml:id");
        if (attr != nullptr) SUBTITLE_LOGI("parseXml xml:id=%s\n", attr->Value());

        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        attr = parag->FindAttribute("begin");
        if (attr != nullptr) {
            item->start = attr->FloatValue() *100; // subtitle pts multiply
            SUBTITLE_LOGI("parseXml begin:%s\n", attr->Value());
        }

        attr = parag->FindAttribute("end");
        if (attr != nullptr) {
            item->end = attr->FloatValue() *100; // subtitle pts multiply
            SUBTITLE_LOGI("parseXml end:%s\n", attr->Value());
        }

        SUBTITLE_LOGI("parseXml parag Text:%s\n", parag->GetText());
        if (parag->GetText() == nullptr) {
            spanarag = parag->FirstChildElement("tt:span");
            SUBTITLE_LOGI("parseXml spanarag Text:%s\n", spanarag->GetText());
            item->lines.push_back(std::string(spanarag->GetText()));
        } else {
            item->lines.push_back(std::string(parag->GetText()));
        }

        parag = parag->NextSiblingElement();
        mSubtitles.push_back(item);
    }

    doc.Clear();
    delete[] rdBuffer;
}

std::shared_ptr<ExtSubItem> TTML::decodedItem() {
    if (mSubtitles.size() > 0) {
        std::shared_ptr<ExtSubItem> item = mSubtitles.front();
        mSubtitles.pop_front();
        if (item->start < 0) return nullptr;

        if (item->end <= item->start) {
            item->end = item->start + 200;
        }

        return item;
    }
    return nullptr;
}


