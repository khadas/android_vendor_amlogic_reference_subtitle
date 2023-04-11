/*
 * TTML decoding for ffmpeg
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
#define  LOG_TAG "TtmlParser"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
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

#include "TtmlParser.h"
#include "tinyxml2.h"

#define  TRACE()    LOGI("[%s::%d]\n",__FUNCTION__,__LINE__)

#define LOGI ALOGI
#define LOGD ALOGD
#define LOGE ALOGE

#define MAX_BUFFERED_PAGES 25
#define BITMAP_CHAR_WIDTH  12
#define BITMAP_CHAR_HEIGHT 10
#define OSD_HALF_SIZE (1920*1280/8)

#define HIGH_32_BIT_PTS 0xFFFFFFFF
#define TSYNC_32_BIT_PTS 0xFFFFFFFF

bool static inline isMore32Bit(int64_t pts) {
    if (((pts >> 32) & HIGH_32_BIT_PTS) > 0) {
        return true;
    }
    return false;
}

static void save2BitmapFile(const char *filename, uint32_t *bitmap, int w, int h)
{
    LOGI("png_save2:%s\n",filename);
    FILE *f;
    char fname[40];

    snprintf(fname, sizeof(fname), "%s.bmp", filename);
    f = fopen(fname, "w");
    if (!f) {
        ALOGE("Error cannot open file %s!", fname);
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

/* EBU-TT-D timecodes have format hours:minutes:seconds[.fraction] */
static int ttml_parse_timecode (const char * timestring)
{
  int hours = 0, minutes = 0, seconds = 0, milliseconds = 0, time = 0;

  LOGI("%s time string: %s\n",__FUNCTION__, timestring);
  bool success = sscanf(timestring, "%d:%d:%d.%d", &hours, &minutes, &seconds, &milliseconds) == 4;
  if (!success || hours < 0 || hours > 23 || minutes < 0 || minutes > 59 || seconds < 0 || seconds > 59 || milliseconds < 0 || milliseconds > 999) {
    LOGE ("%s invalid time string.", __FUNCTION__, timestring);
  }

  LOGI("hours:%d minutes:%d seconds:%d milliseconds:%d", hours, minutes, seconds, milliseconds);
  time = (hours * 3600 + minutes * 60 + seconds ) * 1000+ milliseconds;
  return time;
}

/* Draw a page as bitmap */
static int genSubBitmap(TtmlContext *ctx, AVSubtitleRect *subRect, vbi_page *page, int chopTop)
{
    int resx = page->columns * BITMAP_CHAR_WIDTH;
    int resy;
    int height;

    uint8_t ci;
    subRect->type = SUBTITLE_BITMAP;
    return 0;
}

TtmlParser::TtmlParser(std::shared_ptr<DataSource> source) {
    mDataSource = source;
    mParseType = TYPE_SUBTITLE_TTML;
    mIndex = 0;
    mDumpSub = 0;
    mPendingAction = -1;
    mTimeoutThread = std::shared_ptr<std::thread>(new std::thread(&TtmlParser::callbackProcess, this));

    initContext();
    checkDebug();
    sInstance = this;
}

TtmlParser *TtmlParser::sInstance = nullptr;

TtmlParser *TtmlParser::getCurrentInstance() {
    return TtmlParser::sInstance;
}

TtmlParser::~TtmlParser() {
    LOGI("%s", __func__);
    stopParser();
    sInstance = nullptr;

    // mContext need protect. accessed by other api or the ttThread.
    mMutex.lock();
    if (mContext != nullptr) {
        mContext->pts = AV_NOPTS_VALUE;
        delete mContext;
        mContext = nullptr;
    }
    mMutex.unlock();
    {   //To TimeoutThread: wakeup! we are exiting...
        std::unique_lock<std::mutex> autolock(mMutex);
        mPendingAction = 1;
        mCv.notify_all();
    }
    mTimeoutThread->join();
}

static inline int generateNormalDisplay(AVSubtitleRect *subRect, unsigned char *des, uint32_t *src, int width, int height) {
    LOGE(" generateNormalDisplay width = %d, height=%d\n",width, height);
    int mt =0, mb =0;
    int ret = -1;
    TtmlParser *parser = TtmlParser::getCurrentInstance();

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

#ifdef SUPPORT_GRAPHICS_MODE_SUBTITLE_PAGE_FULL_SCREEN
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
    return ret;
}

/**
 *  TTML has interaction, with event handling
 *  This function main for this. the control interface.not called in parser thread, need protect.
 */
bool TtmlParser::updateParameter(int type, void *data) {
    if (TYPE_SUBTITLE_TTML == type) {
        DtvKitTtmlParam *pTtmlParam = (DtvKitTtmlParam* )data;
    }
    return true;
}

int TtmlParser::initContext() {
    std::unique_lock<std::mutex> autolock(mMutex);
    mContext = new TtmlContext();
    if (!mContext) {
        LOGE("[%s::%d]malloc error! \n", __FUNCTION__, __LINE__);
    }
    mContext->formatId = 0;
    mContext->pts = AV_NOPTS_VALUE;
    return 0;
}

void TtmlParser::checkDebug() {
    char value[PROPERTY_VALUE_MAX] = {0};
    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("vendor.subtitle.dump", value, "false");
    if (!strcmp(value, "true")) {
        mDumpSub = true;
    }
}

int TtmlParser::TtmlDecodeFrame(std::shared_ptr<AML_SPUVAR> spu, char *srcData, int srcLen) {
    int bufSize = srcLen;
    const uint8_t *p, *pEnd;
    const char *begin = NULL, *end = NULL;
    int pes_packet_start_position = 5;

    while (srcData[0] != '<' && srcLen > 0) {
        srcData++;
        srcLen--;
    }

    while (srcData[bufSize] != '>' && bufSize > 0) {
        bufSize--;
    }

    bufSize = bufSize+1;
    char buf[bufSize];
    for (int i = 0; i<bufSize; i++) {
        buf[i] = srcData[i];
    }

    ALOGD("%s bufSize:%d buf:%s ", __FUNCTION__, bufSize, buf);
    tinyxml2::XMLDocument doc;
    int ret = doc.Parse(buf, bufSize);
    if (ret != tinyxml2::XML_SUCCESS) {
        ALOGD("%s ret:%d Failed to parse XML", __FUNCTION__, ret);
        return -1;
    }

    tinyxml2::XMLElement* root = doc.RootElement();
    if (root == NULL) {
        ALOGD("%s Failed to load file: No root element.", __FUNCTION__);
        doc.Clear();
        return -1;
    }

    // TODO: parse header, if we support style metadata and layout property
    // parse body
    tinyxml2::XMLElement *body = root->FirstChildElement("body");
    if (body == nullptr) body = root->FirstChildElement("tt:body");
    if (body == nullptr) {
        ALOGD("%s Error. no body found!", __FUNCTION__);
        doc.Clear();
        return -1;
    }

    tinyxml2::XMLElement *div = body->FirstChildElement("div");
    if (div == nullptr) div = body->FirstChildElement("tt:div");
    tinyxml2::XMLElement *parag;
    tinyxml2::XMLElement *spanarag;
    if (div == nullptr) {
        ALOGD("%s %d", __FUNCTION__, __LINE__);
        parag = root->FirstChildElement("p");
        if (parag == nullptr) parag = root->FirstChildElement("tt:p");
    } else {
        ALOGD("%s %d", __FUNCTION__, __LINE__);
        parag = div->FirstChildElement("p");
        if (parag == nullptr) parag = div->FirstChildElement("tt:p");
    }

    while (parag != nullptr) {
        const tinyxml2::XMLAttribute *attr = parag->FindAttribute("xml:id");
        if (attr != nullptr) ALOGD("parseXml xml:id=%s\n", attr->Value());

        attr = parag->FindAttribute("begin");
        if (attr != nullptr) {
            //spu->start = attr->FloatValue() *100; // subtitle pts multiply
            begin = attr->Value();
            ALOGD("%s parseXml begin:%s\n", __FUNCTION__, attr->Value());
        }

        attr = parag->FindAttribute("end");
        if (attr != nullptr) {
            //spu->end = attr->FloatValue() *100; // subtitle pts multiply
            end = attr->Value();
            ALOGD("%s parseXml end:%s\n", __FUNCTION__, attr->Value());
        }

        ALOGD("%s parseXml Text:%s\n", __FUNCTION__, parag->GetText());
        if (parag->GetText() == nullptr) {
            spanarag = parag->FirstChildElement("tt:span");
            ALOGD("parseXml spanarag Text:%s spu->pts:%lld %lld\n", spanarag->GetText(),spu->pts, (ttml_parse_timecode(begin) - ttml_parse_timecode(end)) / 90000);
            spu->pts = spu->pts + (ttml_parse_timecode(begin) - ttml_parse_timecode(end)) / 9000;
            spu->spu_data = (unsigned char *)malloc(strlen(spanarag->GetText()));
            memset(spu->spu_data, 0, strlen(spanarag->GetText()));
            memcpy(spu->spu_data, spanarag->GetText(), strlen(spanarag->GetText()));
        }else {
            ALOGD("parseXml parag Text:%s spu->pts:%lld %lld\n", parag->GetText(),spu->pts, (ttml_parse_timecode(begin) - ttml_parse_timecode(end)) / 90000);
            spu->pts = spu->pts + (ttml_parse_timecode(begin) - ttml_parse_timecode(end)) / 9000;
            spu->spu_data = (unsigned char *)malloc(strlen(parag->GetText()));
            memset(spu->spu_data, 0, strlen(parag->GetText()));
            memcpy(spu->spu_data, parag->GetText(), strlen(parag->GetText()));
        }
        parag = parag->NextSiblingElement();
        addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
    }
    doc.Clear();
    return 0;

}

int TtmlParser::getTTMLSpu() {
    char tmpbuf[8] = {0};
    int64_t packetHeader = 0;

    if (mDumpSub) LOGI("enter get_dvb_ttml_spu\n");
    int ret = -1;

    while (mDataSource->read(tmpbuf, 1) == 1) {
        if (mState == SUB_STOP) {
            return 0;
        }

        std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());
        spu->sync_bytes = AML_PARSER_SYNC_WORD;

        packetHeader = ((packetHeader<<8) & 0x000000ffffffffff) | tmpbuf[0];
        if (mDumpSub) LOGI("## get_dvb_spu %x, %llx,-------------\n",tmpbuf[0], packetHeader);

        if ((packetHeader & 0xffffffff) == 0x000001bd) {
            ret = hwDemuxParse(spu);
        } else if (((packetHeader & 0xffffffffff)>>8) == AML_PARSER_SYNC_WORD
                && (((packetHeader & 0xff)== 0x77) || ((packetHeader & 0xff)==0xaa))) {
            ret = softDemuxParse(spu);
        } else if (((packetHeader & 0xffffffffff)>>8) == AML_PARSER_SYNC_WORD
            && ((packetHeader & 0xff)== 0x41)) {//AMLUA  ATV TTML
            ret = atvHwDemuxParse(spu);
        }
    }

    return ret;
}

int TtmlParser::hwDemuxParse(std::shared_ptr<AML_SPUVAR> spu) {
    char tmpbuf[256] = {0};
    int64_t dvbPts = 0, dvbDts = 0;
    int64_t tempPts = 0, tempDts = 0;
    int packetLen = 0, pesHeaderLen = 0;
    bool needSkipData = false;
    int ret = 0;

    if (mDataSource->read(tmpbuf, 2) == 2) {
        packetLen = (tmpbuf[0] << 8) | tmpbuf[1];
        if (packetLen >= 3) {
            if (mDataSource->read(tmpbuf, 3) == 3) {
                packetLen -= 3;
                pesHeaderLen = tmpbuf[2];
                LOGI("get_dvb_ttml spu-packetLen:%d, pesHeaderLen:%d\n", packetLen,pesHeaderLen);

                if (packetLen >= pesHeaderLen) {
                    if ((tmpbuf[1] & 0xc0) == 0x80) {
                        if (mDataSource->read(tmpbuf, pesHeaderLen) == pesHeaderLen) {
                            tempPts = (int64_t)(tmpbuf[0] & 0xe) << 29;
                            tempPts = tempPts | ((tmpbuf[1] & 0xff) << 22);
                            tempPts = tempPts | ((tmpbuf[2] & 0xfe) << 14);
                            tempPts = tempPts | ((tmpbuf[3] & 0xff) << 7);
                            tempPts = tempPts | ((tmpbuf[4] & 0xfe) >> 1);
                            dvbPts = tempPts; // - pts_aligned;
                            packetLen -= pesHeaderLen;
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
                            packetLen -= pesHeaderLen;
                        }
                    } else {
                        // No PTS, has the effect of displaying immediately.
                        //needSkipData = true;
                        mDataSource->read(tmpbuf, pesHeaderLen);
                        packetLen -= pesHeaderLen;
                    }
                } else {
                    needSkipData = true;
                }
            }
        } else {
           needSkipData = true;
        }

        if (needSkipData) {
            if (packetLen < 0 || packetLen > INT_MAX) {
                LOGE("illegal packetLen!!!\n");
                return false;
            }
            for (int iii = 0; iii < packetLen; iii++) {
                char tmp;
                if (mDataSource->read(&tmp, 1)  == 0) {
                    return -1;
                }
            }
        } else if (packetLen > 0) {
            char *buf = NULL;
            if ((packetLen) > (OSD_HALF_SIZE * 4)) {
                LOGE("dvb packet is too big\n\n");
                return -1;
            }
            spu->subtitle_type = TYPE_SUBTITLE_TTML;
            spu->pts = dvbPts;
            if (isMore32Bit(spu->pts) && !isMore32Bit(mDataSource->getSyncTime())) {
                ALOGD("SUB PTS is greater than 32 bits, before subpts: %lld, vpts:%lld", spu->pts, mDataSource->getSyncTime());
                spu->pts &= TSYNC_32_BIT_PTS;
            }
            //If gap time is large than 9 secs, think pts skip,notify info
            if (abs(mDataSource->getSyncTime() - spu->pts) > 9*90000) {
                LOGE("[%s::%d]pts skip,vpts:%lld, spu->pts:%lld notify time error!\n", __FUNCTION__, __LINE__, mDataSource->getSyncTime(), spu->pts);
            }

            LOGI("[%s::%d]pts ---------------,vpts:%lld, spu->pts:%lld\n", __FUNCTION__, __LINE__, mDataSource->getSyncTime(), spu->pts);

            buf = (char *)malloc(packetLen);
            if (buf) {
                LOGI("packetLen is %d, pts is %llx, delay is %llx\n", packetLen, spu->pts, tempPts);
            } else {
                LOGI("packetLen buf malloc fail!!!\n");
            }

            if (buf) {
                memset(buf, 0x0, packetLen);
                if (mDataSource->read(buf, packetLen) == packetLen) {
                    ret = TtmlDecodeFrame(spu, buf, packetLen);
                    LOGI(" @@@@@@@hwDemuxParse parse ret:%d", ret);
                    if (ret != -1) {
                        LOGI("dump-pts-hwdmx!success.\n");
                        if (buf) free(buf);
                        return 0;
                    } else {
                        LOGI("dump-pts-hwdmx!error.\n");
                        if (buf) free(buf);
                        return -1;
                    }
                }
                LOGI("packetLen buf free=%p\n", buf);
                LOGI("@@[%s::%d]free ptr=%p\n", __FUNCTION__, __LINE__, buf);
                free(buf);
            }
        }
    }

    return ret;
}

int TtmlParser::softDemuxParse(std::shared_ptr<AML_SPUVAR> spu) {
    char tmpbuf[256] = {0};
    int64_t dvbPts = 0, ptsDiff = 0;
    int ret = 0;
    int dataLen = 0;
    char *data = NULL;

    // read package header info
    if (mDataSource->read(tmpbuf, 19) == 19) {
        if (mDumpSub) LOGI("## %s get_dvb_spu %x,%x,%x,  %x,%x,%x,  %x,%x,-------------\n",
                __FUNCTION__,
                tmpbuf[0], tmpbuf[1], tmpbuf[2], tmpbuf[3],
                tmpbuf[4], tmpbuf[5], tmpbuf[6], tmpbuf[7]);

        dataLen = subPeekAsInt32(tmpbuf + 3);
        dvbPts  = subPeekAsInt64(tmpbuf + 7);
        ptsDiff = subPeekAsInt32(tmpbuf + 15);

        spu->subtitle_type = TYPE_SUBTITLE_TTML;
        spu->pts = dvbPts;
        if (mDumpSub) LOGI("## %s spu-> pts:%lld,dvPts:%lld\n", __FUNCTION__, spu->pts, dvbPts);
        if (mDumpSub) LOGI("## %s datalen=%d,pts=%llx,delay=%llx,diff=%llx, data: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
                __FUNCTION__, dataLen, dvbPts, spu->m_delay, ptsDiff,
                tmpbuf[0], tmpbuf[1], tmpbuf[2], tmpbuf[3], tmpbuf[4],
                tmpbuf[5], tmpbuf[6], tmpbuf[7], tmpbuf[8], tmpbuf[9],
                tmpbuf[10], tmpbuf[11], tmpbuf[12], tmpbuf[13], tmpbuf[14]);

        data = (char *)malloc(dataLen);
        if (!data) {
            LOGE("[%s::%d]malloc error! \n", __FUNCTION__,__LINE__);
            return -1;
        }
        if (mDumpSub) LOGI("@@[%s::%d]malloc ptr=%p, size = %d\n",__FUNCTION__, __LINE__, data, dataLen);
        memset(data, 0x0, dataLen);
        ret = mDataSource->read(data, dataLen);
        if (mDumpSub) LOGI("## ret=%d,dataLen=%d, %x,%x,%x,%x,%x,%x,%x,%x,---------\n",
                ret, dataLen, data[0], data[1], data[2], data[3],
                data[4], data[5], data[6], data[7]);

        ret = TtmlDecodeFrame(spu, data, dataLen);
        if (ret != -1 && spu->buffer_size > 0) {
            if (mDumpSub) LOGI("dump-pts-swdmx!success pts(%lld) mIndex:%d frame was add\n", spu->pts, mIndex);
            addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
            {
                std::unique_lock<std::mutex> autolock(mMutex);
                mPendingAction = 1;
                mCv.notify_all();
            }
        } else {
            if (mDumpSub) LOGI("dump-pts-swdmx!error this pts(%lld) frame was abandon\n", spu->pts);
        }

        if (data) {
            free(data);
            data = NULL;
        }
    }
    return ret;
}

int TtmlParser::atvHwDemuxParse(std::shared_ptr<AML_SPUVAR> spu) {
        char tmpbuf[256] = {0};
        int64_t dvbPts = 0, ptsDiff = 0;
        int ret = 0;
        int lineNum = 0;
        char *data = NULL;

        // read package header info
        if (mDataSource->read(tmpbuf, 4) == 4) {
            spu->subtitle_type = TYPE_SUBTITLE_TTML;
            data = (char *)malloc(ATV_TTML_DATA_LEN);
            if (!data) {
                LOGE("[%s::%d]malloc error! \n", __FUNCTION__,__LINE__);
                return -1;
            }
            memset(data, 0x0, ATV_TTML_DATA_LEN);
            ret = mDataSource->read(data, ATV_TTML_DATA_LEN);
            if (mDumpSub) LOGI("[%s::%d] ret=%d,dataLen=%d, %x,%x,%x,%x,%x,%x,%x,%x,---------\n",
                    __FUNCTION__, __LINE__, ret, ATV_TTML_DATA_LEN, data[0], data[1], data[2], data[3],
                    data[4], data[5], data[6], data[7]);

            ret = TtmlDecodeFrame(spu, data, ATV_TTML_DATA_LEN);

            if (ret != -1 && spu->buffer_size > 0) {
                if (mDumpSub) LOGI("[%s::%d]dump-pts-atvHwDmx!success pts(%lld) mIndex:%d frame was add\n", __FUNCTION__,__LINE__, spu->pts, ++mIndex);
                if (spu->spu_origin_display_w <= 0 || spu->spu_origin_display_h <= 0) {
                    spu->spu_origin_display_w = VideoInfo::Instance()->getVideoWidth();
                    spu->spu_origin_display_h = VideoInfo::Instance()->getVideoHeight();
                }
                //ttx,need immediatePresent show
                addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
            } else {
                if (mDumpSub) LOGI("[%s::%d]dump-pts-atvHwDmx!error this pts(%lld) frame was abandon\n", __FUNCTION__,__LINE__, spu->pts);
            }

            if (data) {
                free(data);
                data = NULL;
            }
        }
    return ret;
}

void TtmlParser::callbackProcess() {
    int timeout = 60;//property_get_int32("vendor.sys.subtitleService.decodeTimeOut", 60);
    const int SIGNAL_UNSPEC = -1;
    const int NO_SIGNAL = 0;
    const int HAS_SIGNAL = 1;

    // three states flag.
    int signal = SIGNAL_UNSPEC;

    while (!mThreadExitRequested) {
        std::unique_lock<std::mutex> autolock(mMutex);
        mCv.wait_for(autolock, std::chrono::milliseconds(timeout*1000));

        // has pending action, this is new subtitle decoded.
        if (mPendingAction == 1) {
            if (signal != HAS_SIGNAL) {
                mNotifier->onSubtitleAvailable(1);
                signal = HAS_SIGNAL;
            }
            mPendingAction = -1;
            continue;
        }

        // timedout:
        if (signal != NO_SIGNAL) {
            signal = NO_SIGNAL;
            mNotifier->onSubtitleAvailable(0);
        }
    }
}

int TtmlParser::getSpu() {
    if (mState == SUB_INIT) {
        mState = SUB_PLAYING;
    } else if (mState == SUB_STOP) {
        ALOGD(" mState == SUB_STOP \n\n");
        return 0;
    }

    return getTTMLSpu();
}

int TtmlParser::getInterSpu() {
    return getSpu();
}

int TtmlParser::parse() {
    while (!mThreadExitRequested) {
        if (getInterSpu() < 0) {
            // advance input, if parse failed, wait and retry.
            //std::this_thread::sleep_for(std::chrono::milliseconds(1));
            usleep(100);
        }
    }
    return 0;
}

void TtmlParser::dump(int fd, const char *prefix) {
    //TODO: dump run in binder thread, may need add lock!
    dprintf(fd, "%s TTML Parser\n", prefix);
    dumpCommon(fd, prefix);

    if (mContext != nullptr) {
        dprintf(fd, "%s  formatId: %d (0 = bitmap, 1 = text/ass)\n", prefix, mContext->formatId);
        dprintf(fd, "%s  pts:%lld\n", prefix, mContext->pts);
        dprintf(fd, "\n");
    }
}
