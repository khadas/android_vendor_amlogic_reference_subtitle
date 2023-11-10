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

#define LOG_TAG "PgsParser"
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <list>
#include <thread>
#include <algorithm>
#include <functional>

#include "SubtitleLog.h"
#include "StreamUtils.h"

#include "SubtitleTypes.h"
#include "PgsParser.h"
#include "ParserFactory.h"
#include "VideoInfo.h"

#define MAX_EPOCH_PALETTES 8 // Max 8 allowed per PGS epoch
#define MAX_EPOCH_OBJECTS 64 // Max 64 allowed per PGS epoch
#define MAX_OBJECT_REFS 2    // Max objects per display set

#define DEFAULT_DELAY_TIME 2 // second
#define DEFAULT_DVB_TIME_MULTI 90


enum SegmentType {
    PALETTE_SEGMENT      = 0x14,
    OBJECT_SEGMENT       = 0x15,
    PRESENTATION_SEGMENT = 0x16,
    WINDOW_SEGMENT       = 0x17,
    DISPLAY_SEGMENT      = 0x80,
};

struct PGSSubObjectRef {
    int     id;
    int     window_id;
    uint8_t composition_flag;
    int     x = 0;
    int     y = 0;
    int     crop_x;
    int     crop_y;
    int     crop_w;
    int     crop_h;
};

struct PGSSubPalette {
    int         id;
    uint32_t    clut[256];
};

struct PGSSubPalettes {
    int           count;
    PGSSubPalette palette[MAX_EPOCH_PALETTES];
};

struct PgsInfo {
    int startTime;
    int endTime;
    int width;
    int height;
    char fpsCode;
    int objectCount;
    int objectId;
    int versionNum;
    PGSSubObjectRef objects[MAX_OBJECT_REFS];

    char state;
    char paletteUpdateFlag;
    char paletteIdRef;
    char number;

    int windowWidthOffset;
    int windowHeightOffset;
    int windowWidth;
    int windowHeight;
    int imageWidth;
    int imageHeight;
    int palette[0x100];
    unsigned char *rleBuf;
    int rleBufSize;
    int rleReadOff;
    /**/
};

struct PgsBuffer {
    off_t pos;
    unsigned char *pgs_data_buf;
    int dataBufferSize;
    unsigned char pgsTimeHeadBuf[13];
};

struct PgsItem {
    int startTime;
    int endTime;
    off_t pos;
};

/* PGS subtitle API */
struct PgsShowData {
    int x;
    int y;
    int width;
    int height;

    int windowWidthOffset;
    int windowHeightOffset;
    int windowWidth;
    int windowHeight;

    int imageWidth;
    int imageHeight;

    int *palette;
    unsigned char *rleBuf;
    unsigned char *resultBuf;
    int rleBufSize;
    int renderHeight;
    int64_t pts;
    int objectSegmentId;
};

struct PgsSubtitleEpgs {
    /* Add more members here */
    PgsInfo *pgsInfo;
    char *curIdxUrl;
    PgsBuffer pgsBuffer;

    int fd;
    off_t filePos;
    PgsItem *pgsItemTable;
    int pgsItemCount;
    int pgsDisplayIndex;

    PgsShowData showdata;
};


static inline uint8_t readTimeHeader(uint8_t **bufStartPtr, int *pSize, int *startTime, int *endTime) {
    int type;
    unsigned char *buf = *bufStartPtr;
    int size;
    if (buf[0] == 'P' && buf[1] == 'G') {
        *startTime = (buf[2]<<24) | (buf[3]<<16) | (buf[4]<<8) | (buf[5]);
        *endTime = (buf[6]<<24) | (buf[7]<<16) | (buf[8]<<8) | (buf[9]);
        type = buf[10];
        size = (buf[11] << 8) | buf[12];
        *bufStartPtr = buf + 13 + size;
        *pSize = size;
        return type;
    }
    return 0;
}

static inline void readSubpictureHeader(unsigned char *buf, PgsInfo *pgsInfo) {
    pgsInfo->width = (buf[0] << 8) | buf[1];
    pgsInfo->height = (buf[2] << 8) | buf[3];
    pgsInfo->fpsCode = buf[4];
    pgsInfo->state = buf[7];   //0x80
    pgsInfo->paletteUpdateFlag = buf[8];
    pgsInfo->paletteIdRef = buf[9];
    pgsInfo->objectCount = buf[0xa];
    int cropping = 0;
    for (int i = 0; i < pgsInfo->objectCount; i++) {
        pgsInfo->objects[i].id = (buf[0xb + i*8 + cropping] << 8) | buf[0xc + i*8 + cropping];
        pgsInfo->objects[i].window_id = buf[0xd + i*8 + cropping];
        pgsInfo->objects[i].composition_flag = buf[0xe + i*8 + cropping]; //cropped |=0x80, forced |= 0x40
        pgsInfo->objects[i].x = (buf[0xf + i*8 + cropping] << 8) | buf[0x10 + i*8 + cropping];
        pgsInfo->objects[i].y = (buf[0x11 + i*8 + cropping] << 8) | buf[0x12 + i*8 + cropping];
        // If cropping
        if (pgsInfo->objects[i].composition_flag & 0x80) {
            pgsInfo->objects[i].crop_x = (buf[0x13 + i*8] << 8) | buf[0x14 + i*8];
            pgsInfo->objects[i].crop_y = (buf[0x15 + i*8] << 8) | buf[0x16 + i*8];
            pgsInfo->objects[i].crop_w = (buf[0x17 + i*8] << 8) | buf[0x18 + i*8];
            pgsInfo->objects[i].crop_h = (buf[0x19 + i*8] << 8) | buf[0x1A + i*8];
            cropping = 8;
        }
        SUBTITLE_LOGI("--readSubpictureHeader--  i:%d id:%d window_id:%d composition_flag:%d x:%d y:%d crop_x:%d crop_y:%d crop_w:%d, crop_h:%d, width:%d height:%d objectCount:%d\n",
            i,
            pgsInfo->objects[i].id,
            pgsInfo->objects[i].window_id,
            pgsInfo->objects[i].composition_flag,
            pgsInfo->objects[i].x,
            pgsInfo->objects[i].y,
            pgsInfo->objects[i].crop_x,
            pgsInfo->objects[i].crop_y,
            pgsInfo->objects[i].crop_w,
            pgsInfo->objects[i].crop_h ,
            pgsInfo->width,
            pgsInfo->height,
            pgsInfo->objectCount);
    }
}

static inline void readWindowHeader(unsigned char *buf, PgsInfo *pgsInfo) {
    SUBTITLE_LOGI("--readWindowHeader-- %d, %d\n", pgsInfo->imageWidth, pgsInfo->imageHeight);
    pgsInfo->windowWidthOffset = (buf[2] << 8) | buf[3];
    pgsInfo->windowHeightOffset = (buf[4] << 8) | buf[5];
    pgsInfo->windowWidth = (buf[6] << 8) | buf[7];
    pgsInfo->windowHeight = (buf[8] << 8) | buf[9];
}
static inline void readWindowInfo(unsigned char *buf, PgsInfo *pgsInfo) {
    pgsInfo->windowWidth = (buf[0] << 8) | buf[1];
    pgsInfo->windowHeight = (buf[2] << 8) | buf[3];
    SUBTITLE_LOGI("readWindowInfo windowwidth:%d,windowHeight:%d",pgsInfo->windowWidth,pgsInfo->windowHeight);
}


static inline void readColorTable(unsigned char *buf, int size, PgsInfo *pgsInfo) {
    SUBTITLE_LOGI("--readColorTable-- %d, %d\n", pgsInfo->imageWidth, pgsInfo->imageHeight);
    for (int pos = 2; pos < size; pos += 5) {
        unsigned char y = buf[pos + 1];
        unsigned char u = buf[pos + 2];
        unsigned char v = buf[pos + 3];
        unsigned char r = y + (v - 128) + ((v - 128) * 103 >> 8);
        unsigned char g = y - ((u - 128) * 88 >> 8) - ((v - 128) * 183 >> 8);
        unsigned char b = y + (u - 128) + ((u - 128) * 198 >> 8);
        /*
        R = Y + 1.140V
        G = Y - 0.395U - 0.581V
        B = Y + 2.032U
        */
        pgsInfo->palette[buf[pos]] =
            ((r > 255 ? 255: r) << 24) | ((g > 255 ? 255: g) << 16) | ((b > 255 ? 255: b) << 8) | (buf[pos + 4]);
    }
}

static inline int findObject(int id, PgsInfo *pgsInfo)
{
    for (int i = 0; i < pgsInfo->objectCount; i++) {
        if (pgsInfo->objects[i].id == id)
            return i;
    }
    return -1;
}

static inline int findPalette(int id, PGSSubPalettes *palettes)
{
    for (int i = 0; i < palettes->count; i++) {
        if (palettes->palette[i].id == id)
            return i;
    }
    return -1;
}

static inline unsigned char readBitmap(unsigned char *buf, int size, PgsInfo *pgsInfo) {
    SUBTITLE_LOGI("--readBitmap-- %d, %d\n", pgsInfo->imageWidth, pgsInfo->imageHeight);
    int rleBytes;

    // buf[1]: objectId, buf[2]: versionNum
    pgsInfo->objectId = buf[1];
    pgsInfo->versionNum = buf[2];
    char continueFlag = buf[3];

    if (continueFlag & 0x80) {
        //first
        int objectSize = (buf[4] << 16) | (buf[5] << 8) | (buf[6]);
        pgsInfo->imageWidth = (buf[7] << 8) | (buf[8]);
        pgsInfo->imageHeight = (buf[9] << 8) | (buf[10]);
        SUBTITLE_LOGI("readBitmap values are %d,%d,%d\n", objectSize, pgsInfo->imageWidth, pgsInfo->imageHeight);

        pgsInfo->rleBufSize = 0;
        pgsInfo->rleReadOff = 0;
        pgsInfo->rleBuf = (unsigned char *)malloc(objectSize);
        if (pgsInfo->rleBuf) {
            pgsInfo->rleBufSize = objectSize;
        } else {
            pgsInfo->rleBufSize = 0;
            SUBTITLE_LOGI("readBitmap rleBufSize = 0 \n");
            return -1;
        }
        rleBytes = size - 11;
    } else {
        //last
        rleBytes = size - 4;
    }

    if ((pgsInfo->rleBuf) && ((pgsInfo->rleReadOff + rleBytes) <= pgsInfo->rleBufSize)) {
        memcpy(pgsInfo->rleBuf + pgsInfo->rleReadOff, buf + size - rleBytes, rleBytes);
        pgsInfo->rleReadOff += rleBytes;
    }
    return (continueFlag & 0x40);  //last, 0x40
}

static inline void save2BitmapFile(const char *filename, uint32_t *bitmap, int w, int h) {
    FILE *f;
    char fname[40];
    snprintf(fname, sizeof(fname), "%s.ppm", filename);
    f = fopen(fname, "w");
    if (!f) {
        perror(fname);
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

static inline int drawPixel2Result(int x, int y, unsigned pixel, PgsShowData *result) {
    int lineSize = result->imageWidth*4;
    char *buf = (char *)(result->resultBuf + lineSize*y + x*4);

    if ((x >= result->imageWidth) || (y >= result->imageHeight)) {
        return 0;
    }
    buf[0] = pixel >> 24;
    buf[1] = (pixel >> 16) & 0xff;
    buf[2] = (pixel >> 8) & 0xff;
    buf[3] = pixel & 0xff;
    return 0;
}

static inline void afRlebitmapRender(PgsShowData *showdata, PgsShowData *result, char mode) {
    unsigned char *ptr = NULL;
    unsigned char *endBuf = NULL;
    int x, y, count;
    unsigned char colorIndex;

    if (showdata == NULL) return;

    ptr = showdata->rleBuf;
    endBuf = showdata->rleBuf + showdata->rleBufSize;
    showdata->renderHeight = 0;
    y = 0;
    while ((y < showdata->imageHeight) && (ptr < endBuf)) {
        x = 0;
        while ((x < showdata->imageWidth) && (ptr < endBuf)) {
            if ((*ptr == 0) && (*(ptr + 1) == 0)) {
                if (x > 0) {
                    for (; x < showdata->imageWidth; x++) {
                        drawPixel2Result(x, y, 0, result);
                    }
                }
                ptr += 2;
            } else if (*ptr) {
                drawPixel2Result(x, y,mode ? showdata->palette[*ptr] : *ptr, result);
                x++;
                ptr++;
            } else {
                ptr++;
                if (*ptr & 0x40) {
                    count = ((*ptr & 0x3f) << 8) | (*(ptr + 1));
                    if (*ptr & 0x80) {
                        colorIndex = *(ptr + 2);
                        ptr++;
                    } else {
                        colorIndex = 0;
                    }
                    ptr += 2;
                } else {
                    count = *ptr & 0x3f;
                    if (*ptr & 0x80) {
                        colorIndex = *(ptr + 1);
                        ptr++;
                    } else {
                        colorIndex = 0;
                    }
                    ptr++;
                }
                for (; count > 0; count--, x++) {
                    drawPixel2Result(x, y, mode?showdata->palette[colorIndex]:colorIndex, result);
                }
            }
        }
        y++;
    }
    showdata->renderHeight = y;
}

void PgsParser::checkDebug() {
    //dump pgs subtitle bitmap
    char value[PROPERTY_VALUE_MAX] = {0};
    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("vendor.subtitle.dump", value, "false");
    if (!strcmp(value, "true")) {
        mDumpSub = true;
    } else {
        mDumpSub = false;
    }
}

PgsParser::PgsParser(std::shared_ptr<DataSource> source) {
    mDataSource = source;
    mParseType = TYPE_SUBTITLE_PGS;
    mPgsEpgs = new PgsSubtitleEpgs();
    mPgsEpgs->pgsInfo = new PgsInfo();
    checkDebug();
}

PgsParser::~PgsParser() {
    if (mPgsEpgs != nullptr) {
        if (mPgsEpgs->pgsInfo !=nullptr) {
            delete mPgsEpgs->pgsInfo;
        }
        delete mPgsEpgs;
    }
}

int PgsParser::parserOnePgs(std::shared_ptr<AML_SPUVAR> spu) {
    int bufferSize = 0;
    char filename[32];

    bufferSize = (mPgsEpgs->showdata.imageWidth *
                   mPgsEpgs->showdata.imageHeight * 4);
    if (bufferSize <= 0)
        return -1;
    mPgsEpgs->showdata.resultBuf = (unsigned char *)malloc(bufferSize);
    if (mPgsEpgs->showdata.resultBuf == NULL) {
        SUBTITLE_LOGE("malloc pgs result buf failed \n");
        return -1;
    }
    memset(mPgsEpgs->showdata.resultBuf, 0x0, bufferSize);
    if (mState == SUB_STOP) {
        return 0;
    }
    afRlebitmapRender(&(mPgsEpgs->showdata), &mPgsEpgs->showdata, 1);
    if (mPgsEpgs->showdata.imageWidth == 1920 && mPgsEpgs->showdata.imageHeight == 1080) {
        unsigned char *cutBuffer = (unsigned char *)malloc(bufferSize / 4);
        if (cutBuffer != NULL) {
            memset(cutBuffer, 0x0, bufferSize / 4);
            memcpy(cutBuffer, mPgsEpgs->showdata.resultBuf+bufferSize*3/4, bufferSize/4);
            free(mPgsEpgs->showdata.resultBuf);
            mPgsEpgs->showdata.resultBuf = cutBuffer;
            mPgsEpgs->showdata.imageHeight /= 4;
        } else {
            SUBTITLE_LOGI("malloc cut buffer failed \n ");
        }
    }
    spu->subtitle_type = TYPE_SUBTITLE_PGS;
    spu->spu_start_x = mPgsEpgs->showdata.x;
    spu->spu_start_y = mPgsEpgs->showdata.y;
    spu->spu_data = mPgsEpgs->showdata.resultBuf;
    spu->spu_width = mPgsEpgs->showdata.imageWidth;
    spu->spu_height = mPgsEpgs->showdata.imageHeight;
    spu->spu_origin_display_w = mPgsEpgs->showdata.windowWidth;
    spu->spu_origin_display_h = mPgsEpgs->showdata.windowHeight;
    spu->pts = mPgsEpgs->showdata.pts;
    spu->buffer_size = spu->spu_width * spu->spu_height * 4;
    spu->objectSegmentId = mPgsEpgs->showdata.objectSegmentId;
    if (spu->buffer_size > 0 && spu->spu_data != NULL) {
        if (mDumpSub) {
            snprintf(filename, sizeof(filename), "./data/subtitleDump/pgs(%lld)", spu->pts);
            save2BitmapFile(filename, (uint32_t *)spu->spu_data, spu->spu_width, spu->spu_height);
        }
        if (spu->spu_origin_display_w <= 0 || spu->spu_origin_display_h <= 0) {
            if (mPgsEpgs->pgsInfo->width > 0 && mPgsEpgs->pgsInfo->height > 0) {
                 spu->spu_origin_display_w = mPgsEpgs->pgsInfo->width;
                 spu->spu_origin_display_h = mPgsEpgs->pgsInfo->height;

            } else {
                spu->spu_origin_display_w = VideoInfo::Instance()->getVideoWidth();
                spu->spu_origin_display_h = VideoInfo::Instance()->getVideoHeight();
            }
        }

        addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
    } else {
        SUBTITLE_LOGI("spu buffer size %d, spu->spu_data %p\n", spu->buffer_size,spu->spu_data);
        free(mPgsEpgs->showdata.resultBuf);
    }
    mPgsEpgs->showdata.resultBuf = NULL;
    return 1;
}

int PgsParser::decode(std::shared_ptr<AML_SPUVAR> spu, unsigned char *buf) {
    unsigned char *curBuf = buf;
    int size;
    int startTime, endTime;
    unsigned char type;
    char filename[32];
    PgsInfo *pgsInfo = mPgsEpgs->pgsInfo;
    type = readTimeHeader(&curBuf, &size, &startTime, &endTime);
    switch (type) {
        case PRESENTATION_SEGMENT:     //subpicture header
            readSubpictureHeader(curBuf - size, pgsInfo);
            if (size == 0x13) {
                SUBTITLE_LOGI("enter type 0x16,0x13\n");
                //readSubpictureHeader(curBuf - size, pgsInfo);
            } else if (size == 0xb) {
                //clearSubpictureHeader
                readWindowInfo(curBuf - size,pgsInfo);
                spu->subtitle_type = TYPE_SUBTITLE_PGS;
                spu->pts = startTime;
                if (spu->spu_width != 0 && spu->spu_height != 0) {
                    if (mDumpSub) {
                        snprintf(filename, sizeof(filename), "./data/subtitleDump/pgs(%lld)", spu->pts);
                        save2BitmapFile(filename, (uint32_t *)spu->spu_data, spu->spu_width, spu->spu_height);
                    }
                    if (spu->spu_origin_display_w <= 0 || spu->spu_origin_display_h <= 0) {
                        spu->spu_origin_display_w = VideoInfo::Instance()->getVideoWidth();
                        spu->spu_origin_display_h = VideoInfo::Instance()->getVideoHeight();
                    }
                    addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
                }
            }
            break;
        case WINDOW_SEGMENT:      //window
            if (size == 0xa) {
                //SUBTITLE_LOGI("enter type 0x17, %d\n", read_pgs_byte);
                //readWindowHeader(curBuf - size, pgsInfo);
            }
            break;
        case PALETTE_SEGMENT:      //color table
            //SUBTITLE_LOGI("enter type 0x14 %d\n", read_pgs_byte);
            readColorTable(curBuf - size, size, pgsInfo);
            break;
        case OBJECT_SEGMENT:      //bitmap
            SUBTITLE_LOGI("OBJECT_SEGMENT enter type 0x15\n");
            if (readBitmap(curBuf - size, size, pgsInfo)) {
                SUBTITLE_LOGI("nwpushuai objectId:%d", pgsInfo->objectId);
                int presentationSegmentObjectId = findObject(pgsInfo->objectId, pgsInfo);

                if (mPgsEpgs->pgsInfo->objects[presentationSegmentObjectId].x < 0 || mPgsEpgs->pgsInfo->objects[presentationSegmentObjectId].x > mPgsEpgs->pgsInfo->width) {
                    SUBTITLE_LOGI("fail x nwpushuai --OBJECT_SEGMENT-- y:%d height:%d imageHeight:%d \n", mPgsEpgs->pgsInfo->objects[presentationSegmentObjectId].x, mPgsEpgs->pgsInfo->width, mPgsEpgs->pgsInfo->imageWidth);
                    mPgsEpgs->pgsInfo->objects[presentationSegmentObjectId].x = (mPgsEpgs->pgsInfo->width - mPgsEpgs->pgsInfo->imageWidth) / 2;
                }

                if (mPgsEpgs->pgsInfo->objects[presentationSegmentObjectId].y < 0 || mPgsEpgs->pgsInfo->objects[presentationSegmentObjectId].y > mPgsEpgs->pgsInfo->height) {
                    SUBTITLE_LOGI("fail y nwpushuai --OBJECT_SEGMENT-- y:%d height:%d imageHeight:%d \n", mPgsEpgs->pgsInfo->objects[presentationSegmentObjectId].y, mPgsEpgs->pgsInfo->height, mPgsEpgs->pgsInfo->imageHeight);
                    mPgsEpgs->pgsInfo->objects[presentationSegmentObjectId].y = mPgsEpgs->pgsInfo->height - mPgsEpgs->pgsInfo->imageHeight *2;
                }

                SUBTITLE_LOGI("nwpushuai success readBitmap x:%d y:%d width:%d height:%d windowWidthOffset:%d windowHeightOffset:%d windowWidth%d windowHeight:%d imageWidth:%d imageHeight:%d spu->m_delay:%lld\n",
                    mPgsEpgs->pgsInfo->objects[presentationSegmentObjectId].x,
                    mPgsEpgs->pgsInfo->objects[presentationSegmentObjectId].y,
                    mPgsEpgs->pgsInfo->width,
                    mPgsEpgs->pgsInfo->height,
                    mPgsEpgs->pgsInfo->windowWidthOffset,
                    mPgsEpgs->pgsInfo->windowHeightOffset,
                    mPgsEpgs->pgsInfo->windowWidth,
                    mPgsEpgs->pgsInfo->windowHeight,
                    mPgsEpgs->pgsInfo->imageWidth,
                    mPgsEpgs->pgsInfo->imageHeight,
                    spu->m_delay
                );
                //render it
                mPgsEpgs->showdata.x = mPgsEpgs->pgsInfo->objects[presentationSegmentObjectId].x;
                mPgsEpgs->showdata.y = mPgsEpgs->pgsInfo->objects[presentationSegmentObjectId].y;
                mPgsEpgs->showdata.width = mPgsEpgs->pgsInfo->width;
                mPgsEpgs->showdata.height = mPgsEpgs->pgsInfo->height;
                mPgsEpgs->showdata.windowWidthOffset = mPgsEpgs->pgsInfo->windowWidthOffset;
                mPgsEpgs->showdata.windowHeightOffset = mPgsEpgs->pgsInfo->windowHeightOffset;
                mPgsEpgs->showdata.windowWidth = mPgsEpgs->pgsInfo->windowWidth;
                mPgsEpgs->showdata.windowHeight = mPgsEpgs->pgsInfo->windowHeight;
                mPgsEpgs->showdata.imageWidth = mPgsEpgs->pgsInfo->imageWidth;
                mPgsEpgs->showdata.imageHeight = mPgsEpgs->pgsInfo->imageHeight;
                mPgsEpgs->showdata.palette = mPgsEpgs->pgsInfo->palette;
                mPgsEpgs->showdata.rleBuf = mPgsEpgs->pgsInfo->rleBuf;
                mPgsEpgs->showdata.rleBufSize = mPgsEpgs->pgsInfo->rleBufSize;
                mPgsEpgs->showdata.objectSegmentId = presentationSegmentObjectId;
                SUBTITLE_LOGI("decoder pgs data to show\n\n");
                parserOnePgs(spu);

                if (pgsInfo->rleBuf) {
                    free(pgsInfo->rleBuf);
                    pgsInfo->rleBuf = NULL;
                }

                return 1;
            }
            break;
        case DISPLAY_SEGMENT:      //trailer
            SUBTITLE_LOGI("enter type 0x80\n");
            break;
        default:
            break;
    }
    return 0;
}

int PgsParser::getSpu(std::shared_ptr<AML_SPUVAR> spu) {
    char tmpbuf[256];
    int64_t packetHeader = 0;
    //read_pgs_byte = 0;
    if (mPgsEpgs->pgsInfo == NULL) {
        SUBTITLE_LOGI("pgsInfo is NULL \n");
        return 0;
    }

    if (mState == SUB_INIT) {
        mState = SUB_PLAYING;
    } else if (mState == SUB_STOP) {
        SUBTITLE_LOGI(" subtitle_status == SUB_STOP \n\n");
        return 0;
    }


    packetHeader = 0;
    //uVobSPU.spu_cache_pos = 0;
    while (mDataSource->read(tmpbuf, 1) == 1)  {
        if (mState == SUB_STOP)
            return 0;

        packetHeader = ((packetHeader<<8) & 0x000000ffffffffff) | tmpbuf[0];
        SUBTITLE_LOGI("## get_dvb_spu %x, %llx,-------------\n",tmpbuf[0], packetHeader);

        if ((packetHeader & 0xffffffff) == 0x000001bd) {
            SUBTITLE_LOGI("## 222  get_dvb_teletext_spu hardware demux dvb %x,%llx,-----------\n",
                    tmpbuf[0], packetHeader & 0xffffffffff);
            return hwDemuxParse(spu);
        } else if (((packetHeader & 0xffffffffff)>>8) == AML_PARSER_SYNC_WORD
                && (((packetHeader & 0xff)== 0x77) || ((packetHeader & 0xff)==0xaa))) {
            SUBTITLE_LOGI("## 222  get_dvb_teletext_spu soft demux dvb %x,%llx,-----------\n",
                    tmpbuf[0], packetHeader & 0xffffffffff);
            return softDemuxParse(spu);
        } else {
            SUBTITLE_LOGE("dvb package header error: %x, %llx",tmpbuf[0], packetHeader);
        }
    }

    return 0;
}

int PgsParser::softDemuxParse(std::shared_ptr<AML_SPUVAR> spu) {
    char tmpbuf[256];
    int64_t pts = 0, dts = 0, ptsEnd = 0;
    int packetLen = 0;
    int ret = 0;

    int readDataLen = 0, dataLen = 0;
    int packetType = 0;
    char *data = NULL;
    char *pdata = NULL;
    if (mDataSource->read(tmpbuf, 19) == 19) {
        dataLen = subPeekAsInt32(tmpbuf + 3);
        pts = subPeekAsInt64(tmpbuf + 7);

        spu->m_delay = subPeekAsInt32(tmpbuf + 15);
        if (spu->m_delay == 0) {
            spu->m_delay = pts + (DEFAULT_DELAY_TIME * 1000 * DEFAULT_DVB_TIME_MULTI);
        }
        dts = pts;
        spu->subtitle_type = TYPE_SUBTITLE_PGS;
        spu->pts = pts;
        SUBTITLE_LOGI("## 4444 %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,--%d,%llx,%llx,-------------\n",
                tmpbuf[0], tmpbuf[1], tmpbuf[2], tmpbuf[3],
                tmpbuf[4], tmpbuf[5], tmpbuf[6], tmpbuf[7],
                tmpbuf[8], tmpbuf[9], tmpbuf[10], tmpbuf[11],
                tmpbuf[12], tmpbuf[13], tmpbuf[14], dataLen, pts, ptsEnd);
        data = (char *)calloc(1, dataLen);
        int ret = mDataSource->read(data, dataLen);

        SUBTITLE_LOGI("## ret=%d,dataLen=%d,%x,%x,%x,%x,%x,%x,%x,%x,---------\n",
                ret, dataLen, data[0], data[1], data[2], data[3],
                data[dataLen - 4], data[dataLen - 3], data[dataLen - 2], data[dataLen - 1]);
        pdata = data;
    }

    while (readDataLen < dataLen) {
        SUBTITLE_LOGI("## %x,%x,%x \n", data[0], data[1], data[2]);
        packetType = data[0];
        packetLen = (data[1] << 8) | data[2];
        readDataLen += 3;
        SUBTITLE_LOGI("## read:%d, dataLen:%d, len is %d\n", readDataLen, dataLen, packetLen);
        if (readDataLen + packetLen > dataLen) {
            SUBTITLE_LOGI("## data fault ! ---\n");
            break;
        }

        if ((pts) && (packetLen > 0)) {
            char *buf = NULL;
            if ((8 + 2 + packetLen) > (OSD_HALF_SIZE * 4)) {
                SUBTITLE_LOGE("pgs packet is too big\n\n");
                break;
            }
            /*else if ((uVobSPU.spu_decoding_start_pos + 8 + 2 + packetLen) > (OSD_HALF_SIZE * 4)) {
                uVobSPU.spu_decoding_start_pos = 0;
            }*/

            buf = (char *)malloc(8 + 2 + 3 + packetLen);
            SUBTITLE_LOGI("packetLen is %d, %p\n", packetLen, buf);
            mPgsEpgs->showdata.pts = dts;
            if (mState == SUB_STOP) {
                ret = 0;
                if (buf)  free(buf);
                return -1;
            }

            if (buf) {
                SUBTITLE_LOGI("## 555 get_pgs_spu ------------\n");
                memset(buf, 0x0,8 + 2 + 3 + packetLen);
                buf[0] = 'P';
                buf[1] = 'G';
                buf[2] = (pts >> 24) & 0xff;
                buf[3] = (pts >> 16) & 0xff;
                buf[4] = (pts >> 8) & 0xff;
                buf[5] = pts & 0xff;
                buf[6] = (ptsEnd >> 24) & 0xff;
                buf[7] = (ptsEnd >> 16) & 0xff;
                buf[8] = (ptsEnd >> 8) & 0xff;
                buf[9] = ptsEnd & 0xff;
                buf[10] = packetType & 0xff, buf[11] =
                        (packetLen >> 8) & 0xff,
                        buf[12] = packetLen & 0xff;
                memcpy(buf + 13, data + 3,
                       packetLen);
                SUBTITLE_LOGI("## start decode pgs subtitle %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,\n\n",
                        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
                        buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14]);
                ret = decode(spu, (unsigned char *)buf);
                readDataLen += packetLen;
                data += packetLen + 3;
                free(buf);
                buf = NULL;
            }
        }
    }
    SUBTITLE_LOGI("## break, get_pgs_spu readDataLen=%d,dataLen=%d,spu->spu_data=%p\n",
            readDataLen, dataLen, spu->spu_data);
    if (pdata) free(pdata);

    return 0;
}
int PgsParser::hwDemuxParse(std::shared_ptr<AML_SPUVAR> spu) {
    char tmpbuf[256];
    int64_t pts = 0, dts = 0;
    int64_t tmpPts, tmpDts;
    int packetLen = 0, pesHeaderLen = 0;
    bool needSkipPkt = true;
    //read_pgs_byte = 0;
    SUBTITLE_LOGI("enter get_pgs_spu\n");
    int ret = 0;

    if (mDataSource->read(tmpbuf, 2) == 2) {
        packetLen = (tmpbuf[0] << 8) | tmpbuf[1];

        if (packetLen >= 3) {
            if (mDataSource->read(tmpbuf, 3) == 3) {
                packetLen -= 3;
                pesHeaderLen = tmpbuf[2];
                if (packetLen >= pesHeaderLen) {
                    if ((tmpbuf[1] & 0xc0) == 0x80) {
                        needSkipPkt = false;
                        if (mDataSource->read(tmpbuf, pesHeaderLen) == pesHeaderLen) {
                            tmpPts = (int64_t)(tmpbuf[0] & 0xe) << 29;
                            tmpPts = tmpPts | ((tmpbuf[1] & 0xff) << 22);
                            tmpPts = tmpPts | ((tmpbuf[2] & 0xfe) << 14);
                            tmpPts = tmpPts | ((tmpbuf[3] & 0xff) << 7);
                            tmpPts = tmpPts | ((tmpbuf[4] & 0xfe) >> 1);
                            pts = tmpPts; // - pts_aligned;
                            packetLen -= pesHeaderLen;
                        }
                    } else if ((tmpbuf[1] & 0xc0) == 0xc0) {
                        needSkipPkt = false;
                        if (mDataSource->read(tmpbuf, pesHeaderLen) == pesHeaderLen) {
                            tmpPts =  (int64_t)(tmpbuf[0] & 0xe) << 29;
                            tmpPts = tmpPts | ((tmpbuf[1] & 0xff) << 22);
                            tmpPts = tmpPts | ((tmpbuf[2] & 0xfe) << 14);
                            tmpPts = tmpPts | ((tmpbuf[3] & 0xff) << 7);
                            tmpPts = tmpPts | ((tmpbuf[4] & 0xfe) >> 1);
                            pts = tmpPts; //- pts_aligned;
                            tmpDts = (int64_t)(tmpbuf[5] & 0xe) << 29;
                            tmpDts = tmpDts | ((tmpbuf[6] & 0xff) << 22);
                            tmpDts = tmpDts | ((tmpbuf[7] & 0xfe) << 14);
                            tmpDts = tmpDts | ((tmpbuf[8] & 0xff) << 7);
                            tmpDts = tmpDts | ((tmpbuf[9] & 0xfe) >> 1);
                            dts = tmpDts; // - pts_aligned;
                            packetLen -= pesHeaderLen;
                        }
                    }
                }
            }
        }

        if (needSkipPkt) {
            char tmp;
            for (int iii = 0; iii < packetLen; iii++) {
                if (mDataSource->read(&tmp, 1) == 0) break;
            }
        } else if ((pts) && (packetLen > 0)) {
            char *buf = NULL;
            if ((8 + 2 + packetLen) > (OSD_HALF_SIZE * 4)) {
                SUBTITLE_LOGE("pgs packet is too big\n\n");
                return -1;
            /*} else if ((uVobSPU.spu_decoding_start_pos + 8 + 2 + packetLen) > (OSD_HALF_SIZE * 4)) {
                uVobSPU.spu_decoding_start_pos =0;*/
            }
            buf = (char *)malloc(8 + 2 + packetLen);
            SUBTITLE_LOGI("packetLen is %d\n",packetLen);
            mPgsEpgs->showdata.pts = dts;
            if (mDataSource->availableDataSize() < packetLen || mState == SUB_STOP) {
                ret = 0;
                if (buf) free(buf);
                return 0;
            }

            if (buf) {
                memset(buf, 0x0,8 + 2 +packetLen);
                buf[0] = 'P';
                buf[1] = 'G';
                buf[2] = (pts >> 24) & 0xff;
                buf[3] = (pts >> 16) & 0xff;
                buf[4] = (pts >> 8) & 0xff;
                buf[5] = pts & 0xff;
                buf[6] = (pts >> 24) & 0xff;
                buf[7] = (pts >> 16) & 0xff;
                buf[8] = (pts >> 8) & 0xff;
                buf[9] = pts & 0xff;
                if (mDataSource->read(buf + 10, packetLen) == packetLen) {
                    SUBTITLE_LOGI("start decode pgs subtitle\n\n");
                    ret = decode(spu, (unsigned char *)buf);
                }
                free(buf);
                buf = NULL;
            }
            if (ret == 1) return ret;
        }
    }

    return 0;
}

int PgsParser::getInterSpu() {
    std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());

    spu->sync_bytes = AML_PARSER_SYNC_WORD;
    return getSpu(spu);
}


int PgsParser::parse() {
    while (!mThreadExitRequested) {
        if (getInterSpu() < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(83));
        }
    }

    return 0;
}

void PgsParser::dump(int fd, const char *prefix) {
    dprintf(fd, "%s PGS Parser\n", prefix);
    dumpCommon(fd, prefix);

    dprintf(fd, "\n%s   PgsInfo: %p\n", prefix, mPgsEpgs->pgsInfo);
    if (mPgsEpgs->pgsInfo != nullptr) {
        PgsInfo *pgs = mPgsEpgs->pgsInfo;
        dprintf(fd, "%s     startTime:%d endTime:%d\n", prefix, pgs->startTime, pgs->endTime);
        dprintf(fd, "%s     w:%d h:%d\n", prefix, pgs->width, pgs->height);
        dprintf(fd, "%s     fpsCode:%d\n", prefix, pgs->fpsCode);
        dprintf(fd, "%s     objectCount:%d\n", prefix, pgs->objectCount);
        dprintf(fd, "%s     objectCount:%d\n", prefix, pgs->objectCount);

        dprintf(fd, "%s     state:%d\n", prefix, pgs->state);
        dprintf(fd, "%s     paletteUpdateFlag:%d\n", prefix, pgs->paletteUpdateFlag);
        dprintf(fd, "%s     paletteIdRef:%d\n", prefix, pgs->paletteIdRef);
        dprintf(fd, "%s     number:%d\n", prefix, pgs->number);

        dprintf(fd, "%s     window Offset(x:%d y:%d)\n", prefix, pgs->windowWidthOffset, pgs->windowHeightOffset);
        dprintf(fd, "%s     window(w:%d h:%d)\n", prefix, pgs->windowWidth, pgs->windowHeight);

        dprintf(fd, "%s     rleBufferSize(%d)\n", prefix, pgs->rleBufSize);
        dprintf(fd, "%s     palette:\n%s       ", prefix, prefix);
        for (int i= 0; i< 0x100; i++) {
            dprintf(fd, "%08x ", pgs->palette[i]);
            if (i%8 == 7) {
                dprintf(fd, "\n%s       ", prefix);
            }
        }
        dprintf(fd, "\n");
    }

    dprintf(fd, "\n%s   ShowData:\n", prefix);
    dprintf(fd, "%s     x:%d y:%d w:%d h:%d\n", prefix,
        mPgsEpgs->showdata.x, mPgsEpgs->showdata.y, mPgsEpgs->showdata.width, mPgsEpgs->showdata.height);
    dprintf(fd, "%s     window Offset(x:%d y:%d)\n", prefix,
        mPgsEpgs->showdata.windowWidthOffset, mPgsEpgs->showdata.windowWidthOffset);
    dprintf(fd, "%s     window(w:%d h:%d)\n", prefix, mPgsEpgs->showdata.windowWidth, mPgsEpgs->showdata.windowHeight);

    dprintf(fd, "%s     image(w:%d h:%d)\n", prefix, mPgsEpgs->showdata.imageWidth, mPgsEpgs->showdata.imageHeight);
}



