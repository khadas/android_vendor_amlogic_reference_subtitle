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

#ifndef __SUBTITLE_DVB_PARSER_H__
#define __SUBTITLE_DVB_PARSER_H__
#include "Parser.h"
#include "DataSource.h"
#include "sub_types.h"

#include "dvbCommon.h"

struct DVBSubContext;
struct DVBSubObjectDisplay;

class DvbParser: public Parser {
public:
    DvbParser(std::shared_ptr<DataSource> source);
    virtual ~DvbParser();
    virtual int parse();
    virtual bool updateParameter(int type, void *data);
    virtual void dump(int fd, const char *prefix);
    void notifyCallerAvail(int avail);

private:
    int getSpu();
    int getInterSpu();
    int getDvbSpu();
    int decodeSubtitle(std::shared_ptr<AML_SPUVAR> spu, char *pSrc, const int size);

    int softDemuxParse();
    int hwDemuxParse();

    void parsePageSegment(const uint8_t *buf, int bufSize);
    void parseRegionSegment(const uint8_t *buf, int bufSize);
    void parseClutSegment(const uint8_t *buf, int bufSize);
    int parseObjectSegment(const uint8_t *buf, int bufSize);
    void parseDisplayDefinitionSegment(const uint8_t *buf, int bufSize);
    int parsePixelDataBlock(DVBSubObjectDisplay *display,
            const uint8_t *buf, int bufSize, int top_bottom, int non_mod);


    int displayEndSegment(std::shared_ptr<AML_SPUVAR> spu);

    void saveResult2Spu(std::shared_ptr<AML_SPUVAR> spu);
    void notifySubtitleDimension(int width, int height);
    void notifySubtitleErrorInfo(int error);

    void checkDebug();

    int init();

    void callbackProcess();
    DVBSubContext *mContext;

    int mDumpSub;

    //CV for notification.
    std::mutex mMutex;
    std::condition_variable mCv;
    int mPendingAction;
    std::shared_ptr<std::thread> mTimeoutThread;
};


#endif

