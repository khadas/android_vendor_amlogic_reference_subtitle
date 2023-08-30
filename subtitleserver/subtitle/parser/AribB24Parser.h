/*
* Copyright (c) 2014 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package.
*
* Description: h file
*/

#ifndef __SUBTITLE_ARIB24_PARSER_H__
#define __SUBTITLE_ARIB24_PARSER_H__
#include "Parser.h"
#include "DataSource.h"
#include "sub_types.h"

#include "dvbCommon.h"

#include <aribcaption/aribcaption.h>
#include <aribcaption/decoder.h>
#include <aribcaption/caption.h>
#include <aribcaption/context.hpp>
#include <aribcaption/caption.hpp>
#include <aribcaption/decoder.hpp>


#define AV_NOPTS_VALUE          INT64_C(0x8000000000000000)
#define AV_TIME_BASE            1000000
#define AV_TIME_BASE_Q          (AVRational){1, AV_TIME_BASE}
#define AVPALETTE_SIZE 1024
#define MAX_SLICES 64
#define ATV_ARIB_B24_DATA_LEN 42


/**\brief Error code of the Arib B24 module*/
enum AribB24FlagCode
{
    ARIB_B24_FAILURE = -1,   /**< Invalid parameter*/
    ARIB_B24_SUCCESS
};

/**\brief Error code of the Arib B24 module*/
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
    explicit AribB24Parser() : aribcaptionDecoder(aribcaptionContext){};
    AribB24Parser(std::shared_ptr<DataSource> source);
    virtual ~AribB24Parser();
    virtual int parse();
    virtual bool updateParameter(int type, void *data);
    virtual void dump(int fd, const char *prefix);
    static inline AribB24Parser *getCurrentInstance();
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
    static AribB24Parser *sInstance;
    std::mutex mMutex;
    std::condition_variable mCv;
    int mPendingAction;
    std::shared_ptr<std::thread> mTimeoutThread;
    aribcaption::Decoder      aribcaptionDecoder;
    aribcaption::Context      aribcaptionContext;
    aribcaption::DecodeResult aribcaptionResult;
};


#endif
