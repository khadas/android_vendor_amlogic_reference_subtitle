/*
* Copyright (c) 2014 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package.
*
* Description: h file
*/

#ifndef __SUBTITLE_TTML_PARSER_H__
#define __SUBTITLE_TTML_PARSER_H__
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
#define ATV_TTML_DATA_LEN 42


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef enum {
  TTML_ELEMENT_TYPE_STYLE,
  TTML_ELEMENT_TYPE_REGION,
  TTML_ELEMENT_TYPE_BODY,
  TTML_ELEMENT_TYPE_DIV,
  TTML_ELEMENT_TYPE_P,
  TTML_ELEMENT_TYPE_SPAN,
  TTML_ELEMENT_TYPE_ANON_SPAN,
  TTML_ELEMENT_TYPE_BR
} TtmlElementType;

typedef enum {
  TTML_WHITESPACE_MODE_NONE,
  TTML_WHITESPACE_MODE_DEFAULT,
  TTML_WHITESPACE_MODE_PRESERVE,
} TtmlWhitespaceMode;

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
} TtmlContext;

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
} TtmlStyleSet;

typedef struct
{
  TtmlElementType type;
  int id;
  int styles;
  int region;
  int64_t begin;
  int64_t end;
  TtmlStyleSet *style_set;
  int text;
  int text_index;
  TtmlWhitespaceMode whitespace_mode;
} TtmlElement;

/* Represents a static scene consisting of one or more text elements that
 * should be visible over a specific period of time. */
typedef struct
{
  int64_t begin;
  int64_t end;
  int elements;
  int buf;
} TtmlScene;

class TtmlParser: public Parser {
public:
    TtmlParser(std::shared_ptr<DataSource> source);
    virtual ~TtmlParser();
    virtual int parse();
    virtual bool updateParameter(int type, void *data);
    virtual void dump(int fd, const char *prefix);
    static inline TtmlParser *getCurrentInstance();
    void notifyCallerAvail(int avail);

private:
    int getSpu();
    int getInterSpu();
    int getTTMLSpu();
    int softDemuxParse(std::shared_ptr<AML_SPUVAR> spu);
    int hwDemuxParse(std::shared_ptr<AML_SPUVAR> spu);
    int atvHwDemuxParse(std::shared_ptr<AML_SPUVAR> spu);
    int TtmlDecodeFrame(std::shared_ptr<AML_SPUVAR> spu, char *psrc, const int size);
    void checkDebug();
    int initContext();

    void callbackProcess();
    TtmlContext *mContext;
    // control dump file or not
    int mDumpSub;
    int mIndex;
    static TtmlParser *sInstance;
    std::mutex mMutex;
    std::condition_variable mCv;
    int mPendingAction;
    std::shared_ptr<std::thread> mTimeoutThread;
};


#endif
