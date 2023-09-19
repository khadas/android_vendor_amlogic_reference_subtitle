/*
 * AribB24 decoding for ffmpeg
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
#define  LOG_TAG "AribB24Parser"
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

#include "AribB24Parser.h"

#define  TRACE()    LOGI("[%s::%d]\n",__FUNCTION__,__LINE__)

#define LOGI ALOGI
#define LOGD ALOGD
#define LOGE ALOGE

#define MAX_BUFFERED_PAGES 25
#define SCALE_FACTOR       2
#define STROKE_WIDTH       3
#define FORCE_STROKE_TEXT  1

#define OSD_HALF_SIZE (1920*1280/8)

#define HIGH_32_BIT_PTS 0xFFFFFFFF
#define TSYNC_32_BIT_PTS 0xFFFFFFFF

#define CLOCK_FREQ INT64_C(1000000)
#if (CLOCK_FREQ % 1000) == 0
#define ARIB_B24_TICK_FROM_MS(ms)  ((CLOCK_FREQ / INT64_C(1000)) * (ms))
#elif (1000 % CLOCK_FREQ) == 0
#define ARIB_B24_TICK_FROM_MS(ms)  ((ms)  / (INT64_C(1000) / CLOCK_FREQ))
#else /* rounded overflowing conversion */
#define ARIB_B24_TICK_FROM_MS(ms)  (CLOCK_FREQ * (ms) / 1000)
#endif /* CLOCK_FREQ / 1000 */

bool static inline isMore32Bit(int64_t pts) {
    if (((pts >> 32) & HIGH_32_BIT_PTS) > 0) {
        return true;
    }
    return false;
}

std::string colorToString(const aribcaption::ColorRGBA& color) {
    return "rgba("+std::to_string(color.r) + ", " +
           std::to_string(color.g) + ", " +
           std::to_string(color.b) + ", " +
           std::to_string(color.a) + ")";
}

AribB24Parser::AribB24Parser(std::shared_ptr<DataSource> source) : aribcaptionDecoder(aribcaptionContext){
    mDataSource = source;
    mParseType = TYPE_SUBTITLE_ARIB_B24;
    mIndex = 0;
    mDumpSub = 0;
    mPendingAction = -1;
    mTimeoutThread = std::shared_ptr<std::thread>(new std::thread(&AribB24Parser::callbackProcess, this));

    initContext();
    checkDebug();
    sInstance = this;
}

AribB24Parser *AribB24Parser::sInstance = nullptr;

AribB24Parser *AribB24Parser::getCurrentInstance() {
    return AribB24Parser::sInstance;
}

AribB24Parser::~AribB24Parser() {
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
    AribB24Parser *parser = AribB24Parser::getCurrentInstance();

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
 *  AribB24 has interaction, with event handling
 *  This function main for this. the control interface.not called in parser thread, need protect.
 */
bool AribB24Parser::updateParameter(int type, void *data) {
    if (TYPE_SUBTITLE_ARIB_B24 == type) {
        DtvKitArib24Param *pAribParam = (DtvKitArib24Param* )data;
        mContext->languageCode = pAribParam->languageCodeId;
    }
    return true;
}

int AribB24Parser::initContext() {
    std::unique_lock<std::mutex> autolock(mMutex);
    aribcaptionDecoder.Initialize();
    mContext = new AribB24Context();
    if (!mContext) {
        LOGE("[%s::%d]malloc error! \n", __FUNCTION__, __LINE__);
    }
    return ARIB_B24_SUCCESS;
}

void AribB24Parser::checkDebug() {
    char value[PROPERTY_VALUE_MAX] = {0};
    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("vendor.subtitle.dump", value, "false");
    if (!strcmp(value, "true")) {
        mDumpSub = true;
    }
}

int AribB24Parser::aribB24DecodeFrame(std::shared_ptr<AML_SPUVAR> spu, char *srcData, int srcLen) {
    const uint8_t *buf = (uint8_t *)srcData;
    int bufSize = srcLen;
    const uint8_t *p, *pEnd;
    std::string stringJson;
    int width = 0;
    int height = 0;

    spu->spu_width = 0;
    spu->spu_height = 0;
    spu->spu_start_x = 0;
    spu->spu_start_y = 0;
    //default spu display in windows width and height
    //Since the underlying rendering of arib b24 subtitles is a picture,
    //font files need to be built in the vendor partition,
    //so first comment out the relevant code and give up the picture rendering
    //spu->spu_origin_display_w = 720;
    //spu->spu_origin_display_h = 576;
    spu->isSimpleText = true;
    if (mDumpSub) {
        for (int i = 0; i < bufSize; i++) {
            LOGE("0x%x ", buf[i]);
        }
    }
    if (mDumpSub) {
        LOGI("%s languageCode:%d", __FUNCTION__, mContext->languageCode);
    }

    switch (mContext->languageCode) {
        case ARIB_B24_POR:
            aribcaptionDecoder.SetEncodingScheme(aribcaption::EncodingScheme::kABNT_NBR_15606_1_Latin);
        break;
        case ARIB_B24_JPN:
            aribcaptionDecoder.SetEncodingScheme(aribcaption::EncodingScheme::kARIB_STD_B24_JIS);
        break;
        case ARIB_B24_SPA:
            aribcaptionDecoder.SetEncodingScheme(aribcaption::EncodingScheme::kABNT_NBR_15606_1_Latin);
        break;
        case ARIB_B24_ENG:
            aribcaptionDecoder.SetEncodingScheme(aribcaption::EncodingScheme::kARIB_STD_B24_UTF8);
        break;
        case ARIB_B24_TGL:
            aribcaptionDecoder.SetEncodingScheme(aribcaption::EncodingScheme::kARIB_STD_B24_UTF8);
        break;
        default:
        break;
    }

    auto status = aribcaptionDecoder.Decode(buf, bufSize, spu->pts, aribcaptionResult);
    if (status == aribcaption::DecodeStatus::kError) {
        LOGE("%s Decoder::Decode() returned error", __FUNCTION__);
        return -1;
    } else if (status == aribcaption::DecodeStatus::kNoCaption) {
        LOGE("%s kDecodeStatusNoCaption", __FUNCTION__);
        return 0;
    } else if (status == aribcaption::DecodeStatus::kGotCaption) {
        #ifdef NEED_ARIB_CLOSED_CAPTION_STYLE
        stringJson = "{\"type\":\"aribCaption\",\"plane_width\":" + std::to_string(aribcaptionResult.caption->plane_width) +
                                    ",\"plane_height\":" + std::to_string(aribcaptionResult.caption->plane_height)+
                                    ",\"swf\":" + std::to_string(aribcaptionResult.caption->swf)+", \"chars\":[";
        /**
         * Caption't Writing Format
         *
         * *Decimal numbers specifying format are as follows.
         * *0: horizontal writing form in standard density
         * *1: vertical writing form in standard density
         * *2: horizontal writing form in high density
         * *3: vertical writing form in high density
         * *4: horizontal writing form in Western language
         * *5: horizontal writing form in   1920 x 1080
         * *6: vertical writing form in 1920 x 1080
         * *7: horizontal writing form in 960 x 540
         * *8: vertical writing form in 960 x 540
         * *9: horizontal writing form in 720 x 480
         * *10: vertical writing form in 720 x 480
         * *11: horizontal writing form in 1280 x 720
         * *12: vertical writing form in 1280 x 720
         */
        for (size_t i = 0; i < aribcaptionResult.caption->regions.size(); i++) {
            aribcaption::CaptionRegion& region = aribcaptionResult.caption->regions[i];
            if (i != 0) stringJson = stringJson +",";
            stringJson = stringJson +"{\"new_line\":"+std::to_string(i)+"},";
            int j = 0;
            for (aribcaption::CaptionChar& ch : region.chars) {
                // background
                aribcaption::CharStyle style = ch.style;
                aribcaption::ColorRGBA stroke_color = ch.stroke_color;
                aribcaption::EnclosureStyle enclosure_style = ch.enclosure_style;
                int stroke_width = 0;

                if (style & aribcaption::CharStyle::kCharStyleStroke) {
                    stroke_width = STROKE_WIDTH;
                }

                if (ch.type == aribcaption::CaptionCharType::kDRCS
                    || ch.type == aribcaption::CaptionCharType::kDRCSReplaced) {
                    aribcaption::DRCS& drcs = aribcaptionResult.caption->drcs_map[ch.drcs_code];
                    LOGE("%s CaptionCharType::kDRCS or CaptionCharType::kDRCSReplaced, haven't dealt with this yet.", __FUNCTION__);
                }

                if (mDumpSub) {
                    LOGI("%s x:%d y:%d char_width:%d char_height:%d char_horizontal_spacing:%d char_vertical_spacing:%d ch.char_horizontal_scale:%f ch.char_vertical_scale:%f text_color:0x%x back_color:0x%x stroke_color:0x%x stroke_width:%d style:%d enclosure_style:%d u8str:%s char_flash:%d char_repeat_display_times:%d section_width:%d section_height:%d codepoint:0x%x pua_codepoint:0x%x drcs_code:0x%x",
                        __FUNCTION__, ch.x, ch.y, ch.char_width, ch.char_height, ch.char_horizontal_spacing, ch.char_vertical_spacing, ch.char_horizontal_scale, ch.char_vertical_scale,
                            ch.text_color, ch.back_color, stroke_color, stroke_width, style, enclosure_style, ch.u8str,
                            ch.char_flash, ch.char_repeat_display_times, ch.section_width(), ch.section_height(), ch.codepoint, ch.pua_codepoint, ch.drcs_code);
                }
                if (j != 0) {
                    stringJson = stringJson + ",";
                }
                stringJson = stringJson + "{\"x\":"+std::to_string(ch.x)+
                                                ",\"y\":"+std::to_string(ch.y)+
                                                ",\"char_width\":"+std::to_string(ch.char_width)+
                                                ",\"char_height\":"+std::to_string(ch.char_height)+
                                                ",\"char_horizontal_spacing\":"+std::to_string(ch.char_horizontal_spacing)+
                                                ",\"char_vertical_spacing\":"+std::to_string(ch.char_vertical_spacing)+
                                                ",\"char_horizontal_scale\":"+std::to_string(ch.char_horizontal_scale)+
                                                ",\"char_vertical_scale\":"+std::to_string(ch.char_vertical_scale)+
                                                ",\"text_color\":\""+colorToString(ch.text_color)+"\""+
                                                ",\"back_color\":\""+colorToString(ch.back_color)+"\""+
                                                ",\"stroke_color\":\""+colorToString(stroke_color)+"\""+
                                                ",\"stroke_width\":"+std::to_string(stroke_width)+
                                                ",\"char_style\":"+std::to_string(style)+
                                                ",\"enclosure_style\":"+std::to_string(enclosure_style)+
                                                ",\"u8str\":\""+ch.u8str+"\""+
                                                ",\"char_flash\":"+std::to_string(ch.char_flash)+
                                                ",\"char_repeat_display_times\":"+std::to_string(ch.char_repeat_display_times)+
                                                ",\"section_width\":"+std::to_string(ch.section_width())+
                                                ",\"section_height\":"+std::to_string(ch.section_height())+
                                                ",\"codepoint\":"+std::to_string(ch.codepoint)+
                                                ",\"pua_codepoint\":"+std::to_string(ch.pua_codepoint)+
                                                ",\"drcs_code\":"+std::to_string(ch.drcs_code)+
                                                "}";
                j++;

            }
        }
        stringJson = stringJson + "]}";
        if (mDumpSub && stringJson.length() > 0) {
            int startPos = 0;
            int chunkSize = 1000;
            int remaining = stringJson.length();
            while (remaining > 0) {
                std::string chunk = stringJson.substr(startPos, chunkSize);
                LOGI("%s startPos:%d stringJson:%s", __FUNCTION__, startPos, chunk.c_str());
                startPos += chunkSize;
                remaining -= chunk.size();
            }
        }

        #else
        stringJson = aribcaptionResult.caption->text;
        LOGD(" %s aribcc_decoder_decode() stringJson:%s\n", __FUNCTION__, stringJson.c_str());
        // Remove leading spaces
        stringJson.erase(stringJson.begin(), std::find_if(stringJson.begin(), stringJson.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));

        // Remove trailing spaces
        stringJson.erase(std::find_if(stringJson.rbegin(), stringJson.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), stringJson.end());
        #endif

        spu->buffer_size = stringJson.length();
        if (aribcaptionResult.caption->wait_duration == ARIBCC_DURATION_INDEFINITE) {
            //p_spu->b_ephemer = true;
            spu->isImmediatePresent = true;
        } else {
            spu->m_delay = spu->pts + ARIB_B24_TICK_FROM_MS(aribcaptionResult.caption->wait_duration);
        }
        if (spu->spu_data) free(spu->spu_data);
        spu->spu_data = (unsigned char *)malloc(spu->buffer_size+1);
        if (!spu->spu_data) {
            LOGE("[%s::%d] malloc error!", __FUNCTION__, __LINE__);
            return ARIB_B24_FAILURE;
        }
        memset(spu->spu_data, 0, stringJson.length()+1);
        memcpy(spu->spu_data, stringJson.c_str(), stringJson.length());

    }
    return ARIB_B24_SUCCESS;

}

int AribB24Parser::getAribB24Spu() {
    char tmpbuf[8] = {0};
    int64_t packetHeader = 0;

    if (mDumpSub) LOGI("enter get_dvb_arib b24_spu\n");
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
            && ((packetHeader & 0xff)== 0x41)) {//AMLUA  ATV Arib B24
            ret = atvHwDemuxParse(spu);
        }
    }

    return ret;
}

int AribB24Parser::hwDemuxParse(std::shared_ptr<AML_SPUVAR> spu) {
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
                LOGI("get_dvb_arib b24_spu-packetLen:%d, pesHeaderLen:%d\n", packetLen,pesHeaderLen);

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
            spu->subtitle_type = TYPE_SUBTITLE_ARIB_B24;
            spu->pts = dvbPts;
            if (isMore32Bit(spu->pts) && !isMore32Bit(mDataSource->getSyncTime())) {
                ALOGD("SUB PTS is greater than 32 bits, before subpts: %lld, vpts:%lld", spu->pts, mDataSource->getSyncTime());
                spu->pts &= TSYNC_32_BIT_PTS;
            }
            //If gap time is large than 9 secs, think pts skip,notify info
            if (abs(mDataSource->getSyncTime() - spu->pts) > 9*90000) {
                LOGE("[%s::%d]pts skip, notify time error!\n", __FUNCTION__, __LINE__);
            }
            buf = (char *)malloc(packetLen);
            if (buf) {
                LOGI("packetLen is %d, pts is %llx, delay is %llx\n", packetLen, spu->pts, tempPts);
            } else {
                LOGI("packetLen buf malloc fail!!!\n");
            }

            if (buf) {
                memset(buf, 0x0, packetLen);
                if (mDataSource->read(buf, packetLen) == packetLen) {
                    ret = aribB24DecodeFrame(spu, buf, packetLen);
                    LOGI(" @@@@@@@hwDemuxParse parse ret:%d, buffer_size:%d", ret, spu->buffer_size);
                    if (ret != -1 && spu->buffer_size > 0) {
                        LOGI("dump-pts-hwdmx!success pts(%lld) %d frame was add\n", spu->pts, mIndex);
                        LOGI(" addDecodedItem buffer_size=%d",
                                spu->buffer_size);
                        addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
                        {   // for signal notification.
                            std::unique_lock<std::mutex> autolock(mMutex);
                            mPendingAction = 1;
                            mCv.notify_all();
                        }
                        if (buf) free(buf);
                        return 0;
                    } else {
                        LOGI("dump-pts-hwdmx!error pts(%lld) frame was abandon ret=%d bufsize=%d\n", spu->pts, ret, spu->buffer_size);
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

int AribB24Parser::softDemuxParse(std::shared_ptr<AML_SPUVAR> spu) {
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

        spu->subtitle_type = TYPE_SUBTITLE_ARIB_B24;
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

        ret = aribB24DecodeFrame(spu, data, dataLen);
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

int AribB24Parser::atvHwDemuxParse(std::shared_ptr<AML_SPUVAR> spu) {
        char tmpbuf[256] = {0};
        int64_t dvbPts = 0, ptsDiff = 0;
        int ret = 0;
        int lineNum = 0;
        char *data = NULL;

        // read package header info
        if (mDataSource->read(tmpbuf, 4) == 4) {
            spu->subtitle_type = TYPE_SUBTITLE_ARIB_B24;
            data = (char *)malloc(ATV_ARIB_B24_DATA_LEN);
            if (!data) {
                LOGE("[%s::%d]malloc error! \n", __FUNCTION__,__LINE__);
                return -1;
            }
            memset(data, 0x0, ATV_ARIB_B24_DATA_LEN);
            ret = mDataSource->read(data, ATV_ARIB_B24_DATA_LEN);
            if (mDumpSub) LOGI("[%s::%d] ret=%d,dataLen=%d, %x,%x,%x,%x,%x,%x,%x,%x,---------\n",
                    __FUNCTION__, __LINE__, ret, ATV_ARIB_B24_DATA_LEN, data[0], data[1], data[2], data[3],
                    data[4], data[5], data[6], data[7]);

            ret = aribB24DecodeFrame(spu, data, ATV_ARIB_B24_DATA_LEN);

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

void AribB24Parser::callbackProcess() {
    int timeout = property_get_int32("vendor.sys.subtitleService.decodeTimeOut", 60);
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

int AribB24Parser::getSpu() {
    if (mState == SUB_INIT) {
        mState = SUB_PLAYING;
    } else if (mState == SUB_STOP) {
        ALOGD(" mState == SUB_STOP \n\n");
        return 0;
    }

    return getAribB24Spu();
}

int AribB24Parser::getInterSpu() {
    return getSpu();
}

int AribB24Parser::parse() {
    while (!mThreadExitRequested) {
        if (getInterSpu() < 0) {
            // advance input, if parse failed, wait and retry.
            //std::this_thread::sleep_for(std::chrono::milliseconds(1));
            usleep(100);
        }
    }
    return 0;
}

void AribB24Parser::dump(int fd, const char *prefix) {
    //TODO: dump run in binder thread, may need add lock!
    dprintf(fd, "%s AribB24 Parser\n", prefix);
    dumpCommon(fd, prefix);

    if (mContext != nullptr) {
        dprintf(fd, "%s  formatId: %d (0 = bitmap, 1 = text/ass)\n", prefix, mContext->formatId);
        dprintf(fd, "%s  pts:%lld\n", prefix, mContext->pts);
        dprintf(fd, "\n");
    }
}
