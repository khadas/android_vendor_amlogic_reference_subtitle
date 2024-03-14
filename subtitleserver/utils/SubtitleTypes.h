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

#ifndef _PGS_SUB_H
#define _PGS_SUB_H

#include <stdlib.h>
#include <pthread.h>
#include <functional>
#include <cutils/properties.h>

#include "DemuxDriver.h"
#include "UserdataDriver.h"
#include "AmlogicUtil.h"
#include "ClosedCaption.h"

#include "SubtitleLog.h"


// TODO: impl as a class item, optimize the structure[currently direct borrow from old impl]
// support sort!!!
typedef struct alm_spuvar
{
    unsigned int sync_bytes = 0;
    unsigned int buffer_size = 0;
    bool useMalloc = false;
    bool isSimpleText = false;
    unsigned int pid = 0;
    int64_t pts = 0;
    bool     isImmediatePresent = false;
    bool     isExtSub = false;
    bool     isKeepShowing = false; //no auto fading out, until close
    bool     isTtxSubtitle = false;
    unsigned int objectSegmentId = 0; //objectSegmentId: the current number object segment object.

    int64_t m_delay = 0;
    unsigned char *spu_data = NULL;
    unsigned short cmd_offset = 0;
    unsigned short length = 0;

    unsigned int r_pt = 0;
    unsigned int frame_rdy = 0;

    unsigned short spu_color = 0;
    unsigned short spu_alpha = 0;
    unsigned short spu_start_x = 0;
    unsigned short spu_start_y = 0;
    unsigned short spu_width = 0;
    unsigned short spu_height = 0;
    unsigned short top_pxd_addr = 0;
    unsigned short bottom_pxd_addr = 0;

    unsigned int spu_origin_display_w = 0; //for bitmap subtitle
    unsigned int spu_origin_display_h = 0;
    unsigned disp_colcon_addr = 0;
    unsigned char display_pending = 0;
    unsigned char displaying = 0;
    unsigned char subtitle_type = 0;
    unsigned char reser[2] = {0};

/*
    unsigned rgba_enable;
    unsigned rgba_background;
    unsigned rgba_pattern1;
    unsigned rgba_pattern2;
    unsigned rgba_pattern3;
    */

    //for vob
    int resize_height = 0;
    int resize_width = 0;
    int resize_xstart = 0;
    int resize_ystart = 0;
    int resize_size = 0;

    int disPlayBackground = 0;
    //for qtone data inserted in cc data.
    bool isQtoneData = false;

    bool dynGen = false; // generate bitmap data dynamically
    int64_t pos = 0;
    std::function<unsigned char *(struct alm_spuvar *spu, size_t *size)> genMethod;
    void genSubtitle() {
        size_t size;
        if (dynGen) {
            spu_data = genMethod(this, &size);
        }
    }

    bool isBitmapSpu() {
        return spu_width > 0 && spu_height > 0 && buffer_size > 0;
    }

    alm_spuvar() : sync_bytes(0), buffer_size(0), useMalloc(true), isSimpleText(false),
            pid(0), pts(0), isImmediatePresent(false), isExtSub(false), isKeepShowing(false),
            m_delay(0), spu_data(nullptr), cmd_offset(0), length(0), disPlayBackground(0), isQtoneData(false)
    {
        dynGen = false;
        pos = 0;
        spu_start_x = spu_start_y = spu_width = spu_height = 0;
        spu_origin_display_w = spu_origin_display_h = 0;
    }

    ~alm_spuvar() {
        if (useMalloc) {
            if (spu_data != nullptr) free(spu_data);
        } else {
            if (spu_data != nullptr) delete[] spu_data;
        }
        spu_data = nullptr;
    }

    void dump(int fd, const char *prefex) {
        dprintf(fd, "%s ", prefex);
        if (isImmediatePresent) {
            dprintf(fd, "ImmediatePresent ");
        } else {
            dprintf(fd, "pts=%lld delayTo=%lld ", pts, m_delay);
        }

        if (isSimpleText) {
            dprintf(fd, "text=[%s]\n", spu_data);
        } else {
            dprintf(fd, "videoW=%u videoH=%u ", spu_origin_display_w, spu_origin_display_h);
            dprintf(fd, "[x,y,w,h][%d %u %u %u] ", spu_start_x, spu_start_y, spu_origin_display_w, spu_origin_display_h);
            dprintf(fd, "data=%p size=%u\n", spu_data, buffer_size);
        }
    }
} AML_SPUVAR;


#define CC_JSON_BUFFER_SIZE 8192

#define PIP_INVALID_ID -1
#define PIP_PLAYER_ID 1
#define PIP_MEDIASYNC_ID 2


typedef void* ClosedCaptionHandleType;

typedef struct {
    pthread_mutex_t  lock;
    //AM_TT2_Handle_t  tt_handle;
    ClosedCaptionHandleType   cc_handle;
    int              dmx_id;
    int              filter_handle;
    //jobject        obj;
    //SkBitmap       *bitmap;
    //jobject        obj_bitmap;
    int              bmp_w;
    int              bmp_h;
    int              bmp_pitch;
    uint8_t          *buffer;
    int              sub_w;
    int              sub_h;

    void dump(int fd, const char* prefix) {
        dprintf(fd, "%s   SubtitleDataContext:\n", prefix);
        dprintf(fd, "%s     handle[cc:%p]\n", prefix, cc_handle);
        dprintf(fd, "%s     dmx_id:%d\n", prefix, dmx_id);
        dprintf(fd, "%s     filter_handle:%d\n", prefix, filter_handle);
        dprintf(fd, "%s     bitmap size:%dx%d\n", prefix, bmp_w, bmp_h);
        dprintf(fd, "%s     bitmap pitch:%d\n", prefix, bmp_pitch);
        dprintf(fd, "%s     subtitle size:%dx%d\n", prefix, sub_w, sub_h);
        dprintf(fd, "%s     buffer:%p\n", prefix, buffer);
    }
} TVSubtitleData;

#endif
