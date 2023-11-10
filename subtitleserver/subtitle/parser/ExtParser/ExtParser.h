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

#pragma once

#include "Parser.h"
#include "DataSource.h"
#include "SubtitleTypes.h"
#include "ExtSubStreamReader.h"
#include "TextSubtitle.h"

#define SUBAPI
/* Maximal length of line of a subtitle */
#define ENABLE_PROBE_UTF8_UTF16
#define SUBTITLE_PROBE_SIZE     1024

/**
 * Subtitle struct unit
 */
typedef struct {
    /// number of subtitle lines
    int lines;

    /// subtitle strings
    char *text[SUB_MAX_TEXT];

    /// alignment of subtitles
    sub_alignment_t alignment;
} subtext_t;

struct subdata_s {
    list_t list;        /* head node of subtitle_t list */
    list_t list_temp;

    int sub_num;
    int sub_error;
    int sub_format;
};

typedef struct subdata_s subdata_t;

typedef struct subtitle_s subtitle_t;

struct subtitle_s {
    list_t list;        /* linked list */
    int start;      /* start time */
    int end;        /* end time */
    subtext_t text;     /* subtitle text */
    unsigned char *subdata; /* data for divx bmp subtitle */
};




class ExtParser: public Parser {
public:
    ExtParser(std::shared_ptr<DataSource> source, int trackId);
    virtual ~ExtParser();
    virtual int parse();

    virtual void dump(int fd, const char *prefix);
    void resetForSeek();


protected:
    ExtSubData mSubData;
    std::shared_ptr<TextSubtitle> mSubDecoder;

    int mPtsRate = 24;
    bool mGotPtsRate = true;
    int mNoTextPostProcess = 0;  // 1 => do not apply text post-processing
    /* read one line of string from data source */
    int mSubIndex = 0;

    int mIdxSubTrackId;

private:
    //bool decodeSubtitles();

    int getSpu();
};

