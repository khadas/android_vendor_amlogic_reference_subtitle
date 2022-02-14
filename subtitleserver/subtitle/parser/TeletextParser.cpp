/*
 * Teletext decoding for ffmpeg
 * Copyright (c) 2005-2010, 2012 Wolfram Gloger
 * Copyright (c) 2013 Marton Balint
 *
 * This library is free software; you can redistribute it and/or
*
* Copyright (c) 2014 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package.
*
* Description: c++file
*/
#define  LOG_TAG "TeletextParser"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bprint.h"
#include <android/log.h>
#include <utils/CallStack.h>

#include <unistd.h>
#include <fcntl.h>
#include <list>
#include <thread>
#include <algorithm>
#include <functional>

#include "ParserFactory.h"
#include "streamUtils.h"

#include "VideoInfo.h"

#include "TeletextParser.h"

//#define  ALOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
//#define  ALOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define  TRACE()    LOGI("[%s::%d]\n",__FUNCTION__,__LINE__)

//teletext graphics support load animation
//#define SUPPORT_LOAD_ANIMATION

#define TELETEX_WARN_PAGE_1 0x548080
#define TELETEX_WARN_PAGE_2 0x548000
#define TELETEX_WARN_PAGE_3 0xb18081

#define LOGI ALOGI
#define LOGD ALOGD
#define LOGE ALOGE
#define LOGV ALOGV

#define TELETEXT_ROW 26
#define TELETEXT_COL 41

#define TELETEXT_USE_SUBTITLESERVER 1

#define TELETEXT_MAX_PAGE_NUMBER 900
#define TELETEXT_MIN_PAGE_NUMBER 99
#define DATA_VALID_AND_BLANK 2 //valid teletext data but is blank
#define TEXT_MAXSZ    ((TELETEXT_ROW) * (56 + 1) * 4 + 2)
#define VBI_NB_COLORS 40
#define VBI_LOP_SUBPAGE_LINK_LENGTH 6*6
#define VBI_TRANSPARENT_BLACK 8
#define AM_TT2_ANY_SUBNO 0x3F7F
#define AM_TT2_HOME_PAGENO 100
#define RGBA(r,g,b,a) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define VBI_R(rgba)   (((rgba) >> 0) & 0xFF)
#define VBI_G(rgba)   (((rgba) >> 8) & 0xFF)
#define VBI_B(rgba)   (((rgba) >> 16) & 0xFF)
#define VBI_A(rgba)   (((rgba) >> 24) & 0xFF)
#define MAX_BUFFERED_PAGES 25
#define BITMAP_CHAR_WIDTH  12
#define BITMAP_CHAR_HEIGHT 10

#define DOUBLE_HEIGHT_SCROLL_FACTOR 2
#define DOUBLE_HEIGHT_SCROLL_SECTION 6
#define DOUBLE_HEIGHT_SCROLL_SECTION_PLUS DOUBLE_HEIGHT_SCROLL_SECTION+1  //+1 for sub page row

#define TELETEXT_HEAD_HEIGHT 10
#define TELETEXT_TEXT_HEIGHT 230
#define TELETEXT_BAR_HEIGHT 20
#define TELETEXT_GRAPHIC_WIDTH 492


#define DOUBLE_BITMAP_CHAR_HEIGHT BITMAP_CHAR_HEIGHT/2


#define OSD_HALF_SIZE (1920*1280/8)
#define NAVIGATOR_COLORBAR_LINK_SIZE 4

#define TELETEXT_GRAPHICS_SUBTITLE_PAGENUMBER_BLACKGROUND

#ifdef NEED_CACHE_ZVBI_STATUS
class ZvbiGlobalStatus {
public:
    /**
    * If the program not changed, we must keep Vbi decoder
    * Or, the cached teletext data will dropped and re-search tt pages.
    * This introduce very bad UI experience.
    *
    */
    bool needReuseVbiDecoder() {
        return mKeepUsingVbi;
    }

    vbi_decoder *getVbiInstance() {
        return mVbi;
    }

    void registerVbiInstance(vbi_decoder *vbi) {
        mVbi = vbi;
    }

    void updateProgramInfo(int pid, int onid, int tsid) {
        if (mLastPid ==pid && mLastOnid == onid && mLastTsid == tsid && tsid != -1) {
            mKeepUsingVbi = true;
        } else {
            mKeepUsingVbi = false;
        }

        mLastPid = pid;
        mLastOnid = onid;
        mLastTsid = tsid;
    }

    void updateSearchLastPageStart() {
        mSearchStart = std::chrono::system_clock::now();
        mNeedCheckTimeout = true;
    }

    // if search success, no need
    void searchLastPageFinished() {
        mNeedCheckTimeout = false;
    }

    bool isSearchLastPageTimeout() {
        if (!mNeedCheckTimeout) return false;

        // timeout is 30S
        std::chrono::duration<double> diff = std::chrono::system_clock::now() - mSearchStart;
        if (diff > std::chrono::seconds(30)) {
            mNeedCheckTimeout = false;
            return true;
        }
        return false;
    }

    ZvbiGlobalStatus() : lastShowingPage(0),subtitlePageId(0), mVbi(nullptr),
        mLastPid(-1), mLastOnid(-1), mLastTsid(-1) {
        mKeepUsingVbi = false;
        for (int j = 0; j < TELETEXT_SUBTITLE_MAX_NUMBER; j++) {
                atvSubtitlePage[j] = 0;
                dtvSubtitlePage[j] = 0;
        }
    }

int lastShowingPage;
int subtitlePageId;
int atvSubtitlePage[TELETEXT_SUBTITLE_MAX_NUMBER];
int dtvSubtitlePage[TELETEXT_SUBTITLE_MAX_NUMBER];


private:
    vbi_decoder *mVbi;
    int mLastPid;
    int mLastOnid;
    int mLastTsid;
    bool mKeepUsingVbi;

    // if use async handler thread, maybe easy to implement this machanism
    std::chrono::time_point<std::chrono::system_clock> mSearchStart;
    bool mNeedCheckTimeout;
};

ZvbiGlobalStatus gVBIStatus;

#endif


static inline int checkIdentifierIsTeletext(int identifier) {
    LOGI("[checkIdentifierIsTeletext]---identifier:0x%x--\n", identifier);
    return (   (identifier >= 0x10 && identifier <= 0x1F)
            || (identifier >= 0x99 && identifier <= 0x9B));
}

/* Returns true if data unit id matches EBU teletext data according to
 * EN 301 775 section 4.4.2 */
static inline int checkUnitIdIsTeletext(int unitId) {
    /*teletext: 0x02,  teletext subtitle 0x03*/
    return (unitId == 0x02 || unitId == 0x03);
}


static inline int chopUtf8Spaces(const char* t, int len) {
    t += len;
    while (len > 0) {
        if (*--t != ' ' || (len-1 > 0 && *(t-1) & 0x80)) {
            break;
        }
        --len;
    }
    return len;
}

static inline void array_sort(int *a, int len)
{
    int i, j, tmp;
    for (i = 0; i < len - 1; i++)
    {
        for (j = i + 1; j < len; j++)
        {
            if (a[i] > a[j])
            {
                tmp = a[i];
                a[i] = a[j];
                a[j] = tmp;
            }
        }
    }
}

static int slice2VbiLines(TeletextContext *ctx, char* buf, int size) {
    int lines = 0;
    while (size >= 2 && lines < MAX_SLICES) {
        int unitId  = buf[0];     //02
        int unitLen = buf[1];  //2c  value: 44

        if (unitLen + 2 > size) {
            return lines;
        }

        if (checkUnitIdIsTeletext(unitId)) {
            //LOGI("[slice2VbiLines]-length==:%x",unitLen);
            if (unitLen != 0x2c) {          //44
                return -1;
            } else {
                int lineOffset  = buf[2] & 0x1f;  //e8  value: 8
                int fieldParity = buf[2] & 0x20;  //e8  value:0x20  32
                //LOGI("[slice2VbiLines]-buf[2]:%x,lineOffset::%d,fieldParity:%d",buf[2],lineOffset,fieldParity);

                ctx->sliced[lines].id = VBI_SLICED_TELETEXT_B;
                ctx->sliced[lines].line = (lineOffset > 0 ? (lineOffset + (fieldParity ? 0 : 13)) : 0);
                //LOGI("[slice2VbiLines]-buf[2]:line:%d",ctx->sliced[lines].line);
                for (int i = 0; i < 42; i++) {
                    ctx->sliced[lines].data[i] = vbi_rev8(buf[4 + i]);
                }
                lines++;
            }
        }
        size -= unitLen + 2;
        buf += unitLen + 2;
    }
    if (size) LOGI("%d bytes remained after slicing data\n", size);
    return lines;
}


/* Draw a page as text */
static int genSubText(TeletextContext *ctx, AVSubtitleRect *subRect, vbi_page *page, int chopTop) {
    const char *in;
    AVBPrint buf;
   char *vbi_text = (char *)malloc(TEXT_MAXSZ);
    int sz;

    if (!vbi_text)
        return -1;//AVERROR(ENOMEM);

    sz = vbi_print_page_region(page, vbi_text, TEXT_MAXSZ-1, "UTF-8",
                                   /*table mode*/ TRUE, FALSE,
                                   0,             chopTop,
                                   page->columns, page->rows-chopTop);
    if (sz <= 0) {
        LOGI("vbi_print error\n");
        free(vbi_text);
        return -1;
    }
    vbi_text[sz] = '\0';
    in  = vbi_text;
    av_bprint_init(&buf, 0, TEXT_MAXSZ);

    if (ctx->chopSpaces) {
        for (;;) {
            int nl, sz;

            // skip leading spaces and newlines
            in += strspn(in, " \n");
            // compute end of row
            for (nl = 0; in[nl]; ++nl)
                if (in[nl] == '\n' && (nl == 0 || !(in[nl-1] & 0x80)))
                    break;
            if (!in[nl])
                break;
            // skip trailing spaces
            sz = chopUtf8Spaces(in, nl);
            //av_bprint_append_data(&buf, in, sz);
            av_bprintf(&buf, "\n");
            in += nl;
        }
    } else {
        av_bprintf(&buf, "%s\n", vbi_text);
    }
    free(vbi_text);

    if (!av_bprint_is_complete(&buf)) {
        av_bprint_finalize(&buf, NULL);
        return -1;//AVERROR(ENOMEM);
    }

    if (buf.len) {
       subRect->type = SUBTITLE_ASS;
        // TODO here
        ALOGD("\n\n\n\n\n\n\n\n\n\nTODO!! should not run here~\n\n\n\n\n\n\n\n\n\n\n");
       //subRect->ass = create_ass_text(ctx, buf.str);

        if (!subRect->ass) {
            av_bprint_finalize(&buf, NULL);
            return -1;//AVERROR(ENOMEM);
       }
        LOGI("subtext:%s:txetbus\n", subRect->ass);
    } else {
        subRect->type = SUBTITLE_NONE;
    }
    av_bprint_finalize(&buf, NULL);
    return 0;
}

static void fixTransparency(TeletextContext *ctx, AVSubtitleRect *subRect, vbi_page *page,
                             int chopTop, int resx, int resy) {
    (void)resx; // unused

    // Hack for transparency, inspired by VLC code...
    for (int iy = 0; iy < resy; iy++) {
        uint8_t *pixel = subRect->pict.data[0] + iy * subRect->pict.lineSize[0];
        vbi_char *vc = page->text + (iy / BITMAP_CHAR_HEIGHT + chopTop) * page->columns;
        vbi_char *vcnext = vc + page->columns;
        for (; vc < vcnext; vc++) {
            uint8_t *pixelnext = pixel + BITMAP_CHAR_WIDTH;
            switch (vc->opacity) {
                case VBI_TRANSPARENT_SPACE:
                    memset(pixel, VBI_TRANSPARENT_BLACK, BITMAP_CHAR_WIDTH);
                    break;
                case VBI_OPAQUE:
                    if (!ctx->transparentBackground) break;
                    [[fallthrough]];
                case VBI_SEMI_TRANSPARENT:
                   if ((ctx->opacity > 0 && ctx->opacity != 255) || (ctx->opacity == 255 && ctx->isSubtitle)) {
                        if (ctx->opacity < 255) {
                            for (; pixel < pixelnext; pixel++) {
                                if (*pixel == vc->background) {
                                    *pixel += VBI_NB_COLORS;
                                }
                            }
                        }
                        break;
                    }
                    [[fallthrough]];
                case VBI_TRANSPARENT_FULL:
                    //when the data is graphics,the last two line data doesn't apply the transparent
                    if (!(ctx->isSubtitle) && ((iy >= 240) && (iy <= 260)))
                        break;
                    for (; pixel < pixelnext; pixel++) {
                        if (*pixel == vc->background) {
                            *pixel = VBI_TRANSPARENT_BLACK;
                        }
                    }
                    break;
            }
            pixel = pixelnext;
        }
    }
}


static void save2BitmapFile(const char *filename, uint32_t *bitmap, int w, int h)
{
    LOGI("png_save2:%s\n",filename);
    FILE *f;
    char fname[40];

    snprintf(fname, sizeof(fname), "%s.bmp", filename);
    f = fopen(fname, "w");
    if (!f) {
        perror(fname);
        return;
    }
    fprintf(f, "P6\n%d %d\n%d\n", w, h, 255);
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

static bool isSubtitlePage(int array[], int pageNumber) {
    for (int i = 0; i < TELETEXT_SUBTITLE_MAX_NUMBER; i++) {
        if (pageNumber == array[i]) return true;
        if (pageNumber < array[i] || 0 == array[i]) return false;
    }
    return false;
}

/* Draw a page as bitmap */
static int genSubBitmap(TeletextContext *ctx, AVSubtitleRect *subRect, vbi_page *page, int chopTop)
{
    int resx = page->columns * BITMAP_CHAR_WIDTH;
    int resy;
    int height;

    subRect->x = ctx->xOffset;
    subRect->y = ctx->yOffset + chopTop * BITMAP_CHAR_HEIGHT;

    if ((ctx->atvTeletext && isSubtitlePage(gVBIStatus.atvSubtitlePage, ctx->gotoPage)) || (ctx->dtvTeletext && isSubtitlePage(gVBIStatus.dtvSubtitlePage, ctx->gotoPage))) {
        resy = (page->rows - chopTop) * BITMAP_CHAR_HEIGHT;
    } else {
        if (ctx->doubleHeight == DOUBLE_HEIGHT_NORMAL) {
            resy = (page->rows - chopTop) * BITMAP_CHAR_HEIGHT;
        } else if (ctx->doubleHeight == DOUBLE_HEIGHT_BOTTOM) {
            resy = (page->rows - chopTop) * DOUBLE_BITMAP_CHAR_HEIGHT;
        } else if (ctx->doubleHeight == DOUBLE_HEIGHT_TOP) {
            resy = (page->rows - chopTop) * DOUBLE_BITMAP_CHAR_HEIGHT;
        }
    }

    uint8_t ci;
    vbi_char *vc = page->text + (chopTop * page->columns);
    vbi_char *vcend = page->text + (page->rows * page->columns);
    LOGI("--%s--\n",__FUNCTION__);
    for (; vc < vcend; vc++) {
        if (vc->opacity != VBI_TRANSPARENT_SPACE)
            break;
    }

    if (vc >= vcend) {
        if (ctx->isSubtitle) {
            LOGI("Currently request show null subtitle data, so draw it will clear screen.");
            // if this page is subtitle, and nothing need to draw.
            // then request to draw, draw an empty clear screen.
            page->rows =TELETEXT_ROW;
            page->columns = TELETEXT_COL;
            resx = page->columns * BITMAP_CHAR_WIDTH;
            resy = (page->rows - chopTop) * BITMAP_CHAR_HEIGHT;
            subRect->w = resx;
            subRect->h = resy;
        } else {
            LOGI("dropping empty page %3x\n", page->pgno);
            subRect->type = SUBTITLE_NONE;
            return 0;
        }
    }

    subRect->pict.data[0] = (uint8_t *)calloc(resx * resy, 1);
    subRect->pict.lineSize[0] = resx;
    if (!subRect->pict.data[0]) {
        return -1;//AVERROR(ENOMEM);
    }

    LOGI("%s, display mode:%d, isSubtitle:%d, ctx->time:%s, subtitlePageNumber:%d, gotoGraphicsSubtitlePage:%d, lockSubpg:%d, page->columns:%d, rows:%d, heightIndex:%d, doubleHeight:%d, x:%d, y:%d, w:%d, h:%d, choptop:%d\n", __FUNCTION__,
     ctx->dispMode, ctx->isSubtitle, ctx->time, ctx->subtitlePageNumber, ctx->gotoGraphicsSubtitlePage, ctx->lockSubpg, page->columns, page->rows, ctx->heightIndex, ctx->doubleHeight, subRect->x,
     subRect->y, subRect->w, subRect->h, chopTop);

    if ((ctx->atvTeletext && isSubtitlePage(gVBIStatus.atvSubtitlePage, ctx->gotoPage)) || (ctx->dtvTeletext && isSubtitlePage(gVBIStatus.dtvSubtitlePage, ctx->gotoPage))) {
        height = page->rows - chopTop;
    } else {
        if (ctx->doubleHeight == DOUBLE_HEIGHT_NORMAL) {
            height = page->rows - chopTop;
        } else {
            height = (page->rows - chopTop)/2;
        }
    }

    if (ctx->atvTeletext && isSubtitlePage(gVBIStatus.atvSubtitlePage, ctx->gotoPage)) {
        LOGI("%s this is ATV ctx->gotoPage", __FUNCTION__, ctx->gotoPage);
        ctx->isSubtitle = true;
    }

    #ifdef TELETEXT_GRAPHICS_SUBTITLE_PAGENUMBER_BLACKGROUND
        if (ctx->isSubtitle) {
            if (ctx->resetShowSubtitlePageNumberTimeFlag || (ctx->subtitlePageNumber != ctx->gotoGraphicsSubtitlePage && ctx->gotoGraphicsSubtitlePage != 0) || (ctx->gotoGraphicsSubtitlePage > 0 && ctx->gotoGraphicsSubtitlePage <= TELETEXT_MIN_PAGE_NUMBER)) {
                page->pgno = vbi_dec2bcd(ctx->gotoGraphicsSubtitlePage);
                ctx->subtitlePageNumber = ctx->gotoGraphicsSubtitlePage;
                ctx->lasttime = std::chrono::system_clock::now();
                ctx->subtitlePageNumberShowTimeOutFlag = true;
                ctx->resetShowSubtitlePageNumberTimeFlag = false;
            } else {
                if (std::chrono::system_clock::now() - ctx->lasttime >= std::chrono::seconds(TELETEXT_SUBTITLE_PAGE_SHOW_TIME)) {
                    page->pgno = -1;//Do not display page numbers
                    ctx->subtitlePageNumberShowTimeOutFlag = false;
                } else {
                    page->pgno = vbi_dec2bcd(ctx->gotoGraphicsSubtitlePage);
                    ctx->subtitlePageNumber = ctx->gotoGraphicsSubtitlePage;
                }
            }

            if (page->pgno < 0) {
                if ((ctx->atvTeletext && isSubtitlePage(gVBIStatus.atvSubtitlePage, ctx->gotoPage)) || (ctx->dtvTeletext && isSubtitlePage(gVBIStatus.dtvSubtitlePage, ctx->gotoPage))) {
                    vbi_draw_vt_page_region(page, VBI_PIXFMT_PAL8, subRect->pict.data[0], subRect->pict.lineSize[0],
                        0, 2*0 + chopTop, page->columns, height,
                        /*reveal*/ctx->reveal, /*flash*/ ctx->flash, /*Subtitle*/ctx->isSubtitle, 1/*ctx->isSubtitle*/, 1, ctx->lockSubpg/*0*/,  ctx->time, 0);
                } else {
                    vbi_draw_vt_page_region(page, VBI_PIXFMT_PAL8, subRect->pict.data[0], subRect->pict.lineSize[0],
                        0, 2*ctx->heightIndex + chopTop, page->columns, height,
                        /*reveal*/ctx->reveal, /*flash*/ ctx->flash, /*Subtitle*/ctx->isSubtitle, 0/*ctx->isSubtitle*/, 1, ctx->lockSubpg/*0*/,  ctx->time, 0);
                }
            } else {
                if ((ctx->atvTeletext && isSubtitlePage(gVBIStatus.atvSubtitlePage, ctx->gotoPage)) || (ctx->dtvTeletext && isSubtitlePage(gVBIStatus.dtvSubtitlePage, ctx->gotoPage))) {
                    vbi_draw_vt_page_region(page, VBI_PIXFMT_PAL8, subRect->pict.data[0], subRect->pict.lineSize[0],
                        0, 2*0 + chopTop, page->columns, height,
                        /*reveal*/ctx->reveal, /*flash*/ ctx->flash, /*Subtitle*/ctx->isSubtitle, 1/*ctx->isSubtitle*/, 1, ctx->lockSubpg/*0*/,  ctx->time, 0);
                } else {
                    vbi_draw_vt_page_region(page, VBI_PIXFMT_PAL8, subRect->pict.data[0], subRect->pict.lineSize[0],
                        0, 2*ctx->heightIndex + chopTop, page->columns, height,
                        /*reveal*/ctx->reveal, /*flash*/ ctx->flash, /*Subtitle*/ctx->isSubtitle, 0/*ctx->isSubtitle*/, 1, ctx->lockSubpg/*0*/,  ctx->time, 0);
                }

            }

        } else {
            ctx->lasttime = std::chrono::system_clock::now();
            ctx->subtitlePageNumberShowTimeOutFlag = false;
            if ((ctx->atvTeletext && isSubtitlePage(gVBIStatus.atvSubtitlePage, ctx->gotoPage)) || (ctx->dtvTeletext && isSubtitlePage(gVBIStatus.dtvSubtitlePage, ctx->gotoPage))) {
                vbi_draw_vt_page_region(page, VBI_PIXFMT_PAL8, subRect->pict.data[0], subRect->pict.lineSize[0],
                    0, 2*0 + chopTop, page->columns, height,
                    /*reveal*/ctx->reveal, /*flash*/ ctx->flash, /*Subtitle*/ctx->isSubtitle, ctx->dispMode/*0*/, 1, ctx->lockSubpg/*0*/,  ctx->time, 0);
            } else {
                vbi_draw_vt_page_region(page, VBI_PIXFMT_PAL8, subRect->pict.data[0], subRect->pict.lineSize[0],
                    0, 2*ctx->heightIndex + chopTop, page->columns, height,
                    /*reveal*/ctx->reveal, /*flash*/ ctx->flash, /*Subtitle*/ctx->isSubtitle, ctx->dispMode/*0*/, 1, ctx->lockSubpg/*0*/,  ctx->time, 0);
            }
        }
    #else
        if ((ctx->atvTeletext && isSubtitlePage(gVBIStatus.atvSubtitlePage, ctx->gotoPage)) || (ctx->dtvTeletext && isSubtitlePage(gVBIStatus.dtvSubtitlePage, ctx->gotoPage))) {
            vbi_draw_vt_page_region(page, VBI_PIXFMT_PAL8, subRect->pict.data[0], subRect->pict.lineSize[0],
                0, 2*0 + chopTop, page->columns, height,
                /*reveal*/ctx->reveal, /*flash*/ ctx->flash, /*Subtitle*/ctx->isSubtitle, ctx->dispMode/*0*/, 1, ctx->lockSubpg/*0*/,  ctx->time, 0);
        } else {
            vbi_draw_vt_page_region(page, VBI_PIXFMT_PAL8, subRect->pict.data[0], subRect->pict.lineSize[0],
                0, 2*ctx->heightIndex + chopTop, page->columns, height,
                /*reveal*/ctx->reveal, /*flash*/ ctx->flash, /*Subtitle*/ctx->isSubtitle, ctx->dispMode/*0*/, 1, ctx->lockSubpg/*0*/,  ctx->time, 0);
        }
    #endif

    fixTransparency(ctx, subRect, page, chopTop, resx, resy);
    subRect->w = resx;
    subRect->h = resy;
    subRect->nbColors = ctx->opacity > 0 && ctx->opacity < 255 ? 2 * VBI_NB_COLORS : VBI_NB_COLORS;
    //LOGI("%s, height state#2:%d, x:%d, y:%d, w:%d, h:%d\n",__FUNCTION__, ctx->doubleHeight, subRect->x, subRect->y, subRect->w, subRect->h);

    subRect->pict.data[1] = (uint8_t *) calloc(AVPALETTE_SIZE, 1);
    if (!subRect->pict.data[1]) {
        free(subRect->pict.data[0]);
        subRect->pict.data[0] = nullptr;
        return -1;//AVERROR(ENOMEM);
    }
    for (ci = 0; ci < VBI_NB_COLORS; ci++) {
        int r, g, b, a;

        r = VBI_R(page->color_map[ci]);
        g = VBI_G(page->color_map[ci]);
        b = VBI_B(page->color_map[ci]);
        a = VBI_A(page->color_map[ci]);
        ((uint32_t *)subRect->pict.data[1])[ci] = RGBA(r, g, b, a);
        ((uint32_t *)subRect->pict.data[1])[ci + VBI_NB_COLORS] = RGBA(r, g, b, ctx->opacity);
        //LOGI("palette %0x\n", ((uint32_t *)subRect->data[1])[ci]);
    }

    ((uint32_t *)subRect->pict.data[1])[VBI_TRANSPARENT_BLACK] = RGBA(0, 0, 0, 0);
    ((uint32_t *)subRect->pict.data[1])[VBI_TRANSPARENT_BLACK + VBI_NB_COLORS] = RGBA(0, 0, 0, 0);
    subRect->type = SUBTITLE_BITMAP;
    return 0;
}

static void tt2TimeUpdate(vbi_event *ev, void *user_data)
{
     TeletextParser *parser = TeletextParser::getCurrentInstance();
     if (parser == nullptr) {
         return;
     }

    TeletextContext *ctx = (TeletextContext*)user_data;

    if (ev->type == VBI_EVENT_TIME) {
        memcpy(ctx->time, ev->time, 8);
        if (ctx->dispUpdate) {
            ctx->acceptSubPage = parser->getSubPageInfoLocked();
            ctx->opacity = ctx->transparentBackground ? 0 : 255;
            ctx->flash = !ctx->flash;
            parser->fetchVbiPageLocked(ctx->gotoPage, ctx->acceptSubPage);
        }
    }
}

static inline void setNavigatorPageNumber(vbi_page *page, int currentPage)
{
     TeletextParser *parser = TeletextParser::getCurrentInstance();
     if ((parser == nullptr) || (page == nullptr)) {
         LOGI("%s, return : page or parser is null \n", __FUNCTION__);
         return;
     }

     /*if (page->have_flof == 1 || vbi_bcd2dec(page->pgno) != currentPage){
         LOGI("%s, page->have_flof:%d,currentPage:%d, vbi_bcd2dec(page->pgno):%d\n", __FUNCTION__,page->have_flof,currentPage,vbi_bcd2dec(page->pgno));
         return;
     }*/

     if ((parser->mNavigatorPage != page->pgno) || (parser->mNavigatorSubPage != page->subno)) {
         parser->mNavigatorPage = page->pgno;
         parser->mNavigatorSubPage = page->subno;
     } else {
         LOGI("%s, return : same page. don't need to set the navigator page \n", __FUNCTION__);
         return;
     }
     if (0 == page->nav_link[0].pgno) {
         LOGI("%s, return : don't have the navigator bar \n", __FUNCTION__);
         return;
     }
     if (!parser->mCurrentNavigatorPage.empty()) {
         parser->mCurrentNavigatorPage.clear();
     }
     for (int i = 0; i < NAVIGATOR_COLORBAR_LINK_SIZE; i++) {
         NavigatorPageT navigatorPage;
         navigatorPage.pageNo = page->nav_link[i].pgno;
         navigatorPage.subPageNo= page->nav_link[i].subno;
         parser->mCurrentNavigatorPage.push_back(navigatorPage);
     }
}

bool isRedundantSubtitlePage(int array[], int pageNumber, int n) {
    if (n == 0) {
        if (array[0] == pageNumber) {
            return true;
        } else {
            return false;
        }
    } else {
        for (int i=0; i<n; i++) {
            if (array[i] == pageNumber) {
                return true;
                break;
            }
        }
        return false;
    }
}

void selectSortSubtitlePage(int array[],int n){
    int c;
    for (int i=0; i<=n-1; i++) {
        for (int j=i+1; j<=n; j++) {
            if (array[i] > array[j]) {
                c=array[j];
                array[j]=array[i];
                array[i]=c;
            }
        }
    }
}


// This is callback function registed to vbi.
// vbi callback from: teletextDecodeFrame, so should not add lock in this
static void handler(vbi_event *ev, void *userData) {
    LOGI("[handler]\n");
    TeletextContext *ctx = (TeletextContext *)userData;
    TeletextPage *newPages;
    int res = 0 ;
    char pgNoStr[12];
    vbi_subno subno;
    vbi_page_type vpt;
    int chopTop = 0x0;
    char *lang;
    int pageType = 0x0;
    int pgno = 0x0;
    int subPgno = 0x0;

    // vbi_page struct is very big, avoid allocate on stack.
    vbi_page *page = (vbi_page*)malloc(sizeof(vbi_page));
    if (page == nullptr) return;
    pgno  = vbi_bcd2dec(ev->ev.ttx_page.pgno);
    subPgno  = vbi_bcd2dec(ev->ev.ttx_page.subno);
    snprintf(pgNoStr, sizeof pgNoStr, "%03x", ev->ev.ttx_page.pgno);
    LOGI("decoded page %s.;  %02x, ctx->pgno=%s, ctx->dispUpdate=%d\n",
           pgNoStr, ev->ev.ttx_page.subno & 0xFF, ctx->pgno, ctx->dispUpdate);

    if (ctx->handlerRet < 0) {
        LOGI("%s, return , ctx->handlerRet=%d\n", __FUNCTION__, ctx->handlerRet);
        free(page);
        return;
    }

    if (ctx->dispUpdate) {
        int atvSubtitlePageInsertFlag = 0;
        int dtvSubtitlePageInsertFlag = 0;
        //save atv subtitle page for skip to subtitle faster
        if (ctx->atvTeletext && pgno < TELETEXT_MAX_PAGE_NUMBER && pgno > TELETEXT_MIN_PAGE_NUMBER) {
            res = vbi_fetch_vt_page(ctx->vbi, page,
                                    ev->ev.ttx_page.pgno,
                                    ev->ev.ttx_page.subno,
                                    VBI_WST_LEVEL_3p5, TELETEXT_ROW, TRUE, &pageType);
            if (!res) {
                LOGE("%s, atv page cannot get now!\n",__FUNCTION__);
            } else {
                if (pageType & 0x8000) {//atv subtitle
                    for (int i=0;i<TELETEXT_SUBTITLE_MAX_NUMBER;i++) {
                        LOGI("%s, save atv subtitle page:%d, gotoAtvSubtitleFlg:%d\n",__FUNCTION__, pgno, ctx->gotoAtvSubtitleFlg);
                        if (!isRedundantSubtitlePage(gVBIStatus.atvSubtitlePage, pgno, i) && gVBIStatus.atvSubtitlePage[i] == 0 && atvSubtitlePageInsertFlag == 0) {
                             gVBIStatus.atvSubtitlePage[i] = pgno;
                             atvSubtitlePageInsertFlag = i;
                        }

                        if (ctx->gotoAtvSubtitleFlg && gVBIStatus.atvSubtitlePage[i] != 0) {
                            ctx->gotoPage = gVBIStatus.atvSubtitlePage[i];
                            ctx->gotoAtvSubtitleFlg = FALSE;
                        }
                    }
                }
            }
        }
        if (ctx->atvTeletext);selectSortSubtitlePage(gVBIStatus.atvSubtitlePage,atvSubtitlePageInsertFlag);
        //save dtv subtitle page for skip to subtitle faster
        if (ctx->dtvTeletext && pgno < TELETEXT_MAX_PAGE_NUMBER && pgno > TELETEXT_MIN_PAGE_NUMBER) {
            res = vbi_fetch_vt_page(ctx->vbi, page,
                                    ev->ev.ttx_page.pgno,
                                    ev->ev.ttx_page.subno,
                                    VBI_WST_LEVEL_3p5, TELETEXT_ROW, TRUE, &pageType);
            if (!res) {
                LOGE("%s, dtv page cannot get now!\n",__FUNCTION__);
            } else {
                if (pageType & 0x8000) {//dtv subtitle
                    for (int i=0;i<TELETEXT_SUBTITLE_MAX_NUMBER;i++) {
                        LOGI("%s, save dtv subtitle page:%d, gotoDtvSubtitleFlg:%d\n",__FUNCTION__, pgno, ctx->gotoDtvSubtitleFlg);
                        if (!isRedundantSubtitlePage(gVBIStatus.dtvSubtitlePage, pgno, i) && gVBIStatus.dtvSubtitlePage[i] == 0 && dtvSubtitlePageInsertFlag == 0) {
                             gVBIStatus.dtvSubtitlePage[i] = pgno;
                             dtvSubtitlePageInsertFlag = i;
                        }

                        if (ctx->gotoDtvSubtitleFlg && gVBIStatus.dtvSubtitlePage[i] != 0) {
                            ctx->gotoPage = gVBIStatus.dtvSubtitlePage[i];
                            ctx->gotoDtvSubtitleFlg = FALSE;
                        }
                    }
                }
            }
        }
        if (ctx->dtvTeletext);selectSortSubtitlePage(gVBIStatus.dtvSubtitlePage,dtvSubtitlePageInsertFlag);

    }

    if (ctx->pageState == (TeletextPageState)TT2_DISPLAY_STATE) {
        if (pgno != ctx->gotoPage) {
            LOGE("%s, return, page(%d), current page(%d), isSubtitle:%d subtitlePageNumberShowTimeOutFlag:%d\n",__FUNCTION__, pgno, ctx->gotoPage, ctx->isSubtitle, ctx->subtitlePageNumberShowTimeOutFlag);
            free(page);
            #ifdef TELETEXT_GRAPHICS_SUBTITLE_PAGENUMBER_BLACKGROUND
            if (ctx->subtitlePageNumberShowTimeOutFlag && std::chrono::system_clock::now() - ctx->lasttime >= std::chrono::seconds(TELETEXT_SUBTITLE_PAGE_SHOW_TIME+1)) {
                TeletextParser *parser = TeletextParser::getCurrentInstance();
                LOGE("%s, return, page(%d), current page(%d), ctx->isSubtitle:%d, ctx->subtitlePageNumberShowTimeOutFlag:%d\n",__FUNCTION__, pgno, ctx->gotoPage, ctx->isSubtitle, ctx->subtitlePageNumberShowTimeOutFlag);
                parser->fetchVbiPageLocked(ctx->gotoPage, ctx->subPageNum);
            }
            #endif
            return;
        }
        if (ctx->subPageNum != AM_TT2_ANY_SUBNO && subPgno != ctx->subPageNum) {
            LOGE("%s, return, subpage(%d), current subpage(%d)\n",__FUNCTION__, subPgno, ctx->subPageNum);
            free(page);
            return;
        }
        ctx->acceptSubPage = subPgno;
    }
    vbi_teletext_set_current_page(ctx->vbi, vbi_dec2bcd(ctx->gotoPage), vbi_dec2bcd(ctx->subPageNum));

    if (ctx->regionId > 0) {
        vbi_teletext_set_default_region(ctx->vbi, ctx->regionId);
    }

    res = vbi_fetch_vt_page(ctx->vbi, page,
                            ev->ev.ttx_page.pgno,
                            ev->ev.ttx_page.subno,
                            VBI_WST_LEVEL_3p5, TELETEXT_ROW, TRUE, &pageType);

    if (!res) {
        LOGE("%s, return, page get error\n",__FUNCTION__);
        free(page);
        return;
    }
    //now has 0x8000, 0x4000, 0x4000 may not subtitle ,so need 0x8000.
    if ((pageType == TELETEX_WARN_PAGE_1) || (pageType == TELETEX_WARN_PAGE_2) || ((pageType == TELETEX_WARN_PAGE_3))) {
        LOGE("%s, return, warn page \n",__FUNCTION__);
        free(page);
        return;
    }
    ctx->isSubtitle = pageType & 0x8000; //pageType subtitle: 32768, graphics:0
    ctx->pageType = pageType;
    int subArray[36];
    int len = 36;
    bool ret = vbi_get_sub_info(ctx->vbi, vbi_dec2bcd(ctx->gotoPage), subArray, &len);

    if (ctx->gotoPage > 0 && pgno == ctx->gotoPage && ctx->isSubtitle) {
        ctx->dispUpdate = false;
        ctx->pageState = (TeletextPageState)TT2_DISPLAY_STATE;
        #ifdef SUPPORT_LOAD_ANIMATION
        TeletextParser *parser = TeletextParser::getCurrentInstance();
        if (parser != nullptr) {
            parser->notifyTeletextLoadState((int)ctx->pageState);
        }
        #endif
    }
    LOGI("TT event handler:isSubtitle:%d, pgno %d, subno: %d, parser->gotoPage %d, parser->pageNum %d, parser->subPageNum %d, pageType:%d,pageState:%d,dispUpdate:%d, subpageLen:%d\n",
         ctx->isSubtitle, pgno, vbi_bcd2dec(ev->ev.ttx_page.subno), ctx->gotoPage, ctx->pageNum, ctx->subPageNum, pageType & 0x8000, ctx->pageState, ctx->dispUpdate, len);


    if (gVBIStatus.atvSubtitlePage[0] == 0 && ctx->isSubtitle) {
        LOGD("%s, gVBIStatus.atvSubtitlePage[0]:%d\n",__FUNCTION__, gVBIStatus.atvSubtitlePage[0]);
        gVBIStatus.atvSubtitlePage[0] = pgno;
    }

    if (gVBIStatus.dtvSubtitlePage[0] == 0 && ctx->isSubtitle) {
        LOGD("%s, gVBIStatus.dtvSubtitlePage[0]:%d\n",__FUNCTION__, gVBIStatus.dtvSubtitlePage[0]);
        gVBIStatus.dtvSubtitlePage[0] = pgno;
    }

    if (ctx->dispUpdate) {
        if (ctx->isSubtitle) {
            free(page);
            return;
       }

       if (pgno == ctx->gotoPage && ctx->pageState == TT2_SEARCH_STATE) {
           ctx->pageState = (TeletextPageState)TT2_DISPLAY_STATE;
           ctx->searchDir = 1;
       }
       setNavigatorPageNumber(page,ctx->gotoPage);
       vbi_set_subtitle_flag(ctx->vbi, 0, ctx->subtitleMode, TELETEXT_USE_SUBTITLESERVER);

       #ifdef SUPPORT_LOAD_ANIMATION
       TeletextParser *parser = TeletextParser::getCurrentInstance();
       if (parser != nullptr && ctx->pageState != TT2_INPUT_STATE) {
           parser->notifyTeletextLoadState((int)ctx->pageState);
           free(page);
           return;
       }
       #endif
       static auto start = std::chrono::system_clock::now();
       auto end =  std::chrono::system_clock::now();
       std::chrono::duration<double> diff = end - start;
       if (diff < std::chrono::milliseconds(40) && ctx->gotoPage > 0 && pgno != ctx->gotoPage) {
            free(page);
            return;
       }
        start = end;
    } else {
        //some stream some page is subtitle(isSubtitle true), while have many subpages.
        //now by sub info to determine if show subpage bar
        //vbi_set_subtitle_flag determine if show subpage bar.
      /*  if (len <= 1) {
            vbi_set_subtitle_flag(ctx->vbi, 1);
        } else {
            vbi_set_subtitle_flag(ctx->vbi, 0);
            ctx->dispUpdate = true;
        }*/
        vbi_set_subtitle_flag(ctx->vbi, 1, ctx->subtitleMode, TELETEXT_USE_SUBTITLESERVER);

        if (!ctx->isSubtitle) {  //default subtitle
            free(page);
            return;
        }
        //filter multi-language subtitle  data
        if (ctx->gotoPage > 0 && pgno != ctx->gotoPage) {
            free(page);
            return;
        }

        #ifdef SUPPORT_LOAD_ANIMATION
        // because in the zte project, when the page is subtitle, the libavbi doesn't get the top row data.
        // so need to reget the top row data after vbi_set_subtitle_flag .
        if (ctx->subtitleMode == 1) {
            vbi_fetch_vt_page(ctx->vbi, page,
                            ev->ev.ttx_page.pgno,
                            ev->ev.ttx_page.subno,
                            VBI_WST_LEVEL_3p5, TELETEXT_ROW, TRUE, &pageType);
        }
        #endif
        //gVBIStatus.atvSubtitlePage[0] = pgno;
    }

#ifdef DEBUG
    fprintf(stderr, "\nSaving res=%d dy0=%d dy1=%d...\n",res, page->dirty.y0, page->dirty.y1);
    fflush(stderr);

    if (!vbi_export_stdio(ctx->ex, stderr, page))
        fprintf(stderr, "failed: %s\n", vbi_export_errstr(ctx->ex));
#endif

    vpt = vbi_classify_page(ctx->vbi, ev->ev.ttx_page.pgno, &subno, &lang);
    chopTop = ctx->chopTop || ((page->rows > 1) && (vpt == VBI_SUBTITLE_PAGE));

    LOGI("%d x %d page chop:%d\n", page->columns, page->rows, chopTop);
    vbi_teletext_set_current_page(ctx->vbi,ev->ev.ttx_page.pgno, ev->ev.ttx_page.subno);

    if (ctx->totalPages < MAX_BUFFERED_PAGES) {

        newPages = (TeletextPage*)realloc(ctx->pages, (ctx->totalPages+1)*sizeof(TeletextPage));

        if (newPages != nullptr) {
            TeletextPage *cur_page = newPages + ctx->totalPages;
            ctx->pages = newPages;
            cur_page->subRect = (AVSubtitleRect *)calloc(1, sizeof(*cur_page->subRect));
            cur_page->pts = ctx->pts;
            cur_page->pgno = ev->ev.ttx_page.pgno;
            cur_page->subno = ev->ev.ttx_page.subno;

            ctx->disPlayBackground = (!ctx->dispMode && ctx->pageState == TT2_DISPLAY_STATE && !ctx->isSubtitle)? 0:1;
            //LOGI("%s, ret cur_page->pts:0x%x\n",__FUNCTION__,cur_page->pts);
            if (cur_page->subRect) {
                res = (ctx->formatId == 0) ?
                    genSubBitmap(ctx, cur_page->subRect, page, chopTop) :
                    genSubText  (ctx, cur_page->subRect, page, chopTop);
                if (res < 0) {
                    if (cur_page->subRect->pict.data[0] != nullptr) {
                        free(cur_page->subRect->pict.data[0]);
                        cur_page->subRect->pict.data[0] = nullptr;
                    }
                    if (cur_page->subRect->pict.data[1] != nullptr) {
                        free(cur_page->subRect->pict.data[1]);
                        cur_page->subRect->pict.data[1] = nullptr;
                    }
                    free(cur_page->subRect);
                    ctx->handlerRet = res;
                } else {
                    ctx->pages[ctx->totalPages++] = *cur_page;
                }
            } else {
                ctx->handlerRet = -1;//AVERROR(ENOMEM);
            }
        } else {
            ctx->handlerRet = -1;//AVERROR(ENOMEM);
        }
    } else {
        //TODO: If multiple packets contain more than one page, pages may got queued up, and this may happen...
        LOGI("Buffered too many pages, dropping page %s.\n", pgNoStr);
        ctx->handlerRet = -1;//AVERROR(ENOSYS);
    }

#ifdef NEED_CACHE_ZVBI_STATUS
    if (ctx->handlerRet >= 0 && ctx->pageState == TT2_DISPLAY_STATE) {
        gVBIStatus.lastShowingPage = pgno; // we only record the success page
    }
#endif

    vbi_unref_page(page);
    free(page);
}

TeletextParser::TeletextParser(std::shared_ptr<DataSource> source) {
    mDataSource = source;
    mParseType = TYPE_SUBTITLE_DVB_TELETEXT;
    mGotoPageNum = 0;
    mIndex = 0;
    mDumpSub = 0;
    mUpdateParamCount = 0;

    initContext();
    checkDebug();
    sInstance = this;
    mTextBack = (unsigned char  *)malloc(TELETEXT_TEXT_HEIGHT * TELETEXT_GRAPHIC_WIDTH * sizeof(uint32_t));
    mBarBack = (unsigned char  *)malloc(TELETEXT_BAR_HEIGHT * TELETEXT_GRAPHIC_WIDTH * sizeof(uint32_t));
}

TeletextParser *TeletextParser::sInstance = nullptr;

TeletextParser *TeletextParser::getCurrentInstance() {
    return TeletextParser::sInstance;
}


TeletextParser::~TeletextParser() {
    LOGI("%s", __func__);
    stopParse();
    sInstance = nullptr;

    // mContext need protect. accessed by other api or the ttThread.
    mMutex.lock();
    if (mTextBack != nullptr) {
        free(mTextBack);
    }
    if (mBarBack != nullptr) {
        free(mBarBack);
    }
    if (mContext != nullptr) {
        mContext->dispUpdate = 0;
        LOGI("lines_total=%u\n", mContext->linesProcessed);
        while (mContext->totalPages) {
            if (mContext->pages[--mContext->totalPages].subRect) {
                free(mContext->pages[mContext->totalPages].subRect->pict.data[0]);
                free(mContext->pages[mContext->totalPages].subRect->pict.data[1]);
                free(mContext->pages[mContext->totalPages].subRect->ass);
                free(mContext->pages[mContext->totalPages].subRect);
            }
        }
        free(mContext->pages);

#ifdef NEED_CACHE_ZVBI_STATUS
//        vbi_event_handler_remove(mContext->vbi, tt2TimeUpdate);
//        vbi_event_handler_remove(mContext->vbi, handler);
        ALOGD("~mContext->pageNum=%d mContext->gotoPage=%d, lastGotoPage=%d", mContext->pageNum, mContext->gotoPage, gVBIStatus.lastShowingPage);
#else
        vbi_decoder_delete(mContext->vbi);
#endif


        mContext->vbi = nullptr;
        mContext->pts = AV_NOPTS_VALUE;
        mContext->mixVideoState = TT2_MIX_BLACK;
        mContext->dispUpdate = 0;
        mContext->regionId = 0;
        delete mContext;
        mContext = nullptr;
    }
    mMutex.unlock();
}

static inline int generateNormalDisplay(AVSubtitleRect *subRect, unsigned char *des, uint32_t *src, int width, int height) {
    LOGE(" generateNormalDisplay width = %d, height=%d\n",width, height);
    int mt =0, mb =0;
    int ret = DATA_VALID_AND_BLANK;
    TeletextParser *parser = TeletextParser::getCurrentInstance();

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            src[(y*width) + x] =
                    ((uint32_t *)subRect->pict.data[1])[(subRect->pict.data[0])[y*width + x]];
            if ((ret != 0) && ((src[(y*width) + x] != 0) && (src[(y*width) + x] != 0xff000000))) {
                ret = 0;
            }
            des[(y*width*4) + x*4] = src[(y*width) + x] & 0xff;
            des[(y*width*4) + x*4 + 1] = (src[(y*width) + x] >> 8) & 0xff;
            des[(y*width*4) + x*4 + 2] = (src[(y*width) + x] >> 16) & 0xff;

#ifdef SUPPORT_LOAD_ANIMATION
            if (parser->mContext->isSubtitle && parser->mContext->subtitleMode == TT2_GRAPHICS_MODE) {
                des[(y*width*4) + x*4 + 3] = 0xff;
            } else {
                des[(y*width*4) + x*4 + 3] = (src[(y*width) + x] >> 24) & 0xff;   //color style
            }
#else
            des[(y*width*4) + x*4 + 3] = (src[(y*width) + x] >> 24) & 0xff;   //color style
#endif
        }
    }
    if (!(parser->mContext->isSubtitle) && (height == (TELETEXT_HEAD_HEIGHT + TELETEXT_TEXT_HEIGHT + TELETEXT_BAR_HEIGHT))) {
        ret = 0;
        memcpy(parser->mTextBack, &des[TELETEXT_HEAD_HEIGHT*width*4], (TELETEXT_TEXT_HEIGHT*width*4));
        memcpy(parser->mBarBack, &des[(TELETEXT_HEAD_HEIGHT+TELETEXT_TEXT_HEIGHT)*width*4], (TELETEXT_BAR_HEIGHT*width*4));
    } else if (parser->mContext->isSubtitle) {
        ret = 0; // subtitle no need copy. this is for transparent subtitle[clean subtitle]
    }
    return ret;
}

static inline int generateSearchDisplay(AVSubtitleRect *subRect, unsigned char *des, uint32_t *src, int width, int height) {
    //in this mode , just fresh head and bottom.
    LOGE(" generateSearchDisplay height = %d, width = %d\n",height, width);
    int maxHeight = TELETEXT_HEAD_HEIGHT+TELETEXT_TEXT_HEIGHT + TELETEXT_BAR_HEIGHT;

    if (height < TELETEXT_TEXT_HEIGHT+TELETEXT_HEAD_HEIGHT+TELETEXT_BAR_HEIGHT) {
        ALOGD("skip incomplete page..");
        return -1;
    }

    TeletextParser *parser = TeletextParser::getCurrentInstance();
    if (parser->mContext->isSubtitle) {
        ALOGD("skip subtitle page for search..");
        return -1;
    }

    int hasData = 0;
    for (int y = 0; y < TELETEXT_HEAD_HEIGHT; y++) {
        for (int x = 0; x < width; x++) {
            src[(y*width) + x] =
                    ((uint32_t *)subRect->pict.data[1])[(subRect->pict.data[0])[y*width + x]];
            des[(y*width*4) + x*4] = src[(y*width) + x] & 0xff;
            des[(y*width*4) + x*4 + 1] = (src[(y*width) + x] >> 8) & 0xff;
            des[(y*width*4) + x*4 + 2] = (src[(y*width) + x] >> 16) & 0xff;
            des[(y*width*4) + x*4 + 3] = (src[(y*width) + x] >> 24) & 0xff;   //color style
            if (src[(y*width) + x] != 0) hasData++;
        }
     }

    if (hasData == 0) {
        ALOGD("skip empty page..");
        return -1;
    }
    memset(&des[TELETEXT_HEAD_HEIGHT*width*4], 0x00, (TELETEXT_TEXT_HEIGHT+TELETEXT_BAR_HEIGHT)*width*4);

    if (height < maxHeight) {
        return 0;// enlarge mode, do not need bottom
    }

    hasData = 0;
    for (int y = (TELETEXT_HEAD_HEIGHT + TELETEXT_TEXT_HEIGHT); y < height; y++) {
        for (int x = 0; x < width; x++) {
            src[(y*width) + x] =
                    ((uint32_t *)subRect->pict.data[1])[(subRect->pict.data[0])[y*width + x]];
            des[(y*width*4) + x*4] = src[(y*width) + x] & 0xff;
            des[(y*width*4) + x*4 + 1] = (src[(y*width) + x] >> 8) & 0xff;
            des[(y*width*4) + x*4 + 2] = (src[(y*width) + x] >> 16) & 0xff;
            des[(y*width*4) + x*4 + 3] = (src[(y*width) + x] >> 24) & 0xff;   //color style
            if (src[(y*width) + x] != 0) hasData++;
        }
     }
     ALOGD("skip empty page end.. hasData=%d", hasData);
     if (hasData < (width)) {
         return -1;
     }
     return 0;
}

static inline int generateInputDisplay(AVSubtitleRect *subRect, unsigned char *des, uint32_t *src, int width, int height) {
    LOGE(" generateInputDisplay height = %d, width = %d\n",height, width);
    TeletextParser *parser = TeletextParser::getCurrentInstance();
    int hasData = 0;
    int maxHeight = TELETEXT_HEAD_HEIGHT+TELETEXT_TEXT_HEIGHT + TELETEXT_BAR_HEIGHT;
    for (int y = 0; y < TELETEXT_HEAD_HEIGHT; y++) {
        for (int x = 0; x < width; x++) {
            src[(y*width) + x] =
                    ((uint32_t *)subRect->pict.data[1])[(subRect->pict.data[0])[y*width + x]];
            des[(y*width*4) + x*4] = src[(y*width) + x] & 0xff;
            des[(y*width*4) + x*4 + 1] = (src[(y*width) + x] >> 8) & 0xff;
            des[(y*width*4) + x*4 + 2] = (src[(y*width) + x] >> 16) & 0xff;
            des[(y*width*4) + x*4 + 3] = (src[(y*width) + x] >> 24) & 0xff;   //color style
            if (src[(y*width) + x] != 0) hasData++;
        }
    }

    if (parser->mContext->isSubtitle || hasData == 0) {
        ALOGD("skip subtitle page for search..");
        return -1;
    }


    if (height < maxHeight) {
        memcpy(&des[TELETEXT_HEAD_HEIGHT*width*4], TeletextParser::getCurrentInstance()->mTextBack, ((maxHeight/2-TELETEXT_HEAD_HEIGHT)*width*4));
        return 0;// enlarge mode, do not need bottom
    }
    memcpy(&des[TELETEXT_HEAD_HEIGHT*width*4], TeletextParser::getCurrentInstance()->mTextBack, (TELETEXT_TEXT_HEIGHT*width*4));
    memcpy(&des[(TELETEXT_HEAD_HEIGHT+TELETEXT_TEXT_HEIGHT)*width*4], TeletextParser::getCurrentInstance()->mBarBack, (TELETEXT_BAR_HEIGHT*width*4));
    return 0;
}


int TeletextParser::saveTeletextGraphicsRect2Spu(std::shared_ptr<AML_SPUVAR> spu, AVSubtitleRect *subRect) {
    LOGI("save_display_set\n");
    int resx = subRect->w;
    int resy = subRect->h;
    int error =  DATA_VALID_AND_BLANK;
    char filename[32];

    uint32_t *pbuf;
    spu->spu_width = resx;
    spu->spu_height = resy;
    spu->disPlayBackground = mContext->disPlayBackground;

    pbuf = (uint32_t *)malloc(resx * resy * sizeof(uint32_t));
    if (!pbuf) {
        LOGI("malloc width height failed!");
        return -1;
    }
    if (spu->spu_data != nullptr) {
        ALOGE("Error, resued spu data, we designed not resuable!!\n\n\n\n\n!!");
        free(spu->spu_data);
    }
    spu->buffer_size = resx * resy * sizeof(uint32_t);
    spu->spu_data = (unsigned char *)malloc(spu->buffer_size);
    if (!spu->spu_data) {
        LOGI("malloc buffer_size failed!\n");
        free(pbuf);
        return -1;
    }
   switch (mContext->pageState) {
        case TT2_DISPLAY_STATE:
            error = generateNormalDisplay(subRect, spu->spu_data, pbuf, resx, resy);
            break;
        case TT2_SEARCH_STATE:
            error =generateSearchDisplay(subRect, spu->spu_data, pbuf, resx, resy);
            break;
        case TT2_INPUT_STATE:
            error =generateInputDisplay(subRect, spu->spu_data, pbuf, resx, resy);
            break;
        default:
            break;
   }
   if (mDumpSub) {
        char filename[32];
        snprintf(filename, sizeof(filename), "./data/subtitleDump/tt(%d)", mIndex);
        save2BitmapFile(filename, (uint32_t *)spu->spu_data, resx, resy);
    }
    free(pbuf);
    return error;
}
int TeletextParser::saveDisplayRect2Spu(std::shared_ptr<AML_SPUVAR> spu, AVSubtitleRect *subRect) {
    LOGI("save_display_set\n");
    int resx = subRect->w;
    int resy = subRect->h;
    int error =  DATA_VALID_AND_BLANK;
    char filename[32];

    uint32_t *pbuf;

    spu->spu_width = resx;
    spu->spu_height = resy;
    spu->disPlayBackground = mContext->disPlayBackground;

    pbuf = (uint32_t *)malloc(resx * resy * sizeof(uint32_t));
    if (!pbuf) {
        LOGI("malloc width height failed!");
        return -1;
    }

    spu->buffer_size = resx * resy * sizeof(uint32_t);
    if (spu->spu_data != nullptr) {
        ALOGE("Error, resued spu data, we designed not resuable!!\n\n\n\n\n!!");
        free(spu->spu_data);
    }
    spu->spu_data = (unsigned char *)malloc(spu->buffer_size);
    if (!spu->spu_data) {
        LOGI("malloc buffer_size failed!\n");
        free(pbuf);
        return -1;
    }

    for (int y = 0; y < resy; y++) {
        for (int x = 0; x < resx; x++) {
            pbuf[(y*resx) + x] =
                    ((uint32_t *)subRect->pict.data[1])[(subRect->pict.data[0])[y*resx + x]];
            //if satisfy this check: the data is valid.And then the data if invalid and be filtered.
            //when the error is 2, it indicate that the data is valid but just background. need to free.
            //when the error is 0, it indicate that the data is valid.
            //when the function reburn -1, it indicate the data is invalid but don't need to free.
            if ((error != 0) && ((pbuf[(y*resx) + x] != 0) && (pbuf[(y*resx) + x] != 0xff000000))) {
                error = 0;
            }
            spu->spu_data[(y*resx*4) + x*4] = pbuf[(y*resx) + x] & 0xff;
            spu->spu_data[(y*resx*4) + x*4 + 1] = (pbuf[(y*resx) + x] >> 8) & 0xff;
            spu->spu_data[(y*resx*4) + x*4 + 2] = (pbuf[(y*resx) + x] >> 16) & 0xff;
            spu->spu_data[(y*resx*4) + x*4 + 3] = (pbuf[(y*resx) + x] >> 24) & 0xff;   //color style
        }
    }

    if (mDumpSub) {
        snprintf(filename, sizeof(filename), "./data/subtitleDump/tt(%lld)", spu->pts);
        save2BitmapFile(filename, (uint32_t *)spu->spu_data, resx, resy);
    }

    free(pbuf);
    return error;
}

//tt2TimeUpdate only below 2 phenomenon can call fetchVbiPageLocked every 1s.
//and update time every 1s.
//1.ttx grahphics
//2.subtitle(pageType:32768,issubtitle:1) which have many subpage
int TeletextParser::fetchVbiPageLocked(int pageNum, int subPageNum) {
    LOGI("%s pgno:%d, subPgno:%d\n", __FUNCTION__, pageNum, subPageNum);

    TeletextPage *newPages;
    vbi_page *page = (vbi_page *)malloc(sizeof(vbi_page));
    int res;
    char pgNoStr[12];
    vbi_subno subno;
    vbi_page_type vpt;
    int chopTop;
    int pageType;
    char *lang;

    if (page == nullptr) return -1;
    memset(page, 0, sizeof(vbi_page));

#ifdef NEED_CACHE_ZVBI_STATUS
    // TODO: monitor success fetch timeout.
#endif

    if (!mContext->vbi) {
        free(page);
        LOGE("%s error! ctx vbi is null\n", __FUNCTION__);
        return -1;
    }
    vbi_teletext_set_current_page(mContext->vbi, vbi_dec2bcd(pageNum), vbi_dec2bcd(subPageNum));

    if (mContext->regionId > 0) {
        vbi_teletext_set_default_region(mContext->vbi, mContext->regionId);
    }

    res = vbi_fetch_vt_page(mContext->vbi, page, vbi_dec2bcd(pageNum/*ctx->pageNum*/),
                            vbi_dec2bcd(subPageNum)/*ctx->subPageNum*/, VBI_WST_LEVEL_3p5, TELETEXT_ROW, TRUE, &pageType);

    if (!res) {
        LOGI("%s, return, page get error\n",__FUNCTION__);
        free(page);
        return -1;
    }

    int subArray[36];
    int len = 36;
    bool ret = vbi_get_sub_info(mContext->vbi, vbi_dec2bcd(pageNum), subArray, &len);
    //this only show graphics, and smoe graphics have subtitle.
   mContext->isSubtitle = pageType & 0x8000;
   mContext->pageType = pageType;
#ifndef SUPPORT_LOAD_ANIMATION
    if (!mContext->atvTeletext && !mContext->dtvTeletext && mContext->isSubtitle && len <= 1) {
        LOGE("%s, teletext subtitle, not page, return!\n",__FUNCTION__);
        free(page);
        return -1;
    } else {
        vbi_set_subtitle_flag(mContext->vbi, 0, mContext->subtitleMode, TELETEXT_USE_SUBTITLESERVER); //set 0, show subpage bar
    }
#endif
    mContext->pageState = TT2_DISPLAY_STATE;
    mContext->searchDir = 1;

    vpt = vbi_classify_page(mContext->vbi, vbi_dec2bcd(pageNum), &subno, &lang);
    chopTop = mContext->chopTop || ((page->rows > 1) && (vpt == VBI_SUBTITLE_PAGE));
    vbi_teletext_set_current_page(mContext->vbi, vbi_dec2bcd(mContext->gotoPage), vbi_dec2bcd(mContext->subPageNum));
    setNavigatorPageNumber(page,mContext->gotoPage);
    vbi_set_subtitle_flag(mContext->vbi, mContext->isSubtitle, mContext->subtitleMode, TELETEXT_USE_SUBTITLESERVER);
    if (mContext->totalPages < MAX_BUFFERED_PAGES) {
        LOGI("%s, totalPages:%d\n",__FUNCTION__, mContext->totalPages);
        newPages = (TeletextPage*)realloc(mContext->pages, (mContext->totalPages+1)*sizeof(TeletextPage));

        if (newPages != nullptr) {
            TeletextPage *cur_page = newPages + mContext->totalPages;
            mContext->pages = newPages;
            cur_page->subRect = (AVSubtitleRect *)calloc(1, sizeof(*cur_page->subRect));
            cur_page->pts = mContext->pts;
            cur_page->pgno = vbi_dec2bcd(pageNum);
            cur_page->subno = vbi_dec2bcd(0);
            mContext->disPlayBackground = (!mContext->dispMode && mContext->pageState == TT2_DISPLAY_STATE && !mContext->isSubtitle)? 0:1;
            //LOGI("%s, ret cur_page->pts:0x%x\n",__FUNCTION__,cur_page->pts);
            if (cur_page->subRect) {
                LOGI("%s,formatId:%d\n",__FUNCTION__, mContext->formatId);
                res = (mContext->formatId == 0) ?
                    genSubBitmap(mContext, cur_page->subRect, page, chopTop) :
                    genSubText  (mContext, cur_page->subRect, page, chopTop);
                if (res < 0) {
                    free(cur_page->subRect);
                    mContext->handlerRet = res;
                } else {
                    mContext->pages[mContext->totalPages++] = *cur_page;
                }
            } else {
                mContext->handlerRet = -1;//AVERROR(ENOMEM);
            }
        } else {
            mContext->handlerRet = -1;//AVERROR(ENOMEM);
        }
    } else {
        mContext->handlerRet = -1;//AVERROR(ENOSYS);
    }
    free(page);

#ifdef NEED_CACHE_ZVBI_STATUS
    if (mContext->handlerRet >= 0) {
        gVBIStatus.lastShowingPage = pageNum; // we only record the success page
        gVBIStatus.searchLastPageFinished();
    } else {
        if (gVBIStatus.isSearchLastPageTimeout()) {
            goHomeLocked();
        }
    }
#endif
    return 1;
}


int TeletextParser::goHomeLocked() {
    LOGI("goHome\n");

    if (!mContext) {
        LOGI("goHome ctx is null\n");
        return -1;
    }

    int cached;
    vbi_page *page = (vbi_page *)malloc(sizeof(vbi_page));
    vbi_link link;
    int pageType;
    LOGI("goHome ctx is %p\n", mContext->vbi);
    if (page == nullptr) return -1;
    memset(page, 0, sizeof(vbi_page));

    if (mContext->lockSubpg == 1) {
        mContext->lockSubpg = 0;
    }

    mContext->gotoPage = AM_TT2_HOME_PAGENO;
    mContext->subPageNum = AM_TT2_ANY_SUBNO;
    mContext->acceptSubPage = getSubPageInfoLocked();
    mContext->dispUpdate = 1;
    mContext->pageState = TT2_SEARCH_STATE;
    mContext->searchDir = 1;
    if (NULL != mContext->vbi) {
        vbi_set_subtitle_page(mContext->vbi, vbi_dec2bcd(AM_TT2_HOME_PAGENO));
    }
    cached = fetchVbiPageLocked(AM_TT2_HOME_PAGENO, mContext->acceptSubPage);
    if (!cached) {
        free(page);
        LOGI("%s, return, page get error\n",__FUNCTION__);
        return TT2_FAILURE;
    }
    free(page);
    mContext->gotoGraphicsSubtitlePage = mContext->gotoPage;
    return TT2_SUCCESS;
}

int TeletextParser::fetchCountPageLocked(int dir, int count) {
    LOGI("%s dir:%d\n", __FUNCTION__, dir);
    int pgno, subno;

    if (!mContext) {
        LOGI("%s, error! Context is null\n", __FUNCTION__);
        return TT2_FAILURE;
    }
    LOGI("%s,  gotopage:%d\n", __FUNCTION__, mContext->gotoPage);

    if (mContext->lockSubpg == 1) {
        mContext->lockSubpg = 0;
    }

    if (mContext->gotoPage >= 100 && mContext->gotoPage <=899) {
        tt2AddBackCachePageLocked(mContext->gotoPage, mContext->subPageNum);
    }

    if (mCurrentNavigatorPage.size() != NAVIGATOR_COLORBAR_LINK_SIZE) {
        int num = 0;
        for (int i =0; i < count; i++) {
            pgno  = vbi_dec2bcd(mContext->gotoPage);
            subno = AM_TT2_ANY_SUBNO;

            num++;
            if (vbi_get_next_pgno(mContext->vbi, dir, &pgno, &subno)) {
                mContext->pageState = TT2_SEARCH_STATE;
                mContext->searchDir = dir;
                if (num == count) {
                    vbi_set_subtitle_page(mContext->vbi, pgno);
                    int res = fetchVbiPageLocked(vbi_bcd2dec(pgno), subno);
                    if (!res) {
                        LOGE("%s, return, page get error\n",__FUNCTION__);
                        return TT2_FAILURE;
                    }
                }

                mContext->pageNum = vbi_bcd2dec(pgno);
                mContext->subPageNum = AM_TT2_ANY_SUBNO;
                mContext->acceptSubPage = getSubPageInfoLocked();
                mContext->gotoPage = mContext->pageNum;
                mContext->dispUpdate = 1;
            } else {
                LOGI("%s, return, page get error\n",__FUNCTION__);
                return TT2_FAILURE;
            }

        }
    } else {
        pgno = mCurrentNavigatorPage[count-1].pageNo;
        subno = mCurrentNavigatorPage[count-1].subPageNo;
        if (0 == pgno) {
            LOGI("%s, error! don't get the navigator pageno\n", __FUNCTION__);
            return TT2_FAILURE;
        }
        vbi_set_subtitle_page(mContext->vbi, pgno);
        mContext->pageNum = vbi_bcd2dec(pgno);
        mContext->subPageNum = vbi_bcd2dec(subno);;
        mContext->acceptSubPage = getSubPageInfoLocked();
        mContext->gotoPage = mContext->pageNum;
        mContext->dispUpdate = 1;
    }
    mContext->gotoGraphicsSubtitlePage = mContext->gotoPage;
    return TT2_SUCCESS;
}

int TeletextParser::nextPageLocked(int dir, bool fetch) {
    LOGI("%s dir:%d\n", __FUNCTION__, dir);
    int pgno, subno;

    if (!mContext) {
        LOGE("%s, error! Context is null\n", __FUNCTION__);
        return -1;
    }

    if (!mContext->vbi) {
        LOGE("%s error! ctx vbi is null\n", __FUNCTION__);
        return -1;
    }

    if (mContext->atvTeletext && mContext->subtitleMode == TT2_SUBTITLE_MODE && gVBIStatus.atvSubtitlePage[gVBIStatus.subtitlePageId] >= 100) {
        mContext->transparentBackground = 0;
        mContext->opacity = mContext->transparentBackground ? 0 : 255;
        mContext->subtitleMode = TT2_GRAPHICS_MODE;
        mContext->gotoPage = gVBIStatus.atvSubtitlePage[gVBIStatus.subtitlePageId];
        LOGI("%s,  atvSubtitlePage[%d]:%d\n", __FUNCTION__, gVBIStatus.subtitlePageId, gVBIStatus.atvSubtitlePage[gVBIStatus.subtitlePageId]);
    }

    if (mContext->dtvTeletext && mContext->subtitleMode == TT2_SUBTITLE_MODE && gVBIStatus.dtvSubtitlePage[gVBIStatus.subtitlePageId] >= 100) {
        mContext->transparentBackground = 0;
        mContext->opacity = mContext->transparentBackground ? 0 : 255;
        mContext->subtitleMode = TT2_GRAPHICS_MODE;
        mContext->gotoPage = gVBIStatus.dtvSubtitlePage[gVBIStatus.subtitlePageId];
        LOGI("%s,  dtvSubtitlePage[%d]:%d\n", __FUNCTION__, gVBIStatus.subtitlePageId, gVBIStatus.dtvSubtitlePage[gVBIStatus.subtitlePageId]);
    }

    LOGI("%s,  gotopage:%d\n", __FUNCTION__, mContext->gotoPage);

    if (mContext->lockSubpg == 1) {
        mContext->lockSubpg = 0;
    }

    if (mContext->gotoPage >= 100 && mContext->gotoPage <=899) {
        tt2AddBackCachePageLocked(mContext->gotoPage, mContext->subPageNum);
    }

    pgno  = vbi_dec2bcd(mContext->gotoPage);

    subno = AM_TT2_ANY_SUBNO;
    if (getVbiNextValidPage(mContext->vbi, dir, &pgno, &subno)) {
        #ifndef SUPPORT_LOAD_ANIMATION
        mContext->pageState = TT2_SEARCH_STATE;
        #endif
        mContext->searchDir = dir;
        if (fetch) {
            vbi_set_subtitle_page(mContext->vbi, pgno);
            int res = fetchVbiPageLocked(vbi_bcd2dec(pgno), subno);
            if (!res) {
                LOGE("%s, return, page get error\n",__FUNCTION__);
                return -1;
            }
        }
        mContext->pageNum = vbi_bcd2dec(pgno);
        mContext->subPageNum = AM_TT2_ANY_SUBNO;
        mContext->acceptSubPage = getSubPageInfoLocked();
        mContext->gotoPage = mContext->pageNum;
        mContext->dispUpdate = 1;
    }
    mContext->gotoGraphicsSubtitlePage = mContext->gotoPage;
    vbi_teletext_set_current_page(mContext->vbi, vbi_dec2bcd(mContext->gotoPage), vbi_dec2bcd(mContext->subPageNum));

    return 0;
}

int TeletextParser::getSubPageInfoLocked() {

    if (!mContext) {
        LOGE("%s error! ctx is null\n", __FUNCTION__);
        return -1;
    }
    if (!mContext->vbi) {
        LOGE("%s error! ctx vbi is null\n", __FUNCTION__);
        return mContext->subPageNum;
    }

    if (mContext->lockSubpg == 1) {
        LOGE("%s, return! sub page lock！\n", __FUNCTION__);
        return mContext->subPageNum;
    }
    int subArray[VBI_LOP_SUBPAGE_LINK_LENGTH] = {0};
    int len = VBI_LOP_SUBPAGE_LINK_LENGTH;
    int pgno = vbi_dec2bcd(mContext->gotoPage);



    bool ret = vbi_get_sub_info(mContext->vbi, pgno, subArray, &len);
    LOGI("%s, ret:%d, len:%d, acceptSubPage:%d\n", __FUNCTION__, ret, len, mContext->acceptSubPage);

    //normal has 1 page, if have multi page, more than 1
    if (ret > 0 && len >= 1) {
        mContext->acceptSubPage = vbi_bcd2dec(subArray[0]);
        mContext->subPageNum = vbi_bcd2dec(subArray[0]);
    } else {
        mContext->acceptSubPage = 0;
        mContext->subPageNum = 0;
    }
    return mContext->acceptSubPage;
}



int TeletextParser::changeMixModeLocked() {

    if (!mContext) {
        LOGE("%s, error! Context is null\n", __FUNCTION__);
        return TT2_FAILURE;
    }
    LOGI("%s, transparentBackground:%d\n", __FUNCTION__, mContext->transparentBackground);

    switch (mContext->mixVideoState) {
        case TT2_MIX_BLACK:
            mContext->mixVideoState = TT2_MIX_TRANSPARENT;
            mContext->transparentBackground = 1;
            notifyMixVideoState(TT2_MIX_TRANSPARENT);
            break;
        case TT2_MIX_TRANSPARENT:
            mContext->mixVideoState = TT2_MIX_HALF_SCREEN;
            mContext->transparentBackground = 0;
            notifyMixVideoState(TT2_MIX_HALF_SCREEN);
            break;
        case TT2_MIX_HALF_SCREEN:
            mContext->mixVideoState = TT2_MIX_BLACK;
            mContext->transparentBackground = 0;
            notifyMixVideoState(TT2_MIX_BLACK);
            break;
        default:
            mContext->mixVideoState = TT2_MIX_BLACK;
            mContext->transparentBackground = 0;
            notifyMixVideoState(TT2_MIX_BLACK);
            break;
    }
    return TT2_SUCCESS;
}

void TeletextParser::notifyMixVideoState(int val) {
    if (mNotifier != nullptr) {
        ALOGD("mix video state: %d", val);
        mNotifier->onMixVideoEvent(val);
    }
}

void TeletextParser::notifyTeletextLoadState(int val) {
    if (mNotifier != nullptr) {
        ALOGD("notifyTeletextLoadState: %d", val);
        mNotifier->onSubtitleInfo(SUBTITLE_INFO_TELETEXT_LOAD_STATE, val);
    }
}

int TeletextParser::setRevealModeLocked() {
    if (!mContext) {
        LOGE("%s, error! Context is null\n", __FUNCTION__);
        return TT2_FAILURE;
    }

    if (mContext->reveal == 0) {
        mContext->reveal = 1;
    } else if (mContext->reveal == 1) {
        mContext->reveal = 0;
    }
    LOGI("%s,change to reveal:%d\n", __FUNCTION__, mContext->reveal);
    return TT2_SUCCESS;
}



int TeletextParser::setDisplayModeLocked() {
    if (!mContext) {
        LOGE("%s, error! Context is null\n", __FUNCTION__);
        return TT2_FAILURE;
    }
    LOGI("%s display mode:%d\n", __FUNCTION__, mContext->dispMode);

    if (mContext->dispMode == 0) {
        mContext->dispMode = 1;
        mContext->disPlayBackground = 1;
    } else if (mContext->dispMode == 1) {
        mContext->dispMode = 0;
        mContext->disPlayBackground = 0;
    }

    return TT2_SUCCESS;
}

int TeletextParser::setClockModeLocked() {
    if (!mContext) {
        LOGE("%s, error! Context is null\n", __FUNCTION__);
        return TT2_FAILURE;
    }
    LOGI("%s clock mode: %d\n", __FUNCTION__, mContext->dispMode);

    if (mContext->dispMode == 0) {
        mContext->dispMode = 2;
        mContext->disPlayBackground = 1;
    } else if (mContext->dispMode == 2) {
        mContext->dispMode = 0;
        mContext->disPlayBackground = 0;
    }

    return TT2_SUCCESS;
}
int TeletextParser::setDoubleHeightStateLocked() {
    if (!mContext) {
        LOGE("%s, error! Context is null\n", __FUNCTION__);
        return TT2_FAILURE;
    }
    LOGI("%s,mContext->subtitleMode:%d,mContext->gotoPage:%d \n", __FUNCTION__,mContext->subtitleMode,mContext->gotoPage);
    if (mContext->doubleHeight == DOUBLE_HEIGHT_NORMAL) {
        mContext->doubleHeight = DOUBLE_HEIGHT_TOP;
        mContext->heightIndex = 0;
    } else if (mContext->doubleHeight == DOUBLE_HEIGHT_TOP) {
        mContext->doubleHeight = DOUBLE_HEIGHT_BOTTOM;
        mContext->heightIndex = DOUBLE_HEIGHT_SCROLL_SECTION_PLUS;
    } else if (mContext->doubleHeight == DOUBLE_HEIGHT_BOTTOM) {
        mContext->doubleHeight = DOUBLE_HEIGHT_NORMAL;
        mContext->heightIndex = 0;

    }
    LOGI("%s, doubleHeight state:%d\n", __FUNCTION__, mContext->doubleHeight);

    return 0;
}

int TeletextParser::doubleScrollLocked(int dir) {

    if (!mContext) {
        LOGI("%s, ctx is null\n", __FUNCTION__);
        return TT2_FAILURE;
    }
    LOGI("%s, double height state:%d, dir:%d\n",__FUNCTION__, mContext->doubleHeight, dir);

    if (mContext->doubleHeight == DOUBLE_HEIGHT_NORMAL) {
        LOGI("%s Invalid! now teletext graphics is normal, can not scroll!\n", __FUNCTION__);
        return TT2_FAILURE;
    }

    if (dir > 0) {
        if (mContext->heightIndex > 0 && mContext->heightIndex <= DOUBLE_HEIGHT_SCROLL_SECTION_PLUS) {
            mContext->heightIndex--;
        }

    } else if (dir < 0) {
        if (mContext->heightIndex >= 0 && mContext->heightIndex <= DOUBLE_HEIGHT_SCROLL_SECTION) {
            mContext->heightIndex++;
        }

    }

    LOGI("%s, heightIndex:%d, doubleHeight:%d\n", __FUNCTION__, mContext->heightIndex, mContext->doubleHeight);
    return TT2_SUCCESS;
}


int TeletextParser::nextSubPageLocked(int dir) {
    LOGI("%s,  dir:%d\n", __FUNCTION__, dir);
    int pgno = -1, subno = -1;

    if (!mContext) {
        LOGE("%s,  ctx is null\n", __FUNCTION__);
        return -1;
    }

    if (!mContext->vbi) {
        LOGE("%s error! ctx vbi is null\n", __FUNCTION__);
        return -1;
    }

    if (!mContext->lockSubpg && !mContext->atvTeletext && !mContext->dtvTeletext) {
        LOGE("%s,  page not be lock! invalid set!\n", __FUNCTION__);
        return -1;
    }

    pgno  = vbi_dec2bcd(mContext->gotoPage);
    subno = mContext->subPageNum;

    int len = VBI_LOP_SUBPAGE_LINK_LENGTH;
    int subArray[VBI_LOP_SUBPAGE_LINK_LENGTH] = {0};
    bool ret = vbi_get_sub_info(mContext->vbi, pgno, subArray, &len);
    LOGI(" %s,  dir:%d, pgno:%d, subno:%d, subpage len:%d\n", __FUNCTION__, dir, pgno, subno, len);
    if (0 == ret) {
        LOGE("ERROR: can't get the subpage!!!");
    }
    array_sort(subArray, len);
    int index = 0;
    for (; index<len; index++) {
        if (subArray[index] == vbi_dec2bcd(subno))
            break;
    }
    if (dir >0) {
        if (index >= len-1)
            index = 0;
        else
            index ++;
    } else {
        if (index <= 0)
            index = len -1;
        else
            index --;
    }
    subno = subArray[index];
    //mContext->pageState = TT2_SEARCH_STATE;
    mContext->searchDir = dir;
    vbi_set_subtitle_page(mContext->vbi, pgno);
    int res = fetchVbiPageLocked(vbi_bcd2dec(pgno), vbi_bcd2dec(subno));
    if (!res) {
        LOGE("%s, return, page fetch error\n",__FUNCTION__);
        return -1;
    }

    LOGI("%s,vbi pgno:%d, subno:%d\n", __FUNCTION__, pgno, subno);
    mContext->pageNum = vbi_bcd2dec(pgno);
    mContext->subPageNum = vbi_bcd2dec(subno);
    mContext->acceptSubPage = vbi_bcd2dec(subno);
    mContext->gotoPage = mContext->pageNum;
    mContext->dispUpdate = 1;
    return 0;
}

int TeletextParser::gotoDefaultAtvSubtitleLocked(int atvSubtitlepageId) {

    if (!mContext) {
        return TT2_FAILURE;
    }

    LOGI("%s atvSubtitlePage[%d]:%d\n", __FUNCTION__, atvSubtitlepageId, gVBIStatus.atvSubtitlePage[atvSubtitlepageId]);

    mContext->subtitleMode = TT2_GRAPHICS_MODE;

    if (gVBIStatus.atvSubtitlePage[atvSubtitlepageId] >= 100) {
        mContext->gotoGraphicsSubtitlePage = gVBIStatus.atvSubtitlePage[atvSubtitlepageId];
        gotoPageLocked(gVBIStatus.atvSubtitlePage[atvSubtitlepageId], AM_TT2_ANY_SUBNO);
    } else {
        LOGI("%s no valid atvSubtitlePage:%d, need wait! \n", __FUNCTION__, gVBIStatus.atvSubtitlePage[atvSubtitlepageId]);
        mContext->gotoAtvSubtitleFlg = TRUE;
        mContext->dispUpdate = 1;
    }

    if (NULL != mContext->vbi) {
        vbi_set_subtitle_page(mContext->vbi, vbi_dec2bcd(mContext->gotoGraphicsSubtitlePage));
    }

    return TT2_SUCCESS;
}

int TeletextParser::gotoDefaultDtvSubtitleLocked(int dtvSubtitlepageId) {

    if (!mContext) {
        return TT2_FAILURE;
    }

    LOGI("%s dtvSubtitlePage[%d]:%d\n", __FUNCTION__, dtvSubtitlepageId, gVBIStatus.dtvSubtitlePage[dtvSubtitlepageId]);

    mContext->subtitleMode = TT2_GRAPHICS_MODE;

    if (gVBIStatus.dtvSubtitlePage[dtvSubtitlepageId] >= 100) {
        mContext->gotoGraphicsSubtitlePage = gVBIStatus.dtvSubtitlePage[dtvSubtitlepageId];
        gotoPageLocked(gVBIStatus.dtvSubtitlePage[dtvSubtitlepageId], AM_TT2_ANY_SUBNO);
    } else {
        LOGI("%s no valid dtvSubtitlePage:%d, need wait! \n", __FUNCTION__, gVBIStatus.dtvSubtitlePage[dtvSubtitlepageId]);
        mContext->gotoDtvSubtitleFlg = TRUE;
        mContext->dispUpdate = 1;
    }

    if (NULL != mContext->vbi) {
        vbi_set_subtitle_page(mContext->vbi, vbi_dec2bcd(mContext->gotoGraphicsSubtitlePage));
    }

    return TT2_SUCCESS;
}


int TeletextParser::gotoPageLocked(int pageNum, int subPageNum)
{
    LOGI("%s, pgno:%d, subPageNum:%d\n", __FUNCTION__, pageNum, subPageNum);

    if (!mContext) {
        return -1;
    }

    if (pageNum < 100 || pageNum > 899) {
        return -1;
    }

    if (subPageNum > 0xFF && subPageNum != AM_TT2_ANY_SUBNO) {
        return -1;
    }
    if (mContext->gotoPage >= 100 && mContext->gotoPage <=899) {
        tt2AddBackCachePageLocked(mContext->gotoPage, mContext->subPageNum);
    }

    if (mContext->lockSubpg == 1) {
        mContext->lockSubpg = 0;
    }

    mContext->pageNum = pageNum;
    mContext->subPageNum = subPageNum;
    mContext->acceptSubPage = subPageNum;
    mContext->gotoPage = pageNum;
    LOGI("[%s,%d] pgno: %d, mContext->subtitleMode:%d\n",__FUNCTION__, __LINE__, mContext->pageNum, mContext->subtitleMode);
    if (NULL != mContext->vbi) {
        vbi_set_subtitle_page(mContext->vbi, vbi_dec2bcd(pageNum));
    }
    if (mContext->subtitleMode == TT2_GRAPHICS_MODE) {
        mContext->dispUpdate = 1;
        int res = fetchVbiPageLocked(pageNum, subPageNum);
        if (!res) {
            LOGE("%s, return, page get error\n",__FUNCTION__);
            return -1;
        }
    }


    return 0;
}

int TeletextParser::lockSubpgLocked()
{
    if (!mContext) {
        LOGE("%s, ctx is null\n", __FUNCTION__);
        return TT2_FAILURE;
    }
    if (!mContext->vbi) {
        LOGE("%s, ctx vbi is null\n", __FUNCTION__);
        return TT2_FAILURE;
    }

    if (mContext->lockSubpg == 0) {
       mContext->lockSubpg = 1;
    } else if (mContext->lockSubpg == 1) {
       mContext->lockSubpg = 0;
    }
    mContext->dispUpdate = 1;
    return TT2_SUCCESS;
}

/**
 *  Teletext has interaction, with event handling
 *  This function main for this. the control interface.not called in parser thread, need protect.
 */
bool TeletextParser::updateParameter(int type, void *data) {
    std::unique_lock<std::mutex> autolock(mMutex);
    std::shared_ptr<TeletextParam> ttParam(new TeletextParam());
    memcpy(ttParam.get(), data, sizeof(TeletextParam));

    if (ttParam->event == TT_EVENT_INVALID) {
        return false; // ignore invalid command requested.
    }

#ifdef NEED_CACHE_ZVBI_STATUS
    // teletext not started. this is the first time we check. when not started, vbi is null.
    if (mContext->vbi == nullptr || mUpdateParamCount == 0) {
        ALOGD("This is the first? pid:%d onid:%d tsid:%d  %d", ttParam->pid, ttParam->onid, ttParam->tsid, ttParam->event);
        // tricky for check need keep using vbi or not
        gVBIStatus.updateProgramInfo( ttParam->pid, ttParam->onid, ttParam->tsid);

        // The first page, is setup by dtvkit. modify to the last
        if (gVBIStatus.needReuseVbiDecoder() && gVBIStatus.lastShowingPage >= 100) {
            // When dtvkit request home page, it goto last saved page when at the same program.
            if ((ttParam->event == TT_EVENT_GO_TO_PAGE && ttParam->pageNo <= 1 && ttParam->subPageNo == 0)
                || (ttParam->event == TT_EVENT_INDEXPAGE)) {
                ttParam->event = TT_EVENT_GO_TO_PAGE;
                int pageNum =vbi_dec2bcd(gVBIStatus.lastShowingPage);
                ttParam->pageNo =  pageNum >> 8;
                ttParam->subPageNo = pageNum & 0xFF;
                // here, search the last saved page. log start search...
                gVBIStatus.updateSearchLastPageStart();
            }
        } else {
            ALOGD("gotoDefaultDtvSubtitleLocked This is the first");
            for (int j = 0; j < TELETEXT_SUBTITLE_MAX_NUMBER; j++) {
                gVBIStatus.atvSubtitlePage[j] = 0;
                gVBIStatus.dtvSubtitlePage[j] = 0;
            }
            gVBIStatus.subtitlePageId = 0;
        }
    }

    ALOGD("mContext->pageNum=%d mContext->gotoPage=%d, lastGotoPage=%d", mContext->pageNum, mContext->gotoPage, gVBIStatus.lastShowingPage);
#endif
    mControlCmds.push_back(ttParam);
    mUpdateParamCount ++;
    return true;
}

bool TeletextParser::handleControl() {
    std::unique_lock<std::mutex> autolock(mMutex);
    if (mControlCmds.size() <= 0) {
        return true; // No command to process
    }

    std::shared_ptr<TeletextParam> ttParam = mControlCmds.front();
    mControlCmds.pop_front();
    int page;
    LOGI("%s, pageNo:%d, subPageNo:%d, regionId:%d, subPageDir:%d, event:%d\n",
        __FUNCTION__, ttParam->pageNo, ttParam->subPageNo, ttParam->regionId, ttParam->subPageDir, ttParam->event);

    switch (ttParam->event) {
        case TT_EVENT_QUICK_NAVIGATE_1:
            return fetchCountPageLocked(1, TT2_COLOR_RED);
        case TT_EVENT_QUICK_NAVIGATE_2:
            return fetchCountPageLocked(1, TT2_COLOR_GREEN);
        case TT_EVENT_QUICK_NAVIGATE_3:
            return fetchCountPageLocked(1, TT2_COLOR_YELLOW);;
        case TT_EVENT_QUICK_NAVIGATE_4:
            return fetchCountPageLocked(1, TT2_COLOR_BLUE);;
        case TT_EVENT_NEXTPAGE:
            return nextPageLocked(1);
        case TT_EVENT_PREVIOUSPAGE:
            return nextPageLocked(-1);
        case TT_EVENT_MIX_VIDEO:
            changeMixModeLocked();
            return true;
        case TT_EVENT_BACKPAGE:
            gotoBackPageLocked();
            return true;
        case TT_EVENT_FORWARDPAGE:
            gotoForwardPageLocked();
            return true;
        case TT_EVENT_REVEAL:
            setRevealModeLocked();
            return true;
        case TT_EVENT_CLEAR:
            setDisplayModeLocked();
            return true;
        case TT_EVENT_HOLD:
            lockSubpgLocked();
            return true;
        case TT_EVENT_DOUBLE_HEIGHT:
            setDoubleHeightStateLocked();
            return true;
        case TT_EVENT_NEXTSUBPAGE:
            return nextSubPageLocked(1);
        case TT_EVENT_PREVIOUSSUBPAGE:
            return nextSubPageLocked(-1);
        case TT_EVENT_DOUBLE_SCROLL_UP:
            return doubleScrollLocked(1);
        case TT_EVENT_DOUBLE_SCROLL_DOWN:
            return doubleScrollLocked(-1);
        case TT_EVENT_INDEXPAGE:
            mContext->transparentBackground = 0;
            mContext->opacity = mContext->transparentBackground ? 0 : 255;
            mContext->subtitleMode = TT2_GRAPHICS_MODE;
            return goHomeLocked();
        case TT_EVENT_GO_TO_PAGE:
            mContext->transparentBackground = 0;
            mContext->opacity = mContext->transparentBackground ? 0 : 255;
            mContext->pageState = TT2_SEARCH_STATE;
            mContext->subtitleMode = TT2_GRAPHICS_MODE;
            mContext->resetShowSubtitlePageNumberTimeFlag = true;
            page = convertPageDecimal2Hex(ttParam->pageNo, ttParam->subPageNo);
            if (NULL != mContext->vbi) {
                vbi_set_subtitle_page(mContext->vbi, vbi_dec2bcd(page));
            }
            mContext->gotoGraphicsSubtitlePage = page;
            return gotoPageLocked(page, AM_TT2_ANY_SUBNO);
        case TT_EVENT_GO_TO_SUBTITLE:
            mContext->transparentBackground = 0;
            mContext->opacity = mContext->transparentBackground ? 0 : 255;
            mContext->subtitleMode = TT2_SUBTITLE_MODE;
            mContext->resetShowSubtitlePageNumberTimeFlag = true;
            LOGI("gVBIStatus.subtitlePageId:%d mContext->atvTeletext:%d mContext->dtvTeletext:%d", gVBIStatus.subtitlePageId, mContext->atvTeletext, mContext->dtvTeletext);
            if (mContext->atvTeletext && ttParam->pageNo == -1 && ttParam->subPageNo == -1) {
                if (gVBIStatus.atvSubtitlePage[gVBIStatus.subtitlePageId] == 0 && gVBIStatus.subtitlePageId == 0) {
                    LOGI("mContext->dtvTeletext:%d gVBIStatus.dtvSubtitlePage[0]:%d no subtitle or null page", mContext->atvTeletext,gVBIStatus.atvSubtitlePage[gVBIStatus.subtitlePageId]);
                } else if (gVBIStatus.atvSubtitlePage[gVBIStatus.subtitlePageId] == 0 && gVBIStatus.subtitlePageId != 0) {
                    gVBIStatus.subtitlePageId = 0;
                    return gotoDefaultAtvSubtitleLocked(gVBIStatus.subtitlePageId);
                } else if (gVBIStatus.atvSubtitlePage[gVBIStatus.subtitlePageId] != 0) {
                    if (gVBIStatus.atvSubtitlePage[gVBIStatus.subtitlePageId+1] != 0) {
                        gVBIStatus.subtitlePageId = gVBIStatus.subtitlePageId+1;
                        return gotoDefaultAtvSubtitleLocked(gVBIStatus.subtitlePageId - 1);
                    } else {
                        int endAtvSubtitlePageFlag = gVBIStatus.subtitlePageId;
                        gVBIStatus.subtitlePageId = 0;
                        return gotoDefaultAtvSubtitleLocked(endAtvSubtitlePageFlag);
                    }
                }
            }
            if (mContext->dtvTeletext && ttParam->pageNo == -1 && ttParam->subPageNo == -1) {
                if (gVBIStatus.dtvSubtitlePage[gVBIStatus.subtitlePageId] == 0 && gVBIStatus.subtitlePageId == 0) {
                    LOGI("mContext->dtvTeletext:%d gVBIStatus.dtvSubtitlePage[0]:%d no subtitle or null page", mContext->dtvTeletext,gVBIStatus.dtvSubtitlePage[gVBIStatus.subtitlePageId]);
                } else if (gVBIStatus.dtvSubtitlePage[gVBIStatus.subtitlePageId] == 0 && gVBIStatus.subtitlePageId != 0) {
                    gVBIStatus.subtitlePageId = 0;
                    return gotoDefaultDtvSubtitleLocked(gVBIStatus.subtitlePageId);
                } else if (gVBIStatus.dtvSubtitlePage[gVBIStatus.subtitlePageId] != 0) {
                    if (gVBIStatus.dtvSubtitlePage[gVBIStatus.subtitlePageId+1] != 0) {
                        gVBIStatus.subtitlePageId = gVBIStatus.subtitlePageId+1;
                        return gotoDefaultDtvSubtitleLocked(gVBIStatus.subtitlePageId - 1);
                    } else {
                        int endDtvSubtitlePageFlag = gVBIStatus.subtitlePageId;
                        gVBIStatus.subtitlePageId = 0;
                        return gotoDefaultDtvSubtitleLocked(endDtvSubtitlePageFlag);
                    }
                }
            }
            page = convertPageDecimal2Hex(ttParam->pageNo, ttParam->subPageNo);
            return gotoPageLocked(page, AM_TT2_ANY_SUBNO);
         case TT_EVENT_0:
         case TT_EVENT_1:
         case TT_EVENT_2:
         case TT_EVENT_3:
         case TT_EVENT_4:
         case TT_EVENT_5:
         case TT_EVENT_6:
         case TT_EVENT_7:
         case TT_EVENT_8:
         case TT_EVENT_9:
             LOGE(" mGotoPageNum ttParam->event = %d, mGotoPageNum=%d",ttParam->event,mGotoPageNum);
             mGotoPageNum = mGotoPageNum*10 + (ttParam->event - 4);
             if ((mGotoPageNum == 9) || (mGotoPageNum == 0) || (mGotoPageNum > TELETEXT_MAX_PAGE_NUMBER)) {
                 LOGE(" ERROR:: Input the error page number = %d \n",mGotoPageNum);
                 mGotoPageNum = 0;
                 break;
             }
             vbi_set_subtitle_page(mContext->vbi, vbi_dec2bcd(mGotoPageNum));
             mContext->gotoGraphicsSubtitlePage = mGotoPageNum;
             if  ((mGotoPageNum > TELETEXT_MIN_PAGE_NUMBER) && (mGotoPageNum < TELETEXT_MAX_PAGE_NUMBER) && (mContext->gotoPage != mGotoPageNum) ) {
                 mContext->pageState = TT2_INPUT_STATE;
                 gotoPageLocked(mGotoPageNum, 0);
                 mGotoPageNum = 0;
             }
            break;
        case TT_EVENT_CLOCK:
        case TT_EVENT_TIMER:
            return setClockModeLocked();
        case TT_EVENT_SET_REGION_ID:
            mContext->regionId = ttParam->regionId;
            return true;
        default:
            break;
    }

    return false;
}

int TeletextParser::convertPageDecimal2Hex(int magazine, int page) {
    LOGI("%s, magazine:%d, page:%d\n", __FUNCTION__, magazine, page);
    int pageNum;
    if (magazine == 0) {
       pageNum = 800 + vbi_bcd2dec(page);
    } else {
       // pageNum = magazine * 100 + page;
       pageNum = (magazine << 8) + page;
       pageNum = vbi_bcd2dec(pageNum);
    }
    LOGI("%s, pageNum:%d\n", __FUNCTION__, pageNum);

    return pageNum;
}



int TeletextParser::initContext() {
    std::unique_lock<std::mutex> autolock(mMutex);
    mContext = new TeletextContext();
    if (!mContext) {
        LOGE("[%s::%d]malloc error! \n", __FUNCTION__, __LINE__);
    }

    unsigned int maj, min, rev;

    vbi_version(&maj, &min, &rev);
    if (!(maj > 0 || min > 2 || (min == 2 && rev >= 26))) {
        LOGE("decoder needs zvbi version >= 0.2.26.\n");
        return -1;
    }
    mContext->formatId = 0;
    mContext->vbi = NULL;
    mContext->pts = AV_NOPTS_VALUE;
    mContext->subDuration = 30000;
    mContext->opacity = -1;
    mContext->chopTop = 0;  //graphics first line
    mContext->xOffset = 0;
    mContext->yOffset = 0;
    mContext->dispUpdate = 0;
    mContext->lockSubpg = 0;
    mContext->dispMode = 0;
    mContext->regionId = 0;
    mContext->gotoGraphicsSubtitlePage = 0;
    mContext->subtitlePageNumber = 0;
    mContext->subtitlePageNumberShowTimeOutFlag = false;
    mContext->resetShowSubtitlePageNumberTimeFlag = false;
    mContext->removewHeights = 0;
    mContext->doubleHeight = DOUBLE_HEIGHT_NORMAL;
    mContext->mixVideoState = TT2_MIX_BLACK;
    mContext->subtitleMode = TT2_SUBTITLE_MODE;
    mContext->reveal = 0;
    mContext->gotoAtvSubtitleFlg = FALSE;
    mContext->gotoDtvSubtitleFlg = FALSE;

    mContext->atvTeletext = FALSE;
    mContext->dtvTeletext = FALSE;

    mContext->lasttime = std::chrono::system_clock::now();

    mContext->heightIndex = 0;
    mContext->chopSpaces = 1;
    mContext->subPageNum = AM_TT2_ANY_SUBNO;
    mContext->pageState = TT2_DISPLAY_STATE;
    mContext->searchDir = 1;
    //1:transparent 0:black default transparent
    mContext->transparentBackground = 1;
    //display backGround, page not full Green, need add prop define non-page display backGround
    //1:transparent 0:black
    mContext->disPlayBackground = 0;
    if (mContext->opacity == -1) {
        mContext->opacity = mContext->transparentBackground ? 0 : 255;
    }

#ifdef DEBUG
    {
        char *t;
        mContext->ex = vbi_export_new("text", &t);
    }
#endif
    LOGI(" %s page filter pgno: %d gVBIStatus.subtitlePageId:%d\n", __FUNCTION__, mContext->pageNum, gVBIStatus.subtitlePageId);

    return 0;
}

void TeletextParser::checkDebug() {
    char value[PROPERTY_VALUE_MAX] = {0};
    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("vendor.subtitle.dump", value, "false");
    if (!strcmp(value, "true")) {
        mDumpSub = true;
    }
}

int TeletextParser::teletextDecodeFrame(std::shared_ptr<AML_SPUVAR> spu, char *srcData, int srcLen) {
    int ret = 0;

    std::unique_lock<std::mutex> autolock(mMutex);
    if (mContext->formatId == 0) {
        spu->spu_width  = TELETEXT_COL * BITMAP_CHAR_WIDTH;
        spu->spu_height = TELETEXT_ROW * BITMAP_CHAR_HEIGHT;
    }

    //LOGD(" %s, ctx->vbi:%p\n", __FUNCTION__, mContext->vbi);
    if (!mContext->vbi) {
        //TODO: check need close or not
#ifdef NEED_CACHE_ZVBI_STATUS
        if (!gVBIStatus.needReuseVbiDecoder()) {
            if (gVBIStatus.getVbiInstance() != nullptr) {
                vbi_decoder_delete(gVBIStatus.getVbiInstance());
                gVBIStatus.registerVbiInstance(nullptr);
            }
        }

        if (gVBIStatus.getVbiInstance() == nullptr) {
            gVBIStatus.registerVbiInstance(vbi_decoder_new());
            if (gVBIStatus.getVbiInstance() == nullptr) return -1;
        } else {
            // clear handler.
            vbi_event_handler_remove(gVBIStatus.getVbiInstance(), tt2TimeUpdate);
            vbi_event_handler_remove(gVBIStatus.getVbiInstance(), handler);
        }
        mContext->vbi = gVBIStatus.getVbiInstance();
#else
        if (!(mContext->vbi = vbi_decoder_new()))
            return -1;
#endif

        if (!vbi_event_handler_add(mContext->vbi, VBI_EVENT_TTX_PAGE, handler, mContext)) {
            //LOGI("[teletext_decode_frame]---%d--\n", __LINE__);
#ifdef NEED_CACHE_ZVBI_STATUS
            vbi_decoder_delete(gVBIStatus.getVbiInstance());
            gVBIStatus.registerVbiInstance(nullptr);
#else
            vbi_decoder_delete(mContext->vbi);
#endif
            mContext->vbi = NULL;
            return -1;
        }

        vbi_event_handler_add(mContext->vbi, VBI_EVENT_TIME, tt2TimeUpdate, mContext);
        vbi_set_subtitle_page(mContext->vbi, vbi_dec2bcd(mContext->pageNum));
    }


    //atv teletext
    if (mContext->atvTeletext) {
        vbi_sliced *s = mContext->sliced;
        s->line = mContext->lineNum;
        s->id = VBI_SLICED_TELETEXT_B;
        memcpy(s->data, srcData, 42);
        vbi_decode(mContext->vbi, s, 1, 0);
    }

    //dtv teletext
    /*if (mContext->dtvTeletext) {
        vbi_sliced *s = mContext->sliced;
        s->line = mContext->lineNum;
        s->id = VBI_SLICED_TELETEXT_B;
        memcpy(s->data, srcData, 42);
        vbi_decode(mContext->vbi, s, 1, 0);
    }*/

    if (srcLen && !mContext->atvTeletext) {
        int lines;
        const int full_pes_size = srcLen + 45; /* PES header is 45 bytes */

        // We allow unreasonably big packets, even if the standard only allows a max size of 1472
        if (full_pes_size < 184 || full_pes_size > 65504 /*|| full_pes_size % 184 != 0*/) {
            return -1;
        }

        mContext->handlerRet = srcLen;

        if (checkIdentifierIsTeletext(*srcData)) {
            if ((lines = slice2VbiLines(mContext, srcData+1, srcLen-1)) < 0)
                return lines;

            LOGV("ctx=%p buf_size=%d lines=%u\n", mContext, srcLen, lines);
            if (lines > 0) {
               int i;
               //LOGI("line numbers:");
               //for (i = 0; i < lines; i++)
               //     LOGI(" %d", mContext->sliced[i].line);

                //LOGI("--%s, ctx->vbi->time=%f\n", __FUNCTION__, ctx->vbi->time);
                vbi_decode(mContext->vbi, mContext->sliced, lines, 0.0);
                mContext->linesProcessed += lines;
                LOGV("%s, after vbi decode,ctx->linesProcessed=%d\n",__FUNCTION__,mContext->linesProcessed);
            }
        }
        mContext->pts = AV_NOPTS_VALUE;
        ret = mContext->handlerRet;
    }

    if (ret < 0) {
        LOGV("--%s,ret=%d, ctx->handlerRet=%d\n", __FUNCTION__, ret, mContext->handlerRet);
        return ret;
    }

    LOGV("--%s,ret=%d, ctx->handlerRet=%d, ctx->totalPages=%d, ctx->formatId=%d\n",
            __FUNCTION__, ret, mContext->handlerRet, mContext->totalPages, mContext->formatId);

    // is there a subtitle to pass?
    if (mContext->totalPages) {
        int i;
        if (mContext->pages && mContext->pages->subRect && mContext->pages->subRect->type != SUBTITLE_NONE) {
            //sub->rects = malloc(sizeof(*sub->rects));
            if (1/*sub->rects*/) {
                if (mContext->formatId == 0) {
                    ret = saveTeletextGraphicsRect2Spu(spu, mContext->pages->subRect);
                    //if (ret < 0) return -1;
                }
            } else {
                ret = -1;//AVERROR(ENOMEM);
            }
        } else {
            LOGI("sending empty sub\n");
            //sub->rects = NULL;
        }

        // advance, free the first item to avoid leak.
        if (mContext->pages[0].subRect) {
            free(mContext->pages[0].subRect->pict.data[0]);
            free(mContext->pages[0].subRect->pict.data[1]);
            free(mContext->pages[0].subRect);
        }
        for (i = 0; i < mContext->totalPages - 1; i++) {
            mContext->pages[i] = mContext->pages[i + 1];
        }
        mContext->totalPages--;
    }

    return ret;
}


int TeletextParser::getDvbTeletextSpu() {
    char tmpbuf[8];
    int64_t packetHeader = 0;

    LOGV("enter get_dvb_teletext_spu\n");
    int ret = -1;

    while (mDataSource->read(tmpbuf, 1) == 1) {
        if (mState == SUB_STOP) {
            return 0;
        }

        std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());
        spu->sync_bytes = AML_PARSER_SYNC_WORD;

        packetHeader = ((packetHeader<<8) & 0x000000ffffffffff) | tmpbuf[0];
        LOGV("## get_dvb_spu %x, %llx,-------------\n",tmpbuf[0], packetHeader);

        if ((packetHeader & 0xffffffff) == 0x000001bd) {
            ret = hwDemuxParse(spu);
        } else if (((packetHeader & 0xffffffffff)>>8) == AML_PARSER_SYNC_WORD
                && (((packetHeader & 0xff)== 0x77) || ((packetHeader & 0xff)==0xaa))) {
            ret = softDemuxParse(spu);
        } else if (((packetHeader & 0xffffffffff)>>8) == AML_PARSER_SYNC_WORD
                && ((packetHeader & 0xff)== 0x41)) {//AMLUA  ATV teletext
            ret = atvHwDemuxParse(spu);
        }

        if (mContext->vbi) {
            handleControl();
        }
    }

    return ret;
}

int TeletextParser::gotoBackPageLocked()
{
    LOGI("%s back page stack size = %d\n", __FUNCTION__, mBackPageStk.size());

    //now first open teletext gfx,will add back stack,ignore this only first page.
    if (mBackPageStk.size() <= 1) {
        LOGE("%s, error, no other back page\n",__FUNCTION__);
        return TT2_FAILURE;
    }

    if (mContext->lockSubpg == 1) {
        mContext->lockSubpg = 0;
    }
    //current page push forward stack first
    TeletextCachedPageT forwardTtCachePage;
    forwardTtCachePage.pageNo = mContext->gotoPage;
    forwardTtCachePage.subPageNo = mContext->subPageNum;
    mForwardPageStk.push(forwardTtCachePage);
    LOGI("%s enter forward stack, page: %d\n", __FUNCTION__, mContext->gotoPage);

    //pop head page
    TeletextCachedPageT headTtCachePage;
    headTtCachePage = mBackPageStk.top();

    mContext->pageNum = headTtCachePage.pageNo;
    mContext->subPageNum = headTtCachePage.subPageNo;
    mContext->dispUpdate = 1;
    mContext->gotoPage = mContext->pageNum;
    LOGI("[%s,%d]pgno: %d\n",__FUNCTION__, __LINE__, mContext->gotoPage);
    vbi_set_subtitle_page(mContext->vbi, vbi_dec2bcd(mContext->gotoPage));
    int res = fetchVbiPageLocked(mContext->gotoPage, 0/*mContext->subPageNum*/);
    mBackPageStk.pop();

    if (!res) {
        LOGE("%s, return, page get error\n",__FUNCTION__);
        return TT2_FAILURE;
    }
    mContext->gotoGraphicsSubtitlePage = mContext->gotoPage;
    return TT2_SUCCESS;
}

int TeletextParser::gotoForwardPageLocked()
{
    LOGI("%s forward page stack size = %d\n", __FUNCTION__, mBackPageStk.size());

    if (mForwardPageStk.size() <= 0) {
        LOGE("%s, error, no forward page\n",__FUNCTION__);
        return TT2_FAILURE;
    }

    if (mContext->lockSubpg == 1) {
        mContext->lockSubpg = 0;
    }

    //current page push back stack first
    TeletextCachedPageT backTtCachePage;
    backTtCachePage.pageNo = mContext->gotoPage;
    backTtCachePage.subPageNo = mContext->subPageNum;
    mBackPageStk.push(backTtCachePage);
    LOGI("%s enter back stack, page: %d\n", __FUNCTION__, mContext->gotoPage);

    //pop head page
    TeletextCachedPageT headTtCachePage;
    headTtCachePage = mForwardPageStk.top();
    mContext->pageNum = headTtCachePage.pageNo;
    mContext->subPageNum = headTtCachePage.subPageNo;
    //mContext->acceptSubPage = subPageNum;
    mContext->dispUpdate = 1;
    mContext->gotoPage = mContext->pageNum;
    LOGI("[%s,%d]pgno: %d\n",__FUNCTION__, __LINE__, mContext->gotoPage);
    vbi_set_subtitle_page(mContext->vbi, vbi_dec2bcd(mContext->gotoPage));
    int res = fetchVbiPageLocked(mContext->gotoPage, mContext->subPageNum);
    mForwardPageStk.pop();

    if (!res) {
        LOGE("%s, return, page get error\n",__FUNCTION__);
        return TT2_FAILURE;
    }
    mContext->gotoGraphicsSubtitlePage = mContext->gotoPage;
    return TT2_SUCCESS;

}



void TeletextParser::tt2AddForwardCachePageLocked(int pgno, int subPgno)
{
    LOGI("%s pgno:%d, subPgno:%d\n", __FUNCTION__, pgno, subPgno);

    TeletextCachedPageT ttCachePage;
    ttCachePage.pageNo = pgno;
    ttCachePage.subPageNo = subPgno;
    mForwardPageStk.push(ttCachePage);
    LOGI("%s add forward cache success!\n", __FUNCTION__);
}

void TeletextParser::tt2AddBackCachePageLocked(int pgno, int subPgno)
{
    LOGI("%s pgno:%d, subPgno:%d\n", __FUNCTION__, pgno, subPgno);

    TeletextCachedPageT ttCachePage;
    ttCachePage.pageNo = pgno;
    ttCachePage.subPageNo = subPgno;
    mBackPageStk.push(ttCachePage);
}

int TeletextParser::hwDemuxParse(std::shared_ptr<AML_SPUVAR> spu) {
    char tmpbuf[256] = {0};
    int64_t dvbPts = 0, dvbDts = 0;
    int64_t tempPts = 0, tempDts = 0;
    int packageLen = 0, pesHeaderLen = 0;
    bool needSkipData = false;
    int ret = 0;

    if (mDataSource->read(tmpbuf, 2) == 2) {
        packageLen = (tmpbuf[0] << 8) | tmpbuf[1];
        if (packageLen >= 3) {
            if (mDataSource->read(tmpbuf, 3) == 3) {
                packageLen -= 3;
                pesHeaderLen = tmpbuf[2];
                LOGI("get_dvb_teletext_spu-packageLen:%d, pesHeaderLen:%d\n", packageLen,pesHeaderLen);

                if (packageLen >= pesHeaderLen) {
                    if ((tmpbuf[1] & 0xc0) == 0x80) {
                        if (mDataSource->read(tmpbuf, pesHeaderLen) == pesHeaderLen) {
                            tempPts = (int64_t)(tmpbuf[0] & 0xe) << 29;
                            tempPts = tempPts | ((tmpbuf[1] & 0xff) << 22);
                            tempPts = tempPts | ((tmpbuf[2] & 0xfe) << 14);
                            tempPts = tempPts | ((tmpbuf[3] & 0xff) << 7);
                            tempPts = tempPts | ((tmpbuf[4] & 0xfe) >> 1);
                            dvbPts = tempPts; // - pts_aligned;
                            packageLen -= pesHeaderLen;
                        }
                    } else if ((tmpbuf[1] &0xc0) == 0xc0) {
                        if (mDataSource->read(tmpbuf, pesHeaderLen) == pesHeaderLen) {
                            tempPts = (int64_t)(tmpbuf[0] & 0xe) << 29;
                            tempPts = tempPts | ((tmpbuf[1] & 0xff) << 22);
                            tempPts = tempPts | ((tmpbuf[2] & 0xfe) << 14);
                            tempPts = tempPts | ((tmpbuf[3] & 0xff) << 7);
                            tempPts = tempPts | ((tmpbuf[4] & 0xfe) >> 1);
                            dvbPts = tempPts; // - pts_aligned;
                            tempDts = (int64_t)(tmpbuf[5] & 0xe) << 29;
                            tempDts = tempDts | ((tmpbuf[6] & 0xff) << 22);
                            tempDts = tempDts | ((tmpbuf[7] & 0xfe) << 14);
                            tempDts = tempDts | ((tmpbuf[8] & 0xff) << 7);
                            tempDts = tempDts | ((tmpbuf[9] & 0xfe) >> 1);
                            dvbDts = tempDts; // - pts_aligned;
                            packageLen -= pesHeaderLen;
                        }
                    } else {
                        // No PTS, has the effect of displaying immediately.
                        //needSkipData = true;
                        mDataSource->read(tmpbuf, pesHeaderLen);
                        packageLen -= pesHeaderLen;
                    }
                } else {
                    needSkipData = true;
                }
            }
        } else {
           needSkipData = true;
        }

        if (needSkipData) {
            for (int iii = 0; iii < packageLen; iii++) {
                char tmp;
                if (mDataSource->read(&tmp, 1)  == 0) {
                    return -1;
                }
            }
        } else if (packageLen > 0) {
            char *buf = NULL;
            if ((packageLen) > (OSD_HALF_SIZE * 4)) {
                LOGE("dvb packet is too big\n\n");
                return -1;
            }
            spu->subtitle_type = TYPE_SUBTITLE_DVB_TELETEXT;
            spu->pts = dvbPts;
            mContext->dtvTeletext = true;
            buf = (char *)malloc(packageLen);
            if (buf) {
                LOGI("packageLen is %d, pts is %llx, delay is %llx\n", packageLen, spu->pts, tempPts);
            } else {
                LOGI("packageLen buf malloc fail!!!\n");
            }

            if (buf) {
                memset(buf, 0x0, packageLen);
                if (mDataSource->read(buf, packageLen) == packageLen) {
                    ret = teletextDecodeFrame(spu, buf, packageLen);
                    LOGI(" @@@@@@@hwDemuxParse parse ret:%d, buffer_size:%d", ret, spu->buffer_size);
                    if (ret != -1 && ret != DATA_VALID_AND_BLANK && spu->buffer_size > 0) {
                        LOGI("dump-pts-hwdmx!success pts(%lld) %d frame was add\n", spu->pts, ++mIndex);
                        if ((spu->spu_origin_display_w <= 0 || spu->spu_origin_display_h <= 0)
                          || (spu->spu_origin_display_w > 4090 || spu->spu_origin_display_h > 2190)) {
                            spu->spu_origin_display_w = VideoInfo::Instance()->getVideoWidth();
                            spu->spu_origin_display_h = VideoInfo::Instance()->getVideoHeight();
                        }
                        #ifdef SUPPORT_LOAD_ANIMATION
                        //wait video first frame ready.
                        if (!mDataSource->isFileAvailble()) {
                            LOGI("video first frame not ready, need wait ready!!\n");
                            if (buf) free(buf);
                            return -1;
                        }
                        #endif
                         //ttx,need immediatePresent show
                        //spu->isImmediatePresent = true;
                        //because when the pageType is bigger than the 0x8000,can't be judged by the 0x8000.
                        if (/*(mContext->subtitleMode == TT2_SUBTITLE_MODE) || */(mContext->isSubtitle)) {
                            spu->isKeepShowing = false;
                            spu->isImmediatePresent = false;
                            spu->isTtxSubtitle = true;
                        } else {
                            spu->isKeepShowing = true;
                            spu->isImmediatePresent = true;
                            spu->isTtxSubtitle = false;
                        }
                        LOGI(" addDecodedItem buffer_size=%d ctx->isSubtitle=%d pageType=0x%x mode=%d",
                                spu->buffer_size, mContext->isSubtitle, mContext->pageType, mContext->subtitleMode);
                        addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
                        if (buf) free(buf);
                        return 0;
                    } else {
                        LOGI("dump-pts-hwdmx!error pts(%lld) frame was abondon ret=%d bufsize=%d\n", spu->pts, ret, spu->buffer_size);
                        if (buf) free(buf);
                        return -1;
                    }
                }
                LOGI("packageLen buf free=%p\n", buf);
                LOGI("@@[%s::%d]free ptr=%p\n", __FUNCTION__, __LINE__, buf);
                free(buf);
            }
        }
    }

    return ret;
}

int TeletextParser::softDemuxParse(std::shared_ptr<AML_SPUVAR> spu) {
    char tmpbuf[256] = {0};
    int64_t dvbPts = 0, ptsDiff = 0;
    int ret = 0;
    int dataLen = 0;
    char *data = NULL;

    // read package header info
    if (mDataSource->read(tmpbuf, 19) == 19) {
        LOGV("## 333 get_dvb_spu %x,%x,%x,  %x,%x,%x,  %x,%x,-------------\n",
                tmpbuf[0], tmpbuf[1], tmpbuf[2], tmpbuf[3],
                tmpbuf[4], tmpbuf[5], tmpbuf[6], tmpbuf[7]);

        dataLen = subPeekAsInt32(tmpbuf + 3);
        dvbPts  = subPeekAsInt64(tmpbuf + 7);
        ptsDiff = subPeekAsInt32(tmpbuf + 15);

        spu->subtitle_type = TYPE_SUBTITLE_DVB_TELETEXT;
        spu->pts = dvbPts;
        LOGV("## spu-> pts:%lld,dvPts:%lld\n", spu->pts, dvbPts);
        LOGV("## 4444 datalen=%d,pts=%llx,delay=%llx,diff=%llx, data: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
                dataLen, dvbPts, spu->m_delay, ptsDiff,
                tmpbuf[0], tmpbuf[1], tmpbuf[2], tmpbuf[3], tmpbuf[4],
                tmpbuf[5], tmpbuf[6], tmpbuf[7], tmpbuf[8], tmpbuf[9],
                tmpbuf[10], tmpbuf[11], tmpbuf[12], tmpbuf[13], tmpbuf[14]);

        data = (char *)malloc(dataLen);
        if (!data) {
            LOGE("[%s::%d]malloc error! \n", __FUNCTION__,__LINE__);
            return -1;
        }
        LOGV("@@[%s::%d]malloc ptr=%p, size = %d\n",__FUNCTION__, __LINE__, data, dataLen);
        memset(data, 0x0, dataLen);
        ret = mDataSource->read(data, dataLen);
        LOGV("## ret=%d,dataLen=%d, %x,%x,%x,%x,%x,%x,%x,%x,---------\n",
                ret, dataLen, data[0], data[1], data[2], data[3],
                data[4], data[5], data[6], data[7]);

        ret = teletextDecodeFrame(spu, data, dataLen);
        LOGV("## dvb: (width=%d,height=%d), (x=%d,y=%d),ret =%d,spu->buffer_size=%d--------\n",
                spu->spu_width, spu->spu_height, spu->spu_start_x, spu->spu_start_y, ret, spu->buffer_size);

        if (ret != -1 && spu->buffer_size > 0) {
            LOGV("dump-pts-swdmx!success pts(%lld) mIndex:%d frame was add\n", spu->pts, ++mIndex);
            if (spu->spu_origin_display_w <= 0 || spu->spu_origin_display_h <= 0) {
                spu->spu_origin_display_w = VideoInfo::Instance()->getVideoWidth();
                spu->spu_origin_display_h = VideoInfo::Instance()->getVideoHeight();
            }
            //ttx,need immediatePresent show
            spu->isImmediatePresent = true;
            spu->isKeepShowing = true;
            addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
        } else {
            LOGV("dump-pts-swdmx!error this pts(%lld) frame was abondon\n", spu->pts);
        }

        if (data) {
            free(data);
            data = NULL;
        }
    }
return ret;
}

int TeletextParser::atvHwDemuxParse(std::shared_ptr<AML_SPUVAR> spu) {
        char tmpbuf[256] = {0};
        int64_t dvbPts = 0, ptsDiff = 0;
        int ret = 0;
        int lineNum = 0;
        char *data = NULL;

        // read package header info
        if (mDataSource->read(tmpbuf, 4) == 4) {
            mContext->lineNum = subPeekAsInt32(tmpbuf);
            LOGV("[%s::%d]line num:%d\n", __FUNCTION__,__LINE__, mContext->lineNum);

            spu->subtitle_type = TYPE_SUBTITLE_DVB_TELETEXT;
            mContext->atvTeletext = true;
            data = (char *)malloc(ATV_TELETEXT_DATA_LEN);
            if (!data) {
                LOGE("[%s::%d]malloc error! \n", __FUNCTION__,__LINE__);
                return -1;
            }
            memset(data, 0x0, ATV_TELETEXT_DATA_LEN);
            ret = mDataSource->read(data, ATV_TELETEXT_DATA_LEN);
            LOGV("[%s::%d] ret=%d,dataLen=%d, %x,%x,%x,%x,%x,%x,%x,%x,---------\n",
                    __FUNCTION__, __LINE__, ret, ATV_TELETEXT_DATA_LEN, data[0], data[1], data[2], data[3],
                    data[4], data[5], data[6], data[7]);

            ret = teletextDecodeFrame(spu, data, ATV_TELETEXT_DATA_LEN);
            LOGV("[%s::%d] (width=%d,height=%d), (x=%d,y=%d),ret =%d,spu->buffer_size=%d--------\n", __FUNCTION__, __LINE__,
                    spu->spu_width, spu->spu_height, spu->spu_start_x, spu->spu_start_y, ret, spu->buffer_size);

            if (ret != -1 && spu->buffer_size > 0) {
                LOGV("[%s::%d]dump-pts-atvHwDmx!success pts(%lld) mIndex:%d frame was add\n", __FUNCTION__,__LINE__, spu->pts, ++mIndex);
                if (spu->spu_origin_display_w <= 0 || spu->spu_origin_display_h <= 0) {
                    spu->spu_origin_display_w = VideoInfo::Instance()->getVideoWidth();
                    spu->spu_origin_display_h = VideoInfo::Instance()->getVideoHeight();
                }
                //ttx,need immediatePresent show
                spu->isImmediatePresent = true;
                spu->isKeepShowing = true;
                addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
            } else {
                LOGV("[%s::%d]dump-pts-atvHwDmx!error this pts(%lld) frame was abondon\n", __FUNCTION__,__LINE__, spu->pts);
            }

            if (data) {
                free(data);
                data = NULL;
            }
        }
    return ret;
}

bool TeletextParser::getVbiNextValidPage(vbi_decoder *vbi, int dir, vbi_pgno *pgno, vbi_pgno *subno) {
    int retryCount = 103;//ff-9a + 1, for sepcail sepcaily case, mostly will break in several times.
    int tmpPage = *pgno, tmpSubPage = *subno;
    bool found = false;

    LOGI("%s.\n", __FUNCTION__);
    for (int i = 0; i < retryCount; i++) {
        if (vbi_get_next_pgno(vbi, dir, &tmpPage, &tmpSubPage)) {
            LOGI("%s: get vbi next page %d\n", __FUNCTION__, vbi_bcd2dec(tmpPage));
            if ((tmpPage & 0x0f) > 9
                || ((tmpPage >> 4) & 0x0f) > 9
                || ((tmpPage >> 8) & 0x0f) > 9) {
                LOGE("%s: Next page %d is invalid, continue.\n", __FUNCTION__, tmpPage);
                continue;
            }
            if (vbi_bcd2dec(tmpPage) < 100 || vbi_bcd2dec(tmpPage) > 899) {
                LOGE("%s: Next page %d is out of range, continue.\n", __FUNCTION__, tmpPage);
                continue;
            }

            *pgno = tmpPage;
            *subno = tmpSubPage;
            found = true;
            break;
        }
    }

    return found;
}

int TeletextParser::getSpu() {
    if (mState == SUB_INIT) {
        mState = SUB_PLAYING;
    } else if (mState == SUB_STOP) {
        ALOGD(" subtitle_status == SUB_STOP \n\n");
        return 0;
    }

    return getDvbTeletextSpu();
}


int TeletextParser::getInterSpu() {
    return getSpu();
}


int TeletextParser::parse() {
    while (!mThreadExitRequested) {
        if (getInterSpu() < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); //decoder freq.
        }
    }
    return 0;
}

void TeletextParser::dump(int fd, const char *prefix) {
    //TODO: dump run in binder thread, may need add lock!

    dprintf(fd, "%s Tele Text Parser\n", prefix);
    dumpCommon(fd, prefix);

    if (mContext != nullptr) {
        dprintf(fd, "%s  pgno:%p\n", prefix, mContext->pgno);
        dprintf(fd, "%s  xOffset:%d yOffset:%d\n", prefix, mContext->xOffset, mContext->yOffset);
        dprintf(fd, "%s  formatId: %d (0 = bitmap, 1 = text/ass)\n", prefix, mContext->formatId);
        dprintf(fd, "%s  pts:%lld\n", prefix, mContext->pts);
        dprintf(fd, "%s  subDuration:%d\n", prefix, mContext->subDuration);
        dprintf(fd, "%s  transparentBackground:%d\n", prefix, mContext->transparentBackground);
        dprintf(fd, "%s  opacity:%d\n", prefix, mContext->opacity);
        dprintf(fd, "%s  totalPages:%d\n", prefix, mContext->totalPages);
        dprintf(fd, "%s  linesProcessed:%d\n", prefix, mContext->linesProcessed);
        dprintf(fd, "%s  pageNum:%d\n", prefix, mContext->pageNum);
        dprintf(fd, "%s  subPageNum:%d\n", prefix, mContext->subPageNum);
        dprintf(fd, "%s  acceptSubPage:%d\n", prefix, mContext->acceptSubPage);
        dprintf(fd, "%s  dispUpdate:%d\n", prefix, mContext->dispUpdate);
        dprintf(fd, "%s  gotoPage:%d\n", prefix, mContext->gotoPage);
        dprintf(fd, "%s  readorder:%d\n", prefix, mContext->readorder);
        dprintf(fd, "\n");
        dprintf(fd, "%s  totalPages:%d\n", prefix, mContext->totalPages);
        dprintf(fd, "Parameter update count:%d\n", mUpdateParamCount);
        for (int i = 0; i < mContext->totalPages; i++) {
            dprintf(fd, "%s   page:%03d ", prefix, i);
            TeletextPage *pg = mContext->pages + i;
            if (pg != nullptr) {
                dprintf(fd, "pts=%llu subno=%d pgno=%d ", pg->pts, pg->subno, pg->pgno);
                AVSubtitleRect *r = pg->subRect;
                if (r != nullptr) {
                    dprintf(fd, "[x y w h nc][%u %u %u %u %u]", r->x, r->y, r->w, r->h, r->nbColors);
                    dprintf(fd, " type=%d data[%p %p %p %p]\n", r->type, r->pict.data[0],
                        r->pict.data[1], r->pict.data[2], r->pict.data[3]);
                }
            }
        }
    }


}

