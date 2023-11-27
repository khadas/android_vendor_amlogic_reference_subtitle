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

#define LOG_TAG "ebuttd"

#include "ExtParserEbuttd.h"
#include "tinyxml.h"
#include "tinystr.h"

ExtParserEbuttd::ExtParserEbuttd(std::shared_ptr<DataSource> source): ExtParser(source) {
    mDataSource = source;
}

ExtParserEbuttd::~ExtParserEbuttd() {

}

subtitle_t * ExtParserEbuttd::ExtSubtitleParser(subtitle_t *current) {
    char line[LINE_LEN + 1];
    char line2[LINE_LEN + 1];
    char *p, *next;
    int i;
    int size = mDataSource->availableDataSize();
    char *rdBuffer = new char[size]();
    mDataSource->read(rdBuffer, size);
    //log_print("internal_sub_read_line_xml size:%d, buffer:%s\n", size, rdBuffer);
    parseEbuttd(rdBuffer);
    delete[] rdBuffer;
    return NULL;
}

int ExtParserEbuttd::parseEbuttd(char *buffer) {
    TiXmlDocument doc;
    doc.Parse(buffer);
    TiXmlElement* tt = doc.RootElement();
    if (tt == NULL) {
        SUBTITLE_LOGI("Failed to load file: No tt element.\n");
        doc.Clear();
        return -1;
    }

    TiXmlElement* body = tt->FirstChildElement("body");
    if (body == NULL) {
        body = tt->FirstChildElement("tt:body");
        if (body == NULL) {
            SUBTITLE_LOGI("Failed to load file: No body element.\n");
            doc.Clear();
            return -1;
        }
    }

    TiXmlElement* div = body->FirstChildElement("div");
    if (div == NULL) {
        div = body->FirstChildElement("tt:div");
        if (div == NULL) {
            SUBTITLE_LOGI("Failed to load file: No div element.\n");
            doc.Clear();
            return -1;
        }
    }

    subtitle_t *sub;
    uint32_t timeStart, timeEnd;
    uint32_t hour, min, sec, ms;
    int count = 0;
    char textBuff[4096] = {0};

    for (TiXmlElement* elem = div->FirstChildElement(); elem != NULL; elem = elem->NextSiblingElement()) {
        const char* begin= elem->Attribute("begin");
        SUBTITLE_LOGI("%s begin:%s\n", __FUNCTION__, begin);
        count = sscanf(begin, "%d:%d:%d.%d", &hour, &min, &sec, &ms);
        if (count == 4) {
            timeStart = ((hour*60*60 + min*60 + sec)*1000 + ms)*90;
        }

        const char* end = elem->Attribute("end");
        SUBTITLE_LOGI("%s end:%s\n", __FUNCTION__, end);
        count = sscanf(end, "%d:%d:%d.%d", &hour, &min, &sec, &ms);
        if (count == 4) {
            timeEnd = ((hour*60*60 + min*60 + sec)*1000 + ms)*90;
        }

        memset(textBuff, 0, sizeof(textBuff));
        for (TiXmlElement* e3 = elem->FirstChildElement(); e3 != NULL; e3 = e3->NextSiblingElement()) {
            const char* text = e3->GetText();
            if (text) {
                strcat(textBuff, text);
            }
        }
        textBuff[sizeof(textBuff)-1] = 0;

        sub = (subtitle_t *) MALLOC(sizeof(subtitle_t));
        if (!sub) {
            break;
        }
        memset(sub, 0, sizeof(subtitle_t));
        sub->start = timeStart;
        sub->end = timeEnd;


        SUBTITLE_LOGI("[%s](%d,%d)%s", __FUNCTION__, sub->start, sub->end, textBuff);
        std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());
        spu->pts = sub->start;
        spu->m_delay = sub->end;
        int len = strlen(textBuff);
        spu->spu_data = (unsigned char *)malloc(len);
        memset(spu->spu_data, 0, len);
        memcpy(spu->spu_data, textBuff, len);
        spu->buffer_size = len;
        spu->isExtSub = true;
        addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
    }
    doc.Clear();
    return 1;
}



