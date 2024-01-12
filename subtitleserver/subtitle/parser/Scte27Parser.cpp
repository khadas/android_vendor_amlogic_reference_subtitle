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

#define LOG_TAG "Scte27Parser"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string>
#include <list>
#include <thread>
#include <algorithm>
#include <functional>
#include <mutex>
#include <math.h>

#include "SubtitleLog.h"

#include "StreamUtils.h"
#include "SubtitleTypes.h"
#include "Scte27Parser.h"
#include "ParserFactory.h"

#define DATA_SIZE 10*1024
#define OSD_HALF_SIZE (1920*1280/8)
#define HIGH_32_BIT_PTS 0xFFFFFFFF
#define TSYNC_32_BIT_PTS 0xFFFFFFFF

static void save2BitmapFile(const char *filename, uint32_t *bitmap, int w, int h) {
    SUBTITLE_LOGI("save2BitmapFile:%s\n",filename);
    FILE *f;
    char fname[50];
    snprintf(fname, sizeof(fname), "%s.ppm", filename);
    f = fopen(fname, "w");
    if (!f) {
        SUBTITLE_LOGE("Error cannot open file %s!", fname);
        return;
    }
    fprintf(f, "P6\n" "%d %d\n" "%d\n", w, h, 255);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int v = bitmap[y * w + x];
            putc((v >> 16) & 0xff, f);
            putc((v >> 8) & 0xff, f);
            putc((v >> 0) & 0xff, f);
        }
    }
    fclose(f);
}

bool static inline isMore32Bit(int64_t pts) {
    if (((pts >> 32) & HIGH_32_BIT_PTS) > 0) {
        return true;
    }
    return false;
}

static uint32_t subtitle_color_to_bitmap_color(uint32_t yCbCr) {
    uint8_t R,G,B,A,Y,Cr,Cb,opaque_enable;
    uint32_t bitmap_color;
    char subtitle_color[2];

    subtitle_color[0] = (yCbCr >> 8) & 0xFF;
    subtitle_color[1] = yCbCr & 0xFF;

    Y = (subtitle_color[0] >> 3) & 0x1F;
    opaque_enable = (subtitle_color[0] >> 2) & 1;
    Cr = ((subtitle_color[0] & 0x3) << 3) | ((subtitle_color[1] >> 5) & 0x7);
    Cb = subtitle_color[1] & 0x1F;

    if (opaque_enable)
        A = 0xFF;
    else
        A = 0x80;

    Y *= 8;
    Cr *= 8;
    Cb *= 8;

    R = Y + 1.4075 * (Cr - 128);
    G = Y - 0.3455 * (Cb - 128) - 0.7169 * (Cr - 128);
    B = Y + 1.779 * (Cb - 128);
    // bitmap_color = (R << 24) | (G << 16) | (B << 8) | A;
    bitmap_color = (A << 24) | (R << 16) | (G << 8) | B;
    SUBTITLE_LOGI("YUV 0x%x R 0x%x G 0x%x B 0x%x, RGBA 0x%x",
            yCbCr, R, G, B, bitmap_color);
    return bitmap_color;
}

void Scte27Parser::notifyCallerAvail(int avil) {
    if (mNotifier != nullptr) {
        mNotifier->onSubtitleAvailable(avil);
    }
}

int Scte27Parser::initContext() {
    mContext = new Scte27SubContext();
    mContext->segment_container  = (char *)malloc(DATA_SIZE);
    memset(mContext->segment_container, 0, DATA_SIZE);
    return 0;
}

void Scte27Parser::checkDebug() {
       //dump subtitle bitmap
       char value[PROPERTY_VALUE_MAX] = {0};
       memset(value, 0, PROPERTY_VALUE_MAX);
       property_get("vendor.subtitle.dump", value, "false");
       if (!strcmp(value, "true")) {
           mDumpSub = true;
       }
}

void Scte27Parser::notifySubtitleDimension(int width, int height) {
    if (mNotifier != nullptr) {
        if ((mContext->lastSpuOriginDisplayW != width) || (mContext->lastSpuOriginDisplayH != height)) {
            SUBTITLE_LOGI("notifySubtitleDimension: last wxh:(%d x %d), now wxh:(%d x %d)",
                mContext->lastSpuOriginDisplayW, mContext->lastSpuOriginDisplayH, width, height);
            mNotifier->onSubtitleDimension(width, height);
            mContext->lastSpuOriginDisplayW = width;
            mContext->lastSpuOriginDisplayH = height;
        }
    }
}

void Scte27Parser::notifySubtitleErrorInfo(int error) {
    if (mNotifier != nullptr) {
        SUBTITLE_LOGI("notifySubtitleErrorInfo: %d", error);
        mNotifier->onSubtitleAvailable(error);
    }
}

void Scte27Parser::cleanSegment() {
    memset(mContext->segment_container, 0, DATA_SIZE);
    mContext->table_ext = 0;
    mContext->curr_no = 0;
    mContext->last_no = 0;
    mContext->seg_size = 0;
    mContext->has_segment = 0;
}

Scte27Parser::Scte27Parser(std::shared_ptr<DataSource> source) {
    mContext = nullptr;
    mDataSource = source;
    mParseType = TYPE_SUBTITLE_SCTE27;
    mPendingAction = -1;
    initContext();
    checkDebug();
}

Scte27Parser::~Scte27Parser() {
    SUBTITLE_LOGI("%s", __func__);
    mState = SUB_STOP;
    if (mContext != NULL) {
        free(mContext->segment_container);
        delete mContext;
        mContext = nullptr;
    }

    {   //To TimeoutThread: wakeup! we are exiting...
        std::unique_lock<std::mutex> autolock(mMutex);
        mPendingAction = 1;
        mCv.notify_all();
    }
}

bool Scte27Parser::updateParameter(int type, void *data) {
    if (TYPE_SUBTITLE_SCTE27 == type) {
        Scte27Param *pScte27Param = (Scte27Param* )data;
    }
    return true;
}

void Scte27Parser::decodeBitmap(std::shared_ptr<AML_SPUVAR> spu, uint8_t* buffer, int size, int bitmap_width, int bitmap_height, int pitch) {
    uint32_t frame_duration;

    if (!buffer) {
        SUBTITLE_LOGE("%s buffer is null", __FUNCTION__);
        return;
    }

    scte_simple_bitmap_type *simple_bitmap;
    simple_bitmap = (scte_simple_bitmap_type*)malloc(sizeof(scte_simple_bitmap_type));
    if (!simple_bitmap) {
        SUBTITLE_LOGE("%s simple_bitmap is null", __FUNCTION__);
        return;
    }

    memset(simple_bitmap, 0, sizeof(scte_simple_bitmap_type));
    simple_bitmap->is_framed = buffer[0] & (1<<2);
    simple_bitmap->outline_style = buffer[0] & 0x3;
    simple_bitmap->character_color = (buffer[1] << 8) | buffer[2];
    simple_bitmap->character_color_rgba = subtitle_color_to_bitmap_color(simple_bitmap->character_color);
    simple_bitmap->top_h = (buffer[3] << 4) | ((buffer[4] >> 4) & 0xF);
    simple_bitmap->top_v = ((buffer[4] & 0xF) << 8) | buffer[5];
    simple_bitmap->bottom_h = (buffer[6] << 4) | ((buffer[7] >> 4) & 0xF);
    simple_bitmap->bottom_v = ((buffer[7] & 0xF) << 8) | buffer[8];

    if (simple_bitmap->top_h >= simple_bitmap->bottom_h || simple_bitmap->top_v >= simple_bitmap->bottom_v) {
        free (simple_bitmap);
        SUBTITLE_LOGE("%s top_h:%d bottom_h:%d top_v:%d bottom_v:%d", __FUNCTION__,simple_bitmap->top_h, simple_bitmap->bottom_h, simple_bitmap->top_v, simple_bitmap->bottom_v);
        return;
    }
    simple_bitmap->frame_bg_style.frame_top_h = simple_bitmap->top_h;
    simple_bitmap->frame_bg_style.frame_top_v = simple_bitmap->top_v;
    simple_bitmap->frame_bg_style.frame_bottom_h = simple_bitmap->bottom_h;
    simple_bitmap->frame_bg_style.frame_bottom_v = simple_bitmap->bottom_v;

    buffer = &buffer[9];

    if (simple_bitmap->is_framed) {
        simple_bitmap->frame_bg_style.frame_top_h = (buffer[0] << 4) | ((buffer[1] >> 4) & 0xF);
        simple_bitmap->frame_bg_style.frame_top_v = ((buffer[1] & 0xF) << 8) | buffer[2];
        simple_bitmap->frame_bg_style.frame_bottom_h = (buffer[3] << 4) | ((buffer[4] >> 4) & 0xF);
        simple_bitmap->frame_bg_style.frame_bottom_v = ((buffer[4] & 0xF) << 8) | buffer[5];
        simple_bitmap->frame_bg_style.frame_color = (buffer[6] << 8) | buffer[7];
        simple_bitmap->frame_bg_style.frame_color_rgba = subtitle_color_to_bitmap_color(simple_bitmap->frame_bg_style.frame_color);
        if (simple_bitmap->frame_bg_style.frame_top_h > simple_bitmap->top_h ||
            simple_bitmap->frame_bg_style.frame_top_v > simple_bitmap->top_v ||
            simple_bitmap->frame_bg_style.frame_bottom_h < simple_bitmap->bottom_h ||
            simple_bitmap->frame_bg_style.frame_bottom_v < simple_bitmap->bottom_v) {
            free (simple_bitmap);
            SUBTITLE_LOGE("%s top_h:%d top_v:%d bottom_h:%d bottom_v:%d  frame_top_h:%d frame_top_v:%d frame_bottom_h:%d frame_bottom_v:%d",
                __FUNCTION__,
                simple_bitmap->top_h,
                simple_bitmap->top_v,
                simple_bitmap->bottom_h,
                simple_bitmap->bottom_v,
                simple_bitmap->frame_bg_style.frame_top_h,
                simple_bitmap->frame_bg_style.frame_top_v,
                simple_bitmap->frame_bg_style.frame_bottom_h,
                simple_bitmap->frame_bg_style.frame_bottom_v);
            return;
        }
        buffer = &buffer[8];
    }

    switch (simple_bitmap->outline_style) {
        case BMP_OUTLINE_OUTLINE:
            simple_bitmap->style_para.outlined.outline_thickness = buffer[0] & 0xF;
            simple_bitmap->style_para.outlined.outline_color = (buffer[1] << 8) | buffer[2];
            simple_bitmap->style_para.outlined.outline_color_rgba = subtitle_color_to_bitmap_color(simple_bitmap->style_para.outlined.outline_color);
            buffer = &buffer[3];
            break;
        case BMP_OUTLINE_DROPSHADOW:
            simple_bitmap->style_para.drop_shadow.shadow_right = (buffer[0] >> 4) & 0xF;
            simple_bitmap->style_para.drop_shadow.shadow_bottom = buffer[0] & 0xF;
            simple_bitmap->style_para.drop_shadow.shadow_color = (buffer[1] << 8) | buffer[2];
            simple_bitmap->style_para.drop_shadow.shadow_color_rgba = subtitle_color_to_bitmap_color(simple_bitmap->style_para.drop_shadow.shadow_color);
            buffer = &buffer[3];
            break;
        case BMP_OUTLINE_RESERVED:
            buffer = &buffer[4];
            break;
        case BMP_OUTLINE_NONE:
        default:
            break;
    }



    int bitmap_size,  bitmap_h, bitmap_v;
    uint8_t *bitmap;
    bitmap_h = simple_bitmap->bottom_h - simple_bitmap->top_h;
    bitmap_v = simple_bitmap->bottom_v - simple_bitmap->top_v;
    bitmap_size = bitmap_h * bitmap_v;
    simple_bitmap->bitmap_length = (buffer[0] << 8) | buffer[1];
    SUBTITLE_LOGI("size %d bmp_size %d", bitmap_size, simple_bitmap->bitmap_length);

    bitmap = (uint8_t*)malloc(bitmap_size);
    if (!bitmap) {
        SUBTITLE_LOGE("%s bitmap is null", __FUNCTION__);
        free (simple_bitmap);
        return;
    }
    simple_bitmap->sub_bmp = bitmap;

    int on_bits, off_bits, row_length;
    uint8_t *row_start, *row_cursor;
    int value = 0,row_count = 0;
    int i, j;

    init_beans_separator(&buffer[2], simple_bitmap->bitmap_length);
    row_start = bitmap;
    row_cursor = row_start;
    row_length = 0;
    while (1) {
        assert_bits(value, 1);
        if (value != 0) {
            //1xxxYYYYY
            assert_bits(on_bits, 3);
            if (on_bits == 0) on_bits = 8;
            for (i=0; i<on_bits; i++) {
                *(row_cursor++) = PIXEL_CH;
            }

            assert_bits(off_bits, 5);
            if (off_bits == 0) off_bits = 32;
            for (i=0; i<off_bits; i++) {
                *(row_cursor++) = PIXEL_BG;
            }
            row_length += (off_bits + on_bits);
        } else {
            assert_bits(value, 1);
            if (value != 0) {
                //01XXXXXX
                assert_bits(off_bits, 6);
                if (off_bits == 0) off_bits = 64;
                for (i=0; i<off_bits; i++) {
                    *(row_cursor++) = PIXEL_BG;
                }
                row_length += off_bits;
            } else {
                assert_bits(value, 1);
                if (value != 0) {
                    //001XXXX
                    assert_bits(on_bits, 4);
                    if (on_bits == 0) on_bits = 16;
                    for (i=0; i<on_bits; i++) {
                        *(row_cursor++) = PIXEL_CH;
                    }
                    row_length += on_bits;
                } else {
                    assert_bits(value, 2);
                    switch (value) {
                        case STUFF: //Stuff
                            SUBTITLE_LOGI("stuffing code");
                            break;
                        case EOL: //EOL
                            if (row_length < bitmap_h) {
                                for (i=0; i<bitmap_h - row_length; i++) {
                                    *(row_cursor++) = PIXEL_BG;
                                }
                            }
                            row_start += bitmap_h;
                            row_cursor = row_start;
                            row_length = 0;
                            row_count++;
                            break;
                        case RESERVED_1: //Reserved
                        case RESERVED_2: //Reserved
                            SUBTITLE_LOGI("reserved %d", value);
                            break;
                        default:
                            SUBTITLE_LOGI("can not be here");
                            break;
                    };
                    if (row_count >= bitmap_v)
                        break;
                }
            }
        }
    }
    //Outline

    if (simple_bitmap->outline_style == OUTLINE_STYLE_OL) {
        int thickness = simple_bitmap->style_para.outlined.outline_thickness;
        for (i=0; i<bitmap_v; i++) {
            for (j=0; j<bitmap_h; j++) {
                if (bitmap[i*bitmap_h + j] == PIXEL_CH) {
                    int outline_i, outline_j;
                    for (outline_i = i - thickness; outline_i <= i + thickness; outline_i++) {
                        for (outline_j = j -thickness; outline_j <= j + thickness; outline_j++) {
                            int w = abs(outline_i - i);
                            int h = abs(outline_j - j);
                            if (sqrt(w*w + h*h) <= thickness) {
                                if (bitmap[outline_i*bitmap_h + outline_j] == PIXEL_BG) {
                                    bitmap[outline_i*bitmap_h + outline_j] = PIXEL_OL;
                                }
                            }
                        }
                    }
                }
            }
        }
    } else if (simple_bitmap->outline_style == OUTLINE_STYLE_DS) {
        int offset_right = simple_bitmap->style_para.drop_shadow.shadow_right;
        int offset_below = simple_bitmap->style_para.drop_shadow.shadow_bottom;
        for (i=0; i<bitmap_v; i++) {
            for (j=0; j<bitmap_h; j++) {
                if (bitmap[i*bitmap_h + j] == PIXEL_CH) {
                    int target_ds = (i + offset_below) * bitmap_h + j + offset_right;
                    if (bitmap[target_ds] == PIXEL_BG) {
                        bitmap[target_ds] = PIXEL_DS;
                    }
                }
            }
        }
    }


    uint8_t *bmp, *target_bmp_start, *cursor;
    bmp = (uint8_t *) malloc(bitmap_width*bitmap_height*4);
    target_bmp_start = bmp + simple_bitmap->top_v * pitch + simple_bitmap->top_h * 4;

    for (i=0; i<bitmap_v; i++) {
        cursor = target_bmp_start + i * pitch;
        for (j=0; j<bitmap_h; j++) {
            switch (bitmap[i*bitmap_h + j]) {
                case PIXEL_BG: //Bg
                    if (simple_bitmap->is_framed)
                        set_rgba(cursor + j * 4, simple_bitmap->frame_bg_style.frame_color_rgba);
                    break;
                case PIXEL_CH: //Font
                    set_rgba(cursor + j * 4, simple_bitmap->character_color_rgba);
                    break;
                case PIXEL_OL: // Outline
                    set_rgba(cursor + j * 4, simple_bitmap->style_para.outlined.outline_color_rgba);
                    break;
                case PIXEL_DS: // Drop shadow
                    set_rgba(cursor + j * 4, simple_bitmap->style_para.drop_shadow.shadow_color_rgba);
                    break;
                default:
                    SUBTITLE_LOGI("render should not be here");
                    break;
            }
        }
    }
    spu->spu_data = (unsigned char *)malloc(bitmap_width*bitmap_height*4);
    if (!spu->spu_data) {
        SUBTITLE_LOGE("%s malloc SCTE27_SUB_SIZE failed!\n",__FUNCTION__);
        free (bmp);
        free (bitmap);
        free (simple_bitmap);
        return;
    }
    memset(spu->spu_data, 0, bitmap_width*bitmap_height*4);
    memcpy(spu->spu_data, bmp, bitmap_width*bitmap_height*4);
    SUBTITLE_LOGI("%s top_h:%d top_v:%d bottom_h:%d bottom_v:%d frame_top_h:%d frame_top_v:%d frame_bottom_h:%d frame_bottom_v:%d frame_color:0x%x outline_style:%d character_color:0x%x bitmap_length:%d",
        __FUNCTION__,
        simple_bitmap->top_h,
        simple_bitmap->top_v,
        simple_bitmap->bottom_h,
        simple_bitmap->bottom_v,
        simple_bitmap->frame_bg_style.frame_top_h,
        simple_bitmap->frame_bg_style.frame_top_v,
        simple_bitmap->frame_bg_style.frame_bottom_h,
        simple_bitmap->frame_bg_style.frame_bottom_v,
        simple_bitmap->frame_bg_style.frame_color,
        simple_bitmap->outline_style,
        simple_bitmap->character_color,
        simple_bitmap->bitmap_length);
    if (!spu->spu_data) {
        SUBTITLE_LOGE("%s malloc SCTE27_SUB_SIZE failed!\n",__FUNCTION__);
        free (bmp);
        free (bitmap);
        free (simple_bitmap);
        return;
    }
}

int Scte27Parser::decodeMessageBodySubtitle(std::shared_ptr<AML_SPUVAR> spu, char *pSrc, const int size) {
    uint8_t *buf = (uint8_t *)pSrc;
    int bufSize = size;
    const uint8_t *p, *pEnd;
    int segmentType;
    int pageId;
    int segmentLength;
    int dataSize;
    int totalObject = 0;
    int total_RegionSegment = 0;

    uint8_t *bitmap = NULL;
    int width;
    int height;
    int video_width;
    int video_height;
    char read_buff[64];
    int lang_valid = -1;
    int i;

    memcpy(mContext->lang, buf, 3);
    for (i=0; i<3; i++)
    {
        if ((mContext->lang[i] >= 'a' && mContext->lang[i] <= 'z') ||
            (mContext->lang[i] >= 'A' && mContext->lang[i] <= 'Z'))
        {
            lang_valid = 0;
        }
        else
        {
            lang_valid = -1;
            break;
        }
    }


    bool pre_clear_display  = buf[3] & 0x80;
    int immediate           = buf[3] & 0x40;
    int display_standard    = buf[3] & 0x1F;
    int subtitle_type       = (buf[8] >> 4) & 0xF;
    int vlc_subtitle_type   = buf[8] >> 4;
    int block_length        = (buf[10] << 8) | buf[11];
    int64_t reveal_pts      = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
    reveal_pts = (reveal_pts < 0) ? -reveal_pts : reveal_pts;
    int display_duration    = ((buf[8] & 0x07) << 8) | buf[9];

    SUBTITLE_LOGI("%s pre_clear_display %d immediate %d display_standard:%d subtitle_type:%d vlc_subtitle_type:%d block_length:%d reveal_pts :%lld display_duration:%d size:%d", __FUNCTION__, pre_clear_display, immediate, display_standard, subtitle_type, vlc_subtitle_type, block_length, reveal_pts, display_duration, size);

    if (block_length < 12 || block_length > size) {
        SUBTITLE_LOGE("%s sub_node->block_len invalid blen %x size %x", __FUNCTION__, block_length, size);
        return -1;
    }

    video_width = VideoInfo::Instance()->getVideoWidth();
    video_height = VideoInfo::Instance()->getVideoHeight();

    int frame_duration;
    switch (display_standard)
    {
        case DISPLAY_STD_720_480_30:
            width = 720;
            height = 480;
            frame_duration = 3000;
            break;
        case DISPLAY_STD_720_576_25:
            //frame_rate = 25; //Protocol says 25
            width = 720;
            height = 576;
            frame_duration = 3600;
            break;
        case DISPLAY_STD_1280_720_60:
            width = 1280;
            height = 720;
            frame_duration = 1500;
            break;
        case DISPLAY_STD_1920_1080_60:
            width = 1920;
            height = 1080;
            frame_duration = 1500;
            break;
        default:
            width = video_width;
            height = video_height;
            frame_duration = 3600;
            SUBTITLE_LOGI("display standard error");
            break;
    }

    if (!pre_clear_display) SUBTITLE_LOGI("%s SCTE-27 subtitles without pre_clear_display flag are not well supported", __FUNCTION__);

    bitmap = &buf[12];

    if (subtitle_type == SUB_TYPE_SIMPLE_BITMAP) {
        decodeBitmap(spu, bitmap, block_length, width, height, width*4);
    }
    //memset(spu->spu_data, 0, width*height*4);
    //memcpy(spu->spu_data,ctx->buffer, width*height*4);
    //ANSI_SCTE_27_2011.pdf5.9 Display Modes
    if (immediate) {
        spu->isImmediatePresent = true;
    } else {
        spu->isImmediatePresent = false;
    }
    spu->spu_width = width;
    spu->spu_height = height;
    spu->spu_origin_display_w = width;
    spu->spu_origin_display_h = height;
    spu->buffer_size = width * height * 4;
    spu->sync_bytes = AML_PARSER_SYNC_WORD;
    spu->subtitle_type = TYPE_SUBTITLE_SCTE27;
    spu->pts = reveal_pts;
    spu->m_delay = reveal_pts + display_duration + frame_duration;
    SUBTITLE_LOGI("%s spu->spu_width:%d, spu->spu_height:%d, spu->spu_origin_display_w:%d, spu->spu_origin_display_h:%d, spu->buffer_size:%d, spu->sync_bytes:%d, spu->subtitle_type:%d, spu->pts:%lld, spu->m_delay:%lld", __FUNCTION__, spu->spu_width, spu->spu_height, spu->spu_origin_display_w, spu->spu_origin_display_h, spu->buffer_size, spu->sync_bytes, spu->subtitle_type, spu->pts, spu->m_delay);
    if (mDumpSub) {
        char filename[64];
        snprintf(filename, sizeof(filename), "./data/subtitleDump/Scte27(%lld)", spu->pts);
        save2BitmapFile(filename, (uint32_t *)spu->spu_data, spu->spu_width, spu->spu_height);
    }
    return 0;
}


int Scte27Parser::decodeSubtitle(std::shared_ptr<AML_SPUVAR> spu, char *psrc, const int size) {
    int segmentation_overlay_included;
    int section_length;
    int protocol_version;
    int segment_size;
    int ret = -1;

    if (!psrc || !size)
    {
        return -1;
    }

    section_length = ((psrc[1] & 0xF) << 8) | psrc[2];
    protocol_version = psrc[3] & 0x3F;
    if (protocol_version != 0) SUBTITLE_LOGI("%s Unsupport scte version: %d only support 0", __FUNCTION__, protocol_version);

    // 1. segmentation_overlay_included
    segmentation_overlay_included = (psrc[3] >> 6) & 1;

    SUBTITLE_LOGI("%s section_length:%d protocol_version:%d segmentation_overlay_included:%d", __FUNCTION__, section_length, protocol_version, segmentation_overlay_included);
    if (segmentation_overlay_included) {
        int table_ext = (psrc[4] << 8) | psrc[5];
        int last_no = (psrc[6] << 4) | ((psrc[7] >> 4) & 0xF);
        int curr_no = ((psrc[7] & 0xF)) << 8 | psrc[8];

        if (curr_no > last_no) {
            SUBTITLE_LOGE("%s curr_no:%d > last_no:%d fail.", __FUNCTION__, curr_no, last_no);
            return -1;
        }

        if (curr_no == 0) {
            SUBTITLE_LOGI("%s scte overlay", __FUNCTION__);
            cleanSegment();
            mContext->seg_size = 0;
            mContext->table_ext = table_ext;
            mContext->last_no = last_no;
            mContext->curr_no = -1;
        }

        if (table_ext != mContext->table_ext && mContext->has_segment) {
            SUBTITLE_LOGE("%s segment ext does not match", __FUNCTION__);
            cleanSegment();
            return -1;
        }

        if ((curr_no != (mContext->curr_no + 1)) && mContext->has_segment) {
            SUBTITLE_LOGE("%s segment curr_no does not match", __FUNCTION__);
            cleanSegment();
            return -1;
        }

        mContext->curr_no = curr_no;
        mContext->has_segment = 1;
        segment_size = section_length - 1 - 5 - 4;
        memcpy(&mContext->segment_container[mContext->seg_size],&psrc[9],segment_size);
        mContext->seg_size += segment_size;
        if (mDumpSub) SUBTITLE_LOGI("%s segment_size:%d seg_size:%d curr_no:%d last_no:%d", __FUNCTION__, segment_size, mContext->seg_size, curr_no, last_no);

        if (curr_no == last_no) {
            if (mContext->seg_size < 12) {
                SUBTITLE_LOGE("%s scte_segment.seg_size:%d < 12 fail.", __FUNCTION__, section_length - 1 - 5 - 4);
                return -1;
            }
            if (mDumpSub) {
                for (size_t i = 0; i < mContext->seg_size; ++i) {
                    SUBTITLE_LOGI("segmentation_overlay_included:%02X", (unsigned char)mContext->segment_container[i]);
                }
            }
            ret = decodeMessageBodySubtitle(spu, mContext->segment_container, mContext->seg_size);
            cleanSegment();
        }
    } else { // decode scte27 message body
        if (mContext->has_segment) cleanSegment();
        if ((section_length - 1 - 4) < 12) {
            SUBTITLE_LOGE("(section_length - 1 - 4):%d < 12 fail.", section_length - 1- 4);
            return -1;
        }
        ret = decodeMessageBodySubtitle(spu, &psrc[4], section_length - 1 - 4);
    }

    SUBTITLE_LOGI("[%s::%d] (width=%d,height=%d), (x=%d,y=%d),ret =%d,spu->buffer_size=%d--------\n", __FUNCTION__, __LINE__,
            spu->spu_width, spu->spu_height, spu->spu_start_x, spu->spu_start_y, ret, spu->buffer_size);

    if (ret != -1 && spu->buffer_size > 0) {
        if (mDumpSub) SUBTITLE_LOGI("[%s::%d]dump-pts-hwdmx!success pts(%lld) frame was add\n", __FUNCTION__,__LINE__, spu->pts);
        if (spu->spu_origin_display_w <= 0 || spu->spu_origin_display_h <= 0) {
            spu->spu_origin_display_w = VideoInfo::Instance()->getVideoWidth();
            spu->spu_origin_display_h = VideoInfo::Instance()->getVideoHeight();
        }
        addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
    } else {
        if (mDumpSub) SUBTITLE_LOGI("[%s::%d]dump-pts-hwdmx!error this pts(%lld) frame was abandon\n", __FUNCTION__,__LINE__, spu->pts);
    }
    if (ret) SUBTITLE_LOGE("%s decodeMessageBodySubtitle failed",__FUNCTION__);
    return ret;
}

int Scte27Parser::hwDemuxParse(std::shared_ptr<AML_SPUVAR> spu, char *psrc, const int size) {
    int ret = -1;
    ret = decodeSubtitle(spu, psrc, size);
    return ret;
}

int Scte27Parser::softDemuxParse(std::shared_ptr<AML_SPUVAR> spu, char *psrc, const int size) {
    char tmpbuf[256] = {0};
    int64_t pts = 0, ptsDiff = 0;
    int ret = 0;
    int dataLen = 0;
    char *data = NULL;
    int avil = 0;

    SUBTITLE_LOGI("## 333 get_dvb_spu %x,%x,%x,%x-------------\n",tmpbuf[0], tmpbuf[1], tmpbuf[2], tmpbuf[3]);
    if (mDataSource->read(tmpbuf, 19) == 19) {
        dataLen = subPeekAsInt32(tmpbuf + 3);
        pts     = subPeekAsInt64(tmpbuf + 7);
        ptsDiff = subPeekAsInt32(tmpbuf + 15);

        std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());
        spu->sync_bytes = AML_PARSER_SYNC_WORD;
        spu->subtitle_type = TYPE_SUBTITLE_SCTE27;
        SUBTITLE_LOGI("@@[%s::%d]malloc ptr=%p, size=%d\n",__FUNCTION__, __LINE__, spu->spu_data, DVB_SUB_SIZE);
        spu->pts = pts;
        SUBTITLE_LOGI("fmq send pts:%lld\n", spu->pts);
        SUBTITLE_LOGI("## 4444 datalen=%d, pts=%llx,delay=%llx, diff=%llx, data: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x-------------\n",
                dataLen, pts, spu->m_delay, ptsDiff,
                tmpbuf[0], tmpbuf[1], tmpbuf[2], tmpbuf[3], tmpbuf[4],
                tmpbuf[5], tmpbuf[6], tmpbuf[7], tmpbuf[8], tmpbuf[9],
                tmpbuf[10], tmpbuf[11], tmpbuf[12], tmpbuf[13], tmpbuf[14]);

        data = (char *)malloc(dataLen);
        if (!data) {
            SUBTITLE_LOGI("data buf malloc fail!\n");
            return -1;
        }
        memset(data, 0x0, dataLen);
        ret = mDataSource->read(data, dataLen);
        if (ret <= 0) {
            SUBTITLE_LOGI("no data now\n");
        }
        ret = decodeMessageBodySubtitle(spu, data, dataLen);
        if (ret != -1 && spu->buffer_size > 0) {
            SUBTITLE_LOGI("dump-pts-swdmx!success pts(%lld) frame was add\n", spu->pts);
            addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
            notifySubtitleDimension(spu->spu_origin_display_w, spu->spu_origin_display_h);

            {
                std::unique_lock<std::mutex> autolock(mMutex);
                mPendingAction = 1;
                mCv.notify_all();
            }

        } else {
            SUBTITLE_LOGI("dump-pts-swdmx!error this pts(%lld) frame was abandon\n", spu->pts);
        }

        if (data) free(data);
    }
    return ret;
}

int Scte27Parser::getScte27Spu() {
    char *originBuffer = NULL; // Pointer to dynamically allocated buffer
    originBuffer = new char[DATA_SIZE];

    if (mDumpSub) SUBTITLE_LOGI("enter get_scte27_spu\n");
    int ret = -1;

    int size = mDataSource->read(originBuffer, 0xffff);
    if (mState == SUB_STOP) {
        if (originBuffer != NULL) {
            delete[] originBuffer;
        }
        return 0;
    }
    if (size <= 0) return -1;
    std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());
    spu->sync_bytes = AML_PARSER_SYNC_WORD;
    ret = hwDemuxParse(spu, originBuffer, DATA_SIZE);

    if (originBuffer != NULL) {
        delete[] originBuffer;
    }
    return ret;
}

int Scte27Parser::getSpu() {
    if (mState == SUB_INIT) {
        mState = SUB_PLAYING;
    } else if (mState == SUB_STOP) {
        SUBTITLE_LOGI(" mState == SUB_STOP \n\n");
        return 0;
    }

    return getScte27Spu();
}

int Scte27Parser::getInterSpu() {
    return getSpu();
}

int Scte27Parser::parse() {
    while (!mThreadExitRequested) {
        if (getInterSpu() < 0) {
            // advance input, if parse failed, wait and retry.
            //std::this_thread::sleep_for(std::chrono::milliseconds(1));
            usleep(100);
        }
    }
    return 0;
}

void Scte27Parser::dump(int fd, const char *prefix) {
    dprintf(fd, "%s SCTE 27 Parser\n", prefix);
    dumpCommon(fd, prefix);

    if (mContext != nullptr) {
        mContext->dump(fd, prefix);
    }

}
