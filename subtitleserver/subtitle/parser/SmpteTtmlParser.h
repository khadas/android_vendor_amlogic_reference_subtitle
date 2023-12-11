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

#ifndef __SUBTITLE_SMPTE_TTML_PARSER_H__
#define __SUBTITLE_SMPTE_TTML_PARSER_H__
#include "Parser.h"
#include "DataSource.h"
#include "SubtitleTypes.h"

#include "DvbCommon.h"

//#include <aribcaption/aribcaption.h>
//#include <aribcaption/decoder.h>

#define AV_NOPTS_VALUE          INT64_C(0x8000000000000000)
#define AV_TIME_BASE            1000000
#define AV_TIME_BASE_Q          (AVRational){1, AV_TIME_BASE}
#define AVPALETTE_SIZE 1024
#define MAX_SLICES 64
#define ATV_SMPTE_TTML_DATA_LEN 42


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef enum {
  SMPTE_TTML_ELEMENT_TYPE_STYLE,
  SMPTE_TTML_ELEMENT_TYPE_REGION,
  SMPTE_TTML_ELEMENT_TYPE_BODY,
  SMPTE_TTML_ELEMENT_TYPE_DIV,
  SMPTE_TTML_ELEMENT_TYPE_P,
  SMPTE_TTML_ELEMENT_TYPE_SPAN,
  SMPTE_TTML_ELEMENT_TYPE_ANON_SPAN,
  SMPTE_TTML_ELEMENT_TYPE_BR
} SmpteTtmlElementType;

typedef enum {
  SMPTE_TTML_WHITESPACE_MODE_NONE,
  SMPTE_TTML_WHITESPACE_MODE_DEFAULT,
  SMPTE_TTML_WHITESPACE_MODE_PRESERVE,
} SmpteTtmlWhitespaceMode;

typedef struct
{
    /* The following fields of TTMLContext are shared between decoder and spu units */
    int                     formatId; /* 0 = bitmap, 1 = text/ass */
    int64_t                 pts;
    int                     text_direction;
    int                     font_family;
    int                     font_size;
    int                     line_height;
    int                     text_align;
    int                     color;
    int                     background_color;
    int                     font_style;
    int                     font_weight;
    int                     text_decoration;
    int                     unicode_bidi;
    int                     wrap_option;
    int                     multi_row_align;
    int                     line_padding;
    int                     origin;
    int                     extent;
    int                     display_align;
    int                     overflow;
    int                     padding;
    int                     writing_mode;
    int                     show_background;
} SmpteTtmlContext;

typedef struct
{
  int                       text_direction;
  int                       font_family;
  int                       font_size;
  int                       line_height;
  int                       text_align;
  int                       color;
  int                       background_color;
  int                       font_style;
  int                       font_weight;
  int                       text_decoration;
  int                       unicode_bidi;
  int                       wrap_option;
  int                       multi_row_align;
  int                       line_padding;
  int                       origin;
  int                       extent;
  int                       display_align;
  int                       overflow;
  int                       padding;
  int                       writing_mode;
  int                       show_background;
} SmpteTtmlStyleSet;

typedef struct
{
  SmpteTtmlElementType type;
  int id;
  int styles;
  int region;
  int64_t begin;
  int64_t end;
  SmpteTtmlStyleSet *style_set;
  int text;
  int text_index;
  SmpteTtmlWhitespaceMode whitespace_mode;
} SmpteTtmlElement;

/* Represents a static scene consisting of one or more text elements that
 * should be visible over a specific period of time. */
typedef struct
{
  int64_t begin;
  int64_t end;
  int elements;
  int buf;
} SmpteTtmlScene;

class SmpteTtmlParser: public Parser {
public:
    SmpteTtmlParser(std::shared_ptr<DataSource> source);
    virtual ~SmpteTtmlParser();
    virtual int parse();
    virtual bool updateParameter(int type, void *data);
    virtual void dump(int fd, const char *prefix);
    static inline SmpteTtmlParser *getCurrentInstance();
    void notifyCallerAvail(int avail);

private:
    int getSpu();
    int getInterSpu();
    int getTTMLSpu();
    int softDemuxParse(std::shared_ptr<AML_SPUVAR> spu);
    int hwDemuxParse(std::shared_ptr<AML_SPUVAR> spu);
    int atvHwDemuxParse(std::shared_ptr<AML_SPUVAR> spu);
    int SmpteTtmlDecodeFrame(std::shared_ptr<AML_SPUVAR> spu, char *psrc, const int size);
    void checkDebug();
    int initContext();

    void callbackProcess();
    SmpteTtmlContext *mContext;
    // control dump file or not
    int mDumpSub;
    int mIndex;
    static SmpteTtmlParser *sInstance;
    std::mutex mMutex;
    std::condition_variable mCv;
    int mPendingAction;
    std::shared_ptr<std::thread> mTimeoutThread;
};
#endif
