/*
* Copyright (c) 2014 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package.
*
* Description: h file
*/
#ifndef _PGS_SUB_H
#define _PGS_SUB_H
#include <utils/Log.h>


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



#endif
