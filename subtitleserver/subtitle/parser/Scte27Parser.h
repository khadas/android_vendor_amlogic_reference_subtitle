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

#ifndef __SUBTITLE_SCTE27_PARSER_H__
#define __SUBTITLE_SCTE27_PARSER_H__
#include "Parser.h"
#include "DataSource.h"
#include "SubtitleTypes.h"

#include "DvbCommon.h"

#define BUFFER_W 720
#define BUFFER_H 480
#define VIDEO_W BUFFER_W
#define VIDEO_H BUFFER_H

#define DEMUX_ID 2
#define BITMAP_PITCH BUFFER_W*4

#define SCTE27_TID 0xC6

#define DISPLAY_STD_720_480_30       0
#define DISPLAY_STD_720_576_25       1
#define DISPLAY_STD_1280_720_60      2
#define DISPLAY_STD_1920_1080_60     3
#define DISPLAY_STD_RESERVE         -1

#define SUB_TYPE_SIMPLE_BITMAP       1

#define SUB_NODE_NOT_DISPLAYING      0
#define SUB_NODE_DISPLAYING          1

#define PTS_PER_SEC                  90000

#define STUFF                        0 //Stuff
#define EOL                          1 //EOL
#define RESERVED_1                   2 //Reserved
#define RESERVED_2                   3 //Reserved

#define BMP_OUTLINE_NONE             0
#define BMP_OUTLINE_OUTLINE          1
#define BMP_OUTLINE_DROPSHADOW       2
#define BMP_OUTLINE_RESERVED         3

#define PIXEL_BG                     0 //Background
#define PIXEL_CH                     1 //Character
#define PIXEL_OL                     2 //Outline
#define PIXEL_DS                     3 //Dropshadow

#define OUTLINE_STYLE_NONE           0
#define OUTLINE_STYLE_OL             1
#define OUTLINE_STYLE_DS             2

#define DISPLAY_STATUS_INIT 0
#define DISPLAY_STATUS_SHOW 1

#define DISPLAY_FREERUN_THRESHOLD    200 * PTS_PER_SEC

#define set_rgba(x, y) \
{ \
    (x)[0] = (y >> 24) & 0xFF; \
    (x)[1] = (y >> 16) & 0xFF; \
    (x)[2] = (y >> 8) & 0xFF; \
    (x)[3] = y & 0xFF; \
}

static int bean_bits;
static int bean_bytes;
static int bean_total;
static uint8_t *bean_buffer;

static void init_beans_separator(uint8_t* buffer, int size)
{
    bean_total = size;
    bean_buffer = (uint8_t*)buffer;
    bean_bytes = 0;
    bean_bits = 0;
}

static int get_beans(int* value, int bits)
{
    int beans;
    int a, b;

    if (bits > 8)
        return -1;

    if (bits + bean_bits < 8)
    {
        beans = (bean_buffer[bean_bytes] >> (8 - bits -bean_bits)) & ((1<< bits) -1);
        bean_bits += bits;
        *value = beans;
    }
    else
    {
        a = bits + bean_bits - 8;
        b = bits - a;
        beans = ((bean_buffer[bean_bytes] & ((1<<b) -1)) << a)|
            ((bean_buffer[bean_bytes+1] >> (8 - a)) & ((1<<a) -1));

        bean_bytes++;
        bean_bits = bits + bean_bits - 8;
        if ((bean_bytes > bean_total) || (bean_bytes == bean_total && bean_bits > 0))
        {
            SUBTITLE_LOGI("buffer overflow, analyze error");
            return -1;
        }
        *value = beans;
    }

    return 1;
}

#define assert_bits(x, y) \
{ \
    int z; \
    z = get_beans(&x, y); \
    if (z <= 0) \
    { \
        SUBTITLE_LOGI("left bits < 0, total %d byte %d bits %d", bean_total, bean_bytes, bean_bits); \
        break; \
    } \
}

enum AM_SCTE27_ERRORCODE
{
    AM_SCTE27_ERROR_BASE=ERROR_BASE(AM_MOD_SCTE27),
    AM_SCTE27_ERROR_INVALID_PARAM,                      /** Invalid parameter*/
    AM_SCTE27_ERROR_INVALID_HANDLE,                     /** Invalid handle*/
    AM_SCTE27_ERROR_NOT_SUPPORTED,                      /** not support action*/
    AM_SCTE27_ERROR_CREATE_DECODE,                      /** open subtitle decode error*/
    AM_SCTE27_ERROR_SET_BUFFER,                         /** set pes buffer error*/
    AM_SCTE27_ERROR_NO_MEM,                             /** out of memmey*/
    AM_SCTE27_ERROR_CANNOT_CREATE_THREAD,               /** cannot creat thread*/
    AM_SCTE27_ERROR_NOT_RUN,                            /** thread run error*/
    AM_SCTE27_INIT_DISPLAY_FAILED,                      /** init display error*/
    AM_SCTE27_PACKET_INVALID,
    AM_SCTE27_ERROR_END
};

enum {
    COLOR_FRAME,
    COLOR_CHARACTER,
    COLOR_OUTLINE,
    COLOR_SHADOW,
};

struct scte_simple_bitmap_type
{
    uint8_t                     is_framed;
    uint8_t                     outline_style;
    uint32_t                    character_color;
    uint32_t                    character_color_rgba;
    uint32_t                    top_h;
    uint32_t                    top_v;
    uint32_t                    bottom_h;
    uint32_t                    bottom_v;
    struct frame_bg_style_s
    {
        uint32_t frame_top_h;
        uint32_t frame_top_v;
        uint32_t frame_bottom_h;
        uint32_t frame_bottom_v;
        uint32_t frame_color;
        uint32_t frame_color_rgba;
    } frame_bg_style;
    union{
        struct outlined_s
        {
            uint8_t    outline_thickness;
            uint32_t   outline_color;
            uint32_t   outline_color_rgba;
        } outlined;
        struct drop_shadow_s
        {
            uint8_t    shadow_right;
            uint8_t    shadow_bottom;
            uint32_t   shadow_color;
            uint32_t   shadow_color_rgba;
        } drop_shadow;
    } style_para;
    uint32_t                    bitmap_length;
    void*                       sub_bmp;
};

struct Scte27SubContext {
    int                 table_ext;
    int                 curr_no;
    int                 last_no;
    int                 seg_size;
    int                 has_segment;
    char                *segment_container;
    int                 scte27_subtitle_type;
    uint8_t             display_std;
    uint8_t             pre_clear;
    uint8_t             immediate;
    char                lang[4];
    uint8_t             display_status;
    int                 block_len;
    uint32_t            msg_crc;
    void*               descriptor; //Not used
    unsigned int lastSpuOriginDisplayW;
    unsigned int lastSpuOriginDisplayH;
    void dump(int fd, const char *prefix) {
    }
};


class Scte27Parser: public Parser {
public:
    Scte27Parser(std::shared_ptr<DataSource> source);
    virtual ~Scte27Parser();
    virtual int parse();
    virtual bool updateParameter(int type, void *data);
    virtual void dump(int fd, const char *prefix);
    void notifyCallerAvail(int avail);

private:
    int getSpu();
    int getInterSpu();
    int getScte27Spu();

    void decodeBitmap(std::shared_ptr<AML_SPUVAR> spu, uint8_t* buffer, int size, int bitmap_width, int bitmap_height, int pitch);
    int decodeMessageBodySubtitle(std::shared_ptr<AML_SPUVAR> spu, char *pSrc, const int size);
    int decodeSubtitle(std::shared_ptr<AML_SPUVAR> spu, char *psrc, const int size);

    int softDemuxParse(std::shared_ptr<AML_SPUVAR> spu, char *psrc, const int size);
    int hwDemuxParse(std::shared_ptr<AML_SPUVAR> spu, char *psrc, const int size);

    void notifySubtitleDimension(int width, int height);
    void notifySubtitleErrorInfo(int error);
    void cleanSegment();

    void checkDebug();
    int initContext();

    Scte27SubContext *mContext;

    int mDumpSub;

    //CV for notification.
    std::mutex mMutex;
    std::condition_variable mCv;
    int mPendingAction;
    std::shared_ptr<std::thread> mTimeoutThread;
};


#endif

