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

#ifndef __SUBTITLE_PGS_PARSER_H__
#define __SUBTITLE_PGS_PARSER_H__
#include "Parser.h"
#include "DataSource.h"
#include "SubtitleTypes.h"

#define OSD_HALF_SIZE 1920*1280/8

struct PgsSubtitleEpgs;

class PgsParser: public Parser {
public:
    PgsParser(std::shared_ptr<DataSource> source);
    virtual ~PgsParser();
    virtual int parse();
    virtual void dump(int fd, const char *prefix);

private:
    int getSpu(std::shared_ptr<AML_SPUVAR> spu);
    int getInterSpu();
    void checkDebug();
    int decode(std::shared_ptr<AML_SPUVAR> spu, unsigned char *psrc);
    int parserOnePgs(std::shared_ptr<AML_SPUVAR> spu);

    int softDemuxParse(std::shared_ptr<AML_SPUVAR> spu);
    int hwDemuxParse(std::shared_ptr<AML_SPUVAR> spu);
    int mDumpSub;

    PgsSubtitleEpgs *mPgsEpgs;
};


#endif

