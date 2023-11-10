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

#ifndef __SUBTITLE_ARIB24_PARSER_H__
#define __SUBTITLE_ARIB24_PARSER_H__
#include "Parser.h"
#include "DataSource.h"
#include "SubtitleTypes.h"

#include "DvbCommon.h"

#ifdef NEED_ARIB24_LIBARIBCAPTION
#include <aribcaption/aribcaption.h>
#include <aribcaption/decoder.h>
#include <aribcaption/caption.h>
#include <aribcaption/context.hpp>
#include <aribcaption/caption.hpp>
#include <aribcaption/decoder.hpp>
#endif


#define AV_NOPTS_VALUE          INT64_C(0x8000000000000000)
#define AV_TIME_BASE            1000000
#define AV_TIME_BASE_Q          (AVRational){1, AV_TIME_BASE}
#define AVPALETTE_SIZE 1024
#define MAX_SLICES 64
#define ATV_ARIB_B24_DATA_LEN 42


/** Error code of the Arib B24 module*/
enum AribB24FlagCode
{
    ARIB_B24_FAILURE = -1,   /**< Invalid parameter*/
    ARIB_B24_SUCCESS
};

/** Error code of the Arib B24 module*/
enum AribB24LanguageCode
{
    ARIB_B24_POR = 0,
    ARIB_B24_JPN = 1,   /**< Invalid parameter*/
    ARIB_B24_SPA = 2,
    ARIB_B24_ENG = 3,
    ARIB_B24_TGL = 4,
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct
{
    /* The following fields of AribB24Context are shared between decoder and spu units */
    int                      formatId; /* 0 = bitmap, 1 = text/ass */
    int64_t                  pts;
    int                      languageCode;
    //unsigned                 i_render_area_width;
    //unsigned                 i_render_area_height;

    int                      i_cfg_rendering_backend;
    char*                    psz_cfg_font_name;
    bool                     b_cfg_replace_drcs;
    bool                     b_cfg_force_stroke_text;
    bool                     b_cfg_ignore_background;
    bool                     b_cfg_ignore_ruby;
    bool                     b_cfg_fadeout;
    float                    f_cfg_stroke_width;

    //int frame_area_width = 3840;
    //int frame_area_height = 2160;
    //int margin_left = 0;
    //int margin_top = 0;
    //int margin_right = 0;
    //int margin_bottom = 0;


} AribB24Context;

class AribB24Parser: public Parser {
public:
    #ifdef NEED_ARIB24_LIBARIBCAPTION
    explicit AribB24Parser() : aribcaptionDecoder(aribcaptionContext){};
    #endif
    AribB24Parser(std::shared_ptr<DataSource> source);
    virtual ~AribB24Parser();
    virtual int parse();
    virtual bool updateParameter(int type, void *data);
    virtual void dump(int fd, const char *prefix);
    void notifyCallerAvail(int avail);

private:
    int getSpu();
    int getInterSpu();
    int getAribB24Spu();
    int softDemuxParse(std::shared_ptr<AML_SPUVAR> spu);
    int hwDemuxParse(std::shared_ptr<AML_SPUVAR> spu);
    int atvHwDemuxParse(std::shared_ptr<AML_SPUVAR> spu);
    int aribB24DecodeFrame(std::shared_ptr<AML_SPUVAR> spu, char *psrc, const int size);
    void checkDebug();
    //int saveResult2Spu(std::shared_ptr<AML_SPUVAR> spu, arib_parser_t *p_parser, arib_decoder_t *p_decoder);
    int initContext();

    void callbackProcess();
    AribB24Context *mContext;
    // control dump file or not
    int mDumpSub;
    int mIndex;
    std::mutex mMutex;
    std::condition_variable mCv;
    int mPendingAction;
    std::shared_ptr<std::thread> mTimeoutThread;
    #ifdef NEED_ARIB24_LIBARIBCAPTION
    aribcaption::Decoder      aribcaptionDecoder;
    aribcaption::Context      aribcaptionContext;
    aribcaption::DecodeResult aribcaptionResult;
    #endif
};


#endif
