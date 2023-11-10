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
#include <utils/CallStack.h>
#include <unistd.h>
#include <fcntl.h>
#include <list>
#include <thread>
#include <algorithm>
#include <functional>


#include "bprint.h"
#include "StreamUtils.h"
#include "VideoInfo.h"

#include "ParserFactory.h"
#include "TtmlParser.h"
#include "tinyxml2.h"

#define MAX_BUFFERED_PAGES 25
#define BITMAP_CHAR_WIDTH 12
#define BITMAP_CHAR_HEIGHT 10
#define OSD_HALF_SIZE (1920 * 1280 / 8)

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
    SUBTITLE_LOGI("png_save2:%s\n",filename);
    FILE *f;
    char fname[40];

    snprintf(fname, sizeof(fname), "%s.bmp", filename);
    f = fopen(fname, "w");
    if (!f) {
        SUBTITLE_LOGE("Error cannot open file %s!", fname);
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
static int64_t ttml_parse_timecode(const char * beginTimeString, const char * endTimeString)
{
  int64_t beginHours = 0, beginMinutes = 0, beginSeconds = 0, beginMilliseconds = 0,
            endHours = 0, endMinutes = 0, endSeconds = 0, endMilliseconds = 0,
            time = 0;

  SUBTITLE_LOGI("%s begin time string: %s, end time string: %s\n",__FUNCTION__, beginTimeString, endTimeString);
  bool beginSuccess = sscanf(beginTimeString, "%d:%d:%d.%d", &beginHours, &beginMinutes, &beginSeconds, &beginMilliseconds) == 4;
  bool endSuccess = sscanf(endTimeString, "%d:%d:%d.%d", &endHours, &endMinutes, &endSeconds, &endMilliseconds) == 4;
  if (!beginSuccess || beginHours < 0 || beginHours > 23 || beginMinutes < 0 || beginMinutes > 59 || beginSeconds < 0 || beginSeconds > 59 || beginMilliseconds < 0 || beginMilliseconds > 999) {
    SUBTITLE_LOGE ("%s invalid begin time string.", __FUNCTION__, beginTimeString);
  }

  if (!endSuccess || endHours < 0 || endHours > 23 || endMinutes < 0 || endMinutes > 59 || endSeconds < 0 || endSeconds > 59 || endMilliseconds < 0 || endMilliseconds > 999) {
    SUBTITLE_LOGE ("%s invalid end time string.", __FUNCTION__, endTimeString);
  }

  time = ((endHours - beginHours) * 3600 + (endMinutes - beginMinutes) * 60 + (endSeconds - beginSeconds) ) * 1000+ (endMilliseconds - beginMilliseconds);
  SUBTITLE_LOGI("%s time:%lld   end\n",__FUNCTION__, time);
  return time;
}

TtmlParser::TtmlParser(std::shared_ptr<DataSource> source) {
    mDataSource = source;
    mParseType = TYPE_SUBTITLE_DVB_TTML;
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
    SUBTITLE_LOGI("%s", __func__);
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
    SUBTITLE_LOGE(" generateNormalDisplay width = %d, height=%d\n",width, height);
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
    if (TYPE_SUBTITLE_DVB_TTML == type) {
        TtmlParam *pTtmlParam = (TtmlParam* )data;
    }
    return true;
}

int TtmlParser::initContext() {
    std::unique_lock<std::mutex> autolock(mMutex);
    mContext = new TtmlContext();
    if (!mContext) {
        SUBTITLE_LOGE("[%s::%d]malloc error! \n", __FUNCTION__, __LINE__);
    }
    mContext->formatId = 0;
    mContext->pts = AV_NOPTS_VALUE;
    return 0;
}

void TtmlParser::checkDebug() {
    #ifdef NEED_DUMP_ANDROID
    char value[PROPERTY_VALUE_MAX] = {0};
    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("vendor.subtitle.dump", value, "false");
    if (!strcmp(value, "true")) {
        mDumpSub = true;
    }
    #endif
}

int TtmlParser::TtmlDecodeFrame(std::shared_ptr<AML_SPUVAR> spu, char *srcData, int srcLen) {
    int bufSize = srcLen;
    const uint8_t *p, *pEnd;
    const char *begin = NULL, *end = NULL;
    std::string tempString;
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
    SUBTITLE_LOGI("%s bufSize:%d buf:%s ", __FUNCTION__, bufSize, buf);

    tinyxml2::XMLDocument doc;
    int ret = doc.Parse(buf, bufSize);
    if (ret != tinyxml2::XML_SUCCESS) {
        SUBTITLE_LOGI("%s ret:%d Failed to parse XML", __FUNCTION__, ret);
        return -1;
    }
    tinyxml2::XMLElement* root = doc.RootElement();
    if (root == NULL) {
        SUBTITLE_LOGI("%s Failed to load file: No root element.", __FUNCTION__);
        doc.Clear();
        return -1;
    }

    // TODO: parse header, if we support style metadata and layout property
    // parse head
    tinyxml2::XMLElement *head = root->FirstChildElement("head");
    if (head == nullptr) head = root->FirstChildElement("tt:head");
    if (head == nullptr) {
        SUBTITLE_LOGI("%s Error. no head found!", __FUNCTION__);
        doc.Clear();
        return -1;
    }
    tinyxml2::XMLElement *headLayout = head->FirstChildElement("layout");
    if (headLayout == nullptr) headLayout = head->FirstChildElement("tt:layout");
    if (headLayout == nullptr) {
        SUBTITLE_LOGI("%s Error. no head layout found!", __FUNCTION__);
        doc.Clear();
        return -1;
    }


    tinyxml2::XMLElement *headLayoutRegion = headLayout->FirstChildElement("region");
    if (headLayoutRegion == nullptr) headLayoutRegion = headLayout->FirstChildElement("tt:region");
    if (headLayoutRegion == nullptr) {
        SUBTITLE_LOGI("%s Error. no head layout region found!", __FUNCTION__);
        doc.Clear();
        return -1;
    }

    // TODO: parse header, if we support style metadata and layout property
    // parse body
    tinyxml2::XMLElement *body = root->FirstChildElement("body");
    if (body == nullptr) body = root->FirstChildElement("tt:body");
    if (body == nullptr) {
        SUBTITLE_LOGI("%s Error. no body found!", __FUNCTION__);
        doc.Clear();
        return -1;
    }

    tinyxml2::XMLElement *bodyDiv = body->FirstChildElement("div");
    if (bodyDiv == nullptr) bodyDiv = body->FirstChildElement("tt:div");
    if (bodyDiv == nullptr) {
        SUBTITLE_LOGI("%s Error. no body div found!", __FUNCTION__);
        doc.Clear();
        return -1;
    }

    tinyxml2::XMLElement *bodyDivP = bodyDiv->FirstChildElement("p");
    if (bodyDivP == nullptr) bodyDivP = bodyDiv->FirstChildElement("tt:p");
    if (bodyDivP == nullptr) {
        SUBTITLE_LOGI("%s Error. no body div p found!", __FUNCTION__);
        doc.Clear();
        return -1;
    }

    tinyxml2::XMLElement *divSpanParagraph;
    if (bodyDivP != nullptr) {
        const tinyxml2::XMLAttribute *regionXmlIdAttribute = headLayoutRegion->FindAttribute("xml:id");
        const tinyxml2::XMLAttribute *regionTtsExtentAttribute = headLayoutRegion->FindAttribute("tts:extent");
        const tinyxml2::XMLAttribute *regionTtsOriginAttribute = headLayoutRegion->FindAttribute("tts:origin");
        const tinyxml2::XMLAttribute *regionTtsPaddingAttribute = headLayoutRegion->FindAttribute("tts:padding");
        const tinyxml2::XMLAttribute *regionTtsDisplayAlignAttribute = headLayoutRegion->FindAttribute("tts:displayAlign");
        const tinyxml2::XMLAttribute *regionTtsWritingModeAttribute = headLayoutRegion->FindAttribute("tts:writingMode");

        const tinyxml2::XMLAttribute *divXmlIdAttribute = bodyDiv->FindAttribute("xml:id");
        const tinyxml2::XMLAttribute *divStyleAttribute = bodyDiv->FindAttribute("style");

        const tinyxml2::XMLAttribute *divPBeginAttribute = bodyDivP->FindAttribute("begin");
        const tinyxml2::XMLAttribute *divPEndAttribute = bodyDivP->FindAttribute("end");
        const tinyxml2::XMLAttribute *divPStyleAttribute = bodyDivP->FindAttribute("style");
        const tinyxml2::XMLAttribute *divPRegionAttribute = bodyDivP->FindAttribute("region");
        const tinyxml2::XMLAttribute *divPXmlIdAttribute = bodyDivP->FindAttribute("xml:id");


        std::string tempString;
        tinyxml2::XMLElement *divPSpan = bodyDivP->FirstChildElement("span");
        if (divPSpan == nullptr) divPSpan = bodyDivP->FirstChildElement("tt:span");
        if (divPSpan == nullptr) {
            SUBTITLE_LOGI("%s Error. no divPSpan found!", __FUNCTION__);
            doc.Clear();
            return -1;
        }

        tinyxml2::XMLNode *childNodeBodyDivP = bodyDivP->FirstChild();
        while (divPSpan != nullptr) {
            const tinyxml2::XMLAttribute *divPSpanStyleAttribute = divPSpan->FindAttribute("style");
            SUBTITLE_LOGI("%s style=%s text:%s", __FUNCTION__, divPSpanStyleAttribute->Value(), divPSpan->GetText());
            if (childNodeBodyDivP && (strcmp((childNodeBodyDivP->ToElement())->Value(), "tt:br") == 0)) {
                tempString = tempString + "\n" + std::string(divPSpan->GetText());
            } else {
                tempString = tempString + std::string(divPSpan->GetText());
            }
            SUBTITLE_LOGI("%s tempString:%s", __FUNCTION__, tempString.c_str());
            divPSpan = divPSpan->NextSiblingElement("tt:span");
            childNodeBodyDivP = childNodeBodyDivP->NextSibling();
        }

        SUBTITLE_LOGI("%s Region: xml:id=%s tts:extent=%s tts:origin=%s tts:padding=%s tts:displayAlign=%s tts:writingMode=%s Div: xml:id=%s style=%s  ttp: begin=%s end=%s style=%s region=%s xml:id=%s  text=%s", __FUNCTION__,
                regionXmlIdAttribute->Value(),
                regionTtsExtentAttribute->Value(),
                regionTtsOriginAttribute->Value(),
                regionTtsPaddingAttribute->Value(),
                regionTtsDisplayAlignAttribute->Value(),
                regionTtsWritingModeAttribute->Value(),
                divXmlIdAttribute->Value(),
                divStyleAttribute->Value(),
                divPBeginAttribute->Value(),
                divPEndAttribute->Value(),
                divPStyleAttribute->Value(),
                divPRegionAttribute->Value(),
                divPXmlIdAttribute->Value(),
                tempString.c_str());

        spu->m_delay = spu->pts + ttml_parse_timecode(divPBeginAttribute->Value(), divPEndAttribute->Value()) * 90;
        spu->spu_data = (unsigned char *)malloc((tempString.length()+1) * sizeof(char));
        memset(spu->spu_data, 0, tempString.length() * sizeof(char));
        memcpy(spu->spu_data, reinterpret_cast<unsigned char*>(const_cast<char*>(tempString.c_str())), tempString.length() * sizeof(char));
        spu->spu_data[tempString.length()] = '\0';
        bodyDivP = bodyDivP->NextSiblingElement("tt:p");
        addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
        // spu->pts = spu->m_delay;
    }

    doc.Clear();
    return 0;
}

int TtmlParser::getTTMLSpu() {
    char tmpbuf[8] = {0};
    int64_t packetHeader = 0;

    if (mDumpSub) SUBTITLE_LOGI("enter get_dvb_ttml_spu\n");
    int ret = -1;

    while (mDataSource->read(tmpbuf, 1) == 1) {
        if (mState == SUB_STOP) {
            return 0;
        }

        std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());
        spu->sync_bytes = AML_PARSER_SYNC_WORD;

        packetHeader = ((packetHeader<<8) & 0x000000ffffffffff) | tmpbuf[0];
        if (mDumpSub) SUBTITLE_LOGI("## get_dvb_spu %x, %llx,-------------\n",tmpbuf[0], packetHeader);

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
                SUBTITLE_LOGI("get_dvb_ttml spu-packetLen:%d, pesHeaderLen:%d\n", packetLen,pesHeaderLen);

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
                SUBTITLE_LOGE("illegal packetLen!!!\n");
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
                SUBTITLE_LOGE("dvb packet is too big\n\n");
                return -1;
            }
            spu->subtitle_type = TYPE_SUBTITLE_DVB_TTML;
            spu->pts = dvbPts;
            if (isMore32Bit(spu->pts) && !isMore32Bit(mDataSource->getSyncTime())) {
                SUBTITLE_LOGI("SUB PTS is greater than 32 bits, before subpts: %lld, vpts:%lld", spu->pts, mDataSource->getSyncTime());
                spu->pts &= TSYNC_32_BIT_PTS;
            }
            //If gap time is large than 9 secs, think pts skip,notify info
            if (abs(mDataSource->getSyncTime() - spu->pts) > 9*90000) {
                SUBTITLE_LOGE("[%s::%d]pts skip,vpts:%lld, spu->pts:%lld notify time error!\n", __FUNCTION__, __LINE__, mDataSource->getSyncTime(), spu->pts);
            }

            SUBTITLE_LOGI("[%s::%d]pts ---------------,vpts:%lld, spu->pts:%lld\n", __FUNCTION__, __LINE__, mDataSource->getSyncTime(), spu->pts);

            buf = (char *)malloc(packetLen);
            if (buf) {
                SUBTITLE_LOGI("packetLen is %d, pts is %llx, delay is %llx\n", packetLen, spu->pts, tempPts);
            } else {
                SUBTITLE_LOGI("packetLen buf malloc fail!!!\n");
            }

            if (buf) {
                memset(buf, 0x0, packetLen);
                if (mDataSource->read(buf, packetLen) == packetLen) {
                    ret = TtmlDecodeFrame(spu, buf, packetLen);
                    SUBTITLE_LOGI(" @@@@@@@hwDemuxParse parse ret:%d", ret);
                    if (ret != -1) {
                        SUBTITLE_LOGI("dump-pts-hwdmx!success.\n");
                        if (buf) free(buf);
                        return 0;
                    } else {
                        SUBTITLE_LOGI("dump-pts-hwdmx!error.\n");
                        if (buf) free(buf);
                        return -1;
                    }
                }
                SUBTITLE_LOGI("packetLen buf free=%p\n", buf);
                SUBTITLE_LOGI("@@[%s::%d]free ptr=%p\n", __FUNCTION__, __LINE__, buf);
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
        if (mDumpSub) SUBTITLE_LOGI("## %s get_dvb_spu %x,%x,%x,  %x,%x,%x,  %x,%x,-------------\n",
                __FUNCTION__,
                tmpbuf[0], tmpbuf[1], tmpbuf[2], tmpbuf[3],
                tmpbuf[4], tmpbuf[5], tmpbuf[6], tmpbuf[7]);

        dataLen = subPeekAsInt32(tmpbuf + 3);
        dvbPts  = subPeekAsInt64(tmpbuf + 7);
        ptsDiff = subPeekAsInt32(tmpbuf + 15);

        spu->subtitle_type = TYPE_SUBTITLE_DVB_TTML;
        spu->pts = dvbPts;
        if (mDumpSub) SUBTITLE_LOGI("## %s spu-> pts:%lld,dvPts:%lld\n", __FUNCTION__, spu->pts, dvbPts);
        if (mDumpSub) SUBTITLE_LOGI("## %s datalen=%d,pts=%llx,delay=%llx,diff=%llx, data: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
                __FUNCTION__, dataLen, dvbPts, spu->m_delay, ptsDiff,
                tmpbuf[0], tmpbuf[1], tmpbuf[2], tmpbuf[3], tmpbuf[4],
                tmpbuf[5], tmpbuf[6], tmpbuf[7], tmpbuf[8], tmpbuf[9],
                tmpbuf[10], tmpbuf[11], tmpbuf[12], tmpbuf[13], tmpbuf[14]);

        data = (char *)malloc(dataLen);
        if (!data) {
            SUBTITLE_LOGE("[%s::%d]malloc error! \n", __FUNCTION__,__LINE__);
            return -1;
        }
        if (mDumpSub) SUBTITLE_LOGI("@@[%s::%d]malloc ptr=%p, size = %d\n",__FUNCTION__, __LINE__, data, dataLen);
        memset(data, 0x0, dataLen);
        ret = mDataSource->read(data, dataLen);
        if (mDumpSub) SUBTITLE_LOGI("## ret=%d,dataLen=%d, %x,%x,%x,%x,%x,%x,%x,%x,---------\n",
                ret, dataLen, data[0], data[1], data[2], data[3],
                data[4], data[5], data[6], data[7]);

        ret = TtmlDecodeFrame(spu, data, dataLen);
        if (ret != -1) {
            if (mDumpSub) SUBTITLE_LOGI("dump-pts-swdmx!success pts(%lld) mIndex:%d frame was add\n", spu->pts, mIndex);
            {
                std::unique_lock<std::mutex> autolock(mMutex);
                mPendingAction = 1;
                mCv.notify_all();
            }
        } else {
            if (mDumpSub) SUBTITLE_LOGI("dump-pts-swdmx!error this pts(%lld) frame was abandon\n", spu->pts);
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
            spu->subtitle_type = TYPE_SUBTITLE_DVB_TTML;
            data = (char *)malloc(ATV_TTML_DATA_LEN);
            if (!data) {
                SUBTITLE_LOGE("[%s::%d]malloc error! \n", __FUNCTION__,__LINE__);
                return -1;
            }
            memset(data, 0x0, ATV_TTML_DATA_LEN);
            ret = mDataSource->read(data, ATV_TTML_DATA_LEN);
            if (mDumpSub) SUBTITLE_LOGI("[%s::%d] ret=%d,dataLen=%d, %x,%x,%x,%x,%x,%x,%x,%x,---------\n",
                    __FUNCTION__, __LINE__, ret, ATV_TTML_DATA_LEN, data[0], data[1], data[2], data[3],
                    data[4], data[5], data[6], data[7]);

            ret = TtmlDecodeFrame(spu, data, ATV_TTML_DATA_LEN);

            if (ret != -1 && spu->buffer_size > 0) {
                if (mDumpSub) SUBTITLE_LOGI("[%s::%d]dump-pts-atvHwDmx!success pts(%lld) mIndex:%d frame was add\n", __FUNCTION__,__LINE__, spu->pts, ++mIndex);
            } else {
                if (mDumpSub) SUBTITLE_LOGI("[%s::%d]dump-pts-atvHwDmx!error this pts(%lld) frame was abandon\n", __FUNCTION__,__LINE__, spu->pts);
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
        SUBTITLE_LOGI(" mState == SUB_STOP \n\n");
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
