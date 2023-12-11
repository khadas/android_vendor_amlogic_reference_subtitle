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

#define  LOG_TAG "SmpteTtmlParser"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include "bprint.h"
#include "SubtitleLog.h"
#include <utils/CallStack.h>

#include <unistd.h>
#include <fcntl.h>
#include <list>
#include <thread>
#include <algorithm>
#include <functional>
#include <png.h>

#include "ParserFactory.h"
#include "StreamUtils.h"
#include "VideoInfo.h"

#include "SubtitleLog.h"

#include "SmpteTtmlParser.h"
#include "tinyxml2.h"

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

// Save binary data as a file
bool save2PngFile(const char* filename, const void* data, size_t dataSize) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        SUBTITLE_LOGE("Error cannot open file %s!", filename);
        return false;
    }

    // write binary data to file
    if (fwrite(data, 1, dataSize, fp) != dataSize) {
        SUBTITLE_LOGE("Error: Failed to write data to file:%s!", filename);
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

/* EBU-TT-D start position X, start position Y, width and height */
static std::vector<int> extractNumbers(const char* str) {
    std::vector<int> numbers;
    const char* start = str;

    while (*start) {
        // Find the starting position of the next number
        while (*start && !std::isdigit(*start)) {
            ++start;
        }
        // exit the loop if there are no more numbers
        if (!*start) {
            break;
        }
        // extract the number part and add it to the container
        int number = std::atoi(start);
        numbers.push_back(number);
        // find the end position of the number
        while (*start && std::isdigit(*start)) {
            ++start;
        }
    }
    return numbers;
}

/* EBU-TT-D timecodes have format hours:minutes:seconds[.fraction] */
static int smpte_ttml_parse_timecode(const char * beginTimeString, const char * endTimeString)
{
  int beginHours = 0, beginMinutes = 0, beginSeconds = 0, beginMilliseconds = 0,
            endHours = 0, endMinutes = 0, endSeconds = 0, endMilliseconds = 0,
            time = 0;

  SUBTITLE_LOGI("%s begin time string: %s, end time string: %s\n",__FUNCTION__, beginTimeString, endTimeString);
  bool beginSuccess = sscanf(beginTimeString, "%d:%d:%d.%d", &beginHours, &beginMinutes, &beginSeconds, &beginMilliseconds) == 4;
  bool endSuccess = sscanf(endTimeString, "%d:%d:%d.%d", &endHours, &endMinutes, &endSeconds, &endMilliseconds) == 4;
  if (!beginSuccess || beginHours < 0 || beginHours > 23 || beginMinutes < 0 || beginMinutes > 59 || beginSeconds < 0 || beginSeconds > 59 || beginMilliseconds < 0 || beginMilliseconds > 999) {
    SUBTITLE_LOGE ("%s invalid begin time:%s.", __FUNCTION__, beginTimeString);
  }

  if (!endSuccess || endHours < 0 || endHours > 23 || endMinutes < 0 || endMinutes > 59 || endSeconds < 0 || endSeconds > 59 || endMilliseconds < 0 || endMilliseconds > 999) {
    SUBTITLE_LOGE ("%s invalid end time:%s.", __FUNCTION__, endTimeString);
  }

  time = ((endHours - beginHours) * 3600 + (endMinutes - beginMinutes) * 60 + (endSeconds - beginSeconds) ) * 1000+ (endMilliseconds - beginMilliseconds);
  return time;
}

std::vector<unsigned char> base64Decode(const char* input) {
    std::vector<unsigned char> decodedData;
    int val = 0, valb = -8, len = strlen(input);

    for (int i = 0; i < len; i++) {
        char c = input[i];
        int v = (c >= 'A' && c <= 'Z') ? c - 'A' :
                (c >= 'a' && c <= 'z') ? c - 'a' + 26 :
                (c >= '0' && c <= '9') ? c - '0' + 52 :
                (c == '+') ? 62 :
                (c == '/') ? 63 : 0;

        val = (val << 6) + v;
        valb += 6;

        if (valb >= 0) {
            unsigned char d = static_cast<unsigned char>((val >> valb) & 0xFF);
            decodedData.push_back(d);
            valb -= 8;
        }
    }
    return decodedData;
}

typedef struct {
    const char* data;
    size_t size;
    size_t offset;
} CustomReadData;

void custom_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    CustomReadData* read_data = (CustomReadData*)png_get_io_ptr(png_ptr);
    if (read_data->offset + length > read_data->size) {
        png_error(png_ptr, "Read error: reached end of data");
        return;
    }
    memcpy(data, read_data->data + read_data->offset, length);
    read_data->offset += length;
}

void decodePng(std::shared_ptr<AML_SPUVAR> spu, const char* png_data, size_t png_size) {
    CustomReadData read_data = {png_data, png_size, 0 };
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep *row_pointers;

    // Create a PNG read structure
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        SUBTITLE_LOGE("%s Error: png_create_read_struct failed.", __FUNCTION__);
        return;
    }
    SUBTITLE_LOGE("%s start 2", __FUNCTION__);
    // Create an information structure
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        SUBTITLE_LOGE("%s Error: png_create_info_struct failed.", __FUNCTION__);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return;
    }

    // set error handler
    if (setjmp(png_jmpbuf(png_ptr))) {
        SUBTITLE_LOGE("%s Error: Error during png read.", __FUNCTION__);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return;
    }

    // Initialize the png reading process
    png_set_read_fn(png_ptr, &read_data, custom_read_data);

    // Start reading PNG data
    png_read_info(png_ptr, info_ptr);

    int width = png_get_image_width(png_ptr, info_ptr);
    int height = png_get_image_height(png_ptr, info_ptr);
    int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    int color_type = png_get_color_type(png_ptr, info_ptr);

    // Some simple output, you can do other processing as needed
    SUBTITLE_LOGI("%s Image Width: %d\n", __FUNCTION__ , width);
    SUBTITLE_LOGI("%s Image Height: %d\n", __FUNCTION__ , height);
    SUBTITLE_LOGI("%s Bit Depth: %d\n", __FUNCTION__ , bit_depth);
    SUBTITLE_LOGI("%s Color Type: %d\n", __FUNCTION__ , color_type);

    // convert to RGB image (if not)
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    if (bit_depth == 16)
        png_set_strip_16(png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    // Update info_ptr, read all the information of the image
    png_read_update_info(png_ptr, info_ptr);

    // Allocate memory for image data and read image data
    row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png_ptr, info_ptr));
    }

    png_read_image(png_ptr, row_pointers);

    // At this point, row_pointers contains the image data.
    // Allocate memory for the final data buffer
    spu->buffer_size = width * height * 4;
    spu->spu_data = (unsigned char*)malloc(width * height * 4);
    int index = 0;

    // Copy image data from row_pointers to the image_data buffer
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width * 4; x++) {
            spu->spu_data[index++] = row_pointers[y][x];
        }
    }
    // After execution, remember to release the memory
    for (int y = 0; y < height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);

    // Close PNG read structure
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
}

/* Draw a page as bitmap */
static int genSubBitmap(SmpteTtmlContext *ctx, AVSubtitleRect *subRect, vbi_page *page, int chopTop)
{
    int resx = page->columns * BITMAP_CHAR_WIDTH;
    int resy;
    int height;

    uint8_t ci;
    subRect->type = SUBTITLE_BITMAP;
    return 0;
}

SmpteTtmlParser::SmpteTtmlParser(std::shared_ptr<DataSource> source) {
    mDataSource = source;
    mParseType = TYPE_SUBTITLE_SMPTE_TTML;
    mIndex = 0;
    mDumpSub = 0;
    mPendingAction = -1;
    mTimeoutThread = std::shared_ptr<std::thread>(new std::thread(&SmpteTtmlParser::callbackProcess, this));

    initContext();
    checkDebug();
    sInstance = this;
}

SmpteTtmlParser *SmpteTtmlParser::sInstance = nullptr;

SmpteTtmlParser *SmpteTtmlParser::getCurrentInstance() {
    return SmpteTtmlParser::sInstance;
}

SmpteTtmlParser::~SmpteTtmlParser() {
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
    SmpteTtmlParser *parser = SmpteTtmlParser::getCurrentInstance();

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
bool SmpteTtmlParser::updateParameter(int type, void *data) {
    if (TYPE_SUBTITLE_SMPTE_TTML == type) {
        TtmlParam *pTtmlParam = (TtmlParam* )data;
    }
    return true;
}

int SmpteTtmlParser::initContext() {
    std::unique_lock<std::mutex> autolock(mMutex);
    mContext = new SmpteTtmlContext();
    if (!mContext) {
        SUBTITLE_LOGE("[%s::%d]malloc error! \n", __FUNCTION__, __LINE__);
    }
    mContext->formatId = 0;
    mContext->pts = AV_NOPTS_VALUE;
    return 0;
}

void SmpteTtmlParser::checkDebug() {
    //char value[PROPERTY_VALUE_MAX] = {0};
    //memset(value, 0, PROPERTY_VALUE_MAX);
    //property_get("vendor.subtitle.dump", value, "false");
    //if (!strcmp(value, "true")) {
        mDumpSub = false;
    //}
}

int SmpteTtmlParser::SmpteTtmlDecodeFrame(std::shared_ptr<AML_SPUVAR> spu, char *srcData, int srcLen) {
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

    tinyxml2::XMLDocument doc;
    int ret = doc.Parse(buf, bufSize);
    if (ret != tinyxml2::XML_SUCCESS) {
        SUBTITLE_LOGE("%s ret:%d Failed to parse XML", __FUNCTION__, ret);
        return -1;
    }

    tinyxml2::XMLElement* root = doc.RootElement();
    if (root == NULL) {
        SUBTITLE_LOGE("%s Failed to load file: No root element.", __FUNCTION__);
        doc.Clear();
        return -1;
    }


    tinyxml2::XMLElement *head = root->FirstChildElement("head");
    if (head == nullptr) head = root->FirstChildElement("tt:head");
    if (head == nullptr) {
        SUBTITLE_LOGE("%s Error. no head found!", __FUNCTION__);
        doc.Clear();
        return -1;
    }

    tinyxml2::XMLElement *smpteInformationParagraph = head->FirstChildElement("information");
    if (smpteInformationParagraph == nullptr) smpteInformationParagraph = head->FirstChildElement("smpte:information");
    if (smpteInformationParagraph != nullptr) {
        const tinyxml2::XMLAttribute *smpteInformationModeParagraphAttribute = smpteInformationParagraph->FindAttribute("smpte:mode");
        if (smpteInformationModeParagraphAttribute == nullptr ) {
            SUBTITLE_LOGE("smpteInformationModeParagraphAttribute is Null!\n");
        }
    }


    tinyxml2::XMLElement *metadata = head->FirstChildElement("metadata");
    if (metadata == nullptr) metadata = head->FirstChildElement("tt:metadata");
    if (metadata == nullptr) {
        SUBTITLE_LOGE("%s Error. no metadata found!", __FUNCTION__);
        doc.Clear();
        return -1;
    }

    tinyxml2::XMLElement *metadataSmpteImageParagraph = metadata->FirstChildElement("image");
    if (metadataSmpteImageParagraph == nullptr) metadataSmpteImageParagraph = metadata->FirstChildElement("smpte:image");




    tinyxml2::XMLElement *layout = head->FirstChildElement("layout");
    if (layout == nullptr) layout = head->FirstChildElement("tt:layout");
    if (layout == nullptr) {
        SUBTITLE_LOGE("%s Error. no layout found!", __FUNCTION__);
        doc.Clear();
        return -1;
    }

    tinyxml2::XMLElement *layoutRegionParagraph = layout->FirstChildElement("region");
    if (layoutRegionParagraph == nullptr) layoutRegionParagraph = layout->FirstChildElement("tt:region");

    // TODO: parse header, if we support style metadata and layout property
    // parse body
    tinyxml2::XMLElement *body = root->FirstChildElement("body");
    if (body == nullptr) body = root->FirstChildElement("tt:body");
    if (body == nullptr) {
        SUBTITLE_LOGE("%s Error. no body found!", __FUNCTION__);
        doc.Clear();
        return -1;
    }

    tinyxml2::XMLElement *div = body->FirstChildElement("div");
    if (div == nullptr) div = body->FirstChildElement("tt:div");
    tinyxml2::XMLElement *divDivParagraph = div->FirstChildElement("div");
    if (divDivParagraph == nullptr) divDivParagraph = div->FirstChildElement("tt:div");


    while (metadataSmpteImageParagraph != nullptr && layoutRegionParagraph != nullptr && divDivParagraph != nullptr) {
        const tinyxml2::XMLAttribute *metadataSmpteImageXmlIdParagraphAttribute     = metadataSmpteImageParagraph->FindAttribute("xml:id");
        const tinyxml2::XMLAttribute *metadataSmpteImageImageTypeParagraphAttribute = metadataSmpteImageParagraph->FindAttribute("imagetype");
        const tinyxml2::XMLAttribute *metadataSmpteImageEncodingParagraphAttribute  = metadataSmpteImageParagraph->FindAttribute("encoding");
        const char* imageData = metadataSmpteImageParagraph->GetText();
        const tinyxml2::XMLAttribute *layoutRegionXmlIdParagraphAttribute           = layoutRegionParagraph->FindAttribute("xml:id");
        const tinyxml2::XMLAttribute *layoutRegionTtsOriginParagraphAttribute       = layoutRegionParagraph->FindAttribute("tts:origin");
        const tinyxml2::XMLAttribute *layoutRegionTtsExtentParagraphAttribute       = layoutRegionParagraph->FindAttribute("tts:extent");
        const tinyxml2::XMLAttribute *divDivRegionParagraphAttribute                = divDivParagraph->FindAttribute("region");
        const tinyxml2::XMLAttribute *divDivBeginParagraphAttribute                 = divDivParagraph->FindAttribute("begin");
        const tinyxml2::XMLAttribute *divDivEndParagraphAttribute                   = divDivParagraph->FindAttribute("end");
        const tinyxml2::XMLAttribute *divDivSmpteBackgroundImageParagraphAttribute  = divDivParagraph->FindAttribute("smpte:backgroundImage");

        // Checks if the input is a null pointer or an empty string
        if (imageData == nullptr || *imageData == '\0') {
            SUBTITLE_LOGE("imageData is Null!\n");
        }

        // Find the position of the first non-newline character
        while (*imageData != '\0' && (*imageData == '\n' || *imageData == '\r')) {
            imageData++;
        }

        if (metadataSmpteImageXmlIdParagraphAttribute == nullptr ) {
            SUBTITLE_LOGI("metadataSmpteImageXmlIdParagraphAttribute is Null!\n");
        }

        if (metadataSmpteImageImageTypeParagraphAttribute == nullptr ) {
            SUBTITLE_LOGI("metadataSmpteImageImageTypeParagraphAttribute is Null!\n");
        }

        if (metadataSmpteImageEncodingParagraphAttribute == nullptr ) {
            SUBTITLE_LOGI("metadataSmpteImageEncodingParagraphAttribute is Null!\n");
        }


        if (layoutRegionXmlIdParagraphAttribute == nullptr ) {
            SUBTITLE_LOGI("layoutRegionXmlIdParagraphAttribute is Null!\n");
        }

        if (layoutRegionTtsOriginParagraphAttribute == nullptr ) {
            SUBTITLE_LOGI("layoutRegionTtsOriginParagraphAttribute is Null!\n");
        }
        if (layoutRegionTtsExtentParagraphAttribute == nullptr ) {
            SUBTITLE_LOGI("layoutRegionTtsExtentParagraphAttribute is Null!\n");
        }

        if (divDivRegionParagraphAttribute == nullptr ) {
            SUBTITLE_LOGI("divDivRegionParagraphAttribute is Null!\n");
        }
        if (divDivBeginParagraphAttribute == nullptr ) {
            SUBTITLE_LOGI("divDivBeginParagraphAttribute is Null!\n");
        }
        if (divDivEndParagraphAttribute == nullptr ) {
            SUBTITLE_LOGI("divDivEndParagraphAttribute is Null!\n");
        }
        if (divDivSmpteBackgroundImageParagraphAttribute == nullptr ) {
            SUBTITLE_LOGI("divDivSmpteBackgroundImageParagraphAttribute is Null!\n");
        }

        if (mDumpSub) {
            if (std::strcmp(divDivRegionParagraphAttribute->Value(), layoutRegionXmlIdParagraphAttribute->Value()) && std::strcmp(divDivSmpteBackgroundImageParagraphAttribute->Value()+1, metadataSmpteImageXmlIdParagraphAttribute->Value())) {
                        SUBTITLE_LOGI("%s region=%s  begin=%s end=%s smpte:backgroundImage=%s tts:origin=%s tts:extent=%s imagetype=%s encoding=%s imageData:%s\n", __FUNCTION__,
                        divDivRegionParagraphAttribute->Value(),
                        divDivBeginParagraphAttribute->Value(),
                        divDivEndParagraphAttribute->Value(),
                        divDivSmpteBackgroundImageParagraphAttribute->Value(),
                        layoutRegionTtsOriginParagraphAttribute->Value(),
                        layoutRegionTtsExtentParagraphAttribute->Value(),
                        metadataSmpteImageImageTypeParagraphAttribute->Value(),
                        metadataSmpteImageEncodingParagraphAttribute->Value(),
                        imageData);
            }
        }
        spu->m_delay = spu->pts + smpte_ttml_parse_timecode(divDivBeginParagraphAttribute->Value(), divDivEndParagraphAttribute->Value()) * 90;

        // Convert from base64 encoding to binary data
        std::vector<unsigned char> binaryImageData = base64Decode(imageData);
        if (mDumpSub) {
            char filename[32];
            snprintf(filename, sizeof(filename), "/tmp/smpte_tt(%d).png", mIndex++);
            save2PngFile(filename, reinterpret_cast<const char*>(binaryImageData.data()), binaryImageData.size());
        }
        if (binaryImageData.size() > 0) {
            if ("PNG" == std::string(metadataSmpteImageImageTypeParagraphAttribute->Value())) {
                decodePng(spu, reinterpret_cast<const char*>(binaryImageData.data()), binaryImageData.size());
            } else {
                SUBTITLE_LOGE("%s The image data is not in png format, which is not supported for now.",__FUNCTION__);
                return -1;
            }
        } else {
            SUBTITLE_LOGE("%s imageData size is null.",__FUNCTION__);
            return -1;
        }

        std::vector<int> layoutRegionTtsOriginNumbers = extractNumbers(layoutRegionTtsOriginParagraphAttribute->Value());
        std::vector<int> layoutRegionTtsExtentNumbers = extractNumbers(layoutRegionTtsExtentParagraphAttribute->Value());
        std::vector<int>::iterator layoutRegionTtsOriginIt = layoutRegionTtsOriginNumbers.begin();
        std::vector<int>::iterator layoutRegionTtsExtentIt = layoutRegionTtsExtentNumbers.begin();
        if (layoutRegionTtsOriginIt != layoutRegionTtsOriginNumbers.end()) {
            spu->spu_start_x = *layoutRegionTtsOriginIt;
            ++layoutRegionTtsOriginIt;
            if (layoutRegionTtsOriginIt != layoutRegionTtsOriginNumbers.end()) {
                spu->spu_start_y = *layoutRegionTtsOriginIt;
            }
        }
        if (layoutRegionTtsExtentIt != layoutRegionTtsExtentNumbers.end()) {
            spu->spu_width = *layoutRegionTtsExtentIt;
            ++layoutRegionTtsExtentIt;
            if (layoutRegionTtsExtentIt != layoutRegionTtsExtentNumbers.end()) {
                spu->spu_height = *layoutRegionTtsExtentIt;
            }
        }
        metadataSmpteImageParagraph = metadataSmpteImageParagraph->NextSiblingElement();
        layoutRegionParagraph       = layoutRegionParagraph->NextSiblingElement();
        divDivParagraph             = divDivParagraph->NextSiblingElement();
        if (spu->spu_origin_display_w <= 0 || spu->spu_origin_display_h <= 0) {
            //spu->spu_origin_display_w = 720;
            //spu->spu_origin_display_h = 480;

            spu->spu_origin_display_w = VideoInfo::Instance()->getVideoWidth();
            spu->spu_origin_display_h = VideoInfo::Instance()->getVideoHeight();
        }
        SUBTITLE_LOGI("%s region=%s  begin=%s end=%s smpte:backgroundImage=%s\n", __FUNCTION__, divDivRegionParagraphAttribute->Value(), divDivBeginParagraphAttribute->Value(), divDivEndParagraphAttribute->Value(), divDivSmpteBackgroundImageParagraphAttribute->Value());
        SUBTITLE_LOGI("%s xml:id=%s tts:origin=%s tts:extent=%s\n", __FUNCTION__, layoutRegionXmlIdParagraphAttribute->Value(), layoutRegionTtsOriginParagraphAttribute->Value(), layoutRegionTtsExtentParagraphAttribute->Value());
        SUBTITLE_LOGI("%s smpte:image xml:id=%s imagetype=%s encoding=%s imageData:%s", __FUNCTION__, metadataSmpteImageXmlIdParagraphAttribute->Value(), metadataSmpteImageImageTypeParagraphAttribute->Value(), metadataSmpteImageEncodingParagraphAttribute->Value(), imageData);
        SUBTITLE_LOGI("%s spu->spu_width:%d,spu->spu_height:%d spu->buffer_size:%d spu->spu_origin_display_w:%d spu->spu_origin_display_h:%d\n", __FUNCTION__, spu->spu_width, spu->spu_height, spu->buffer_size, spu->spu_origin_display_w, spu->spu_origin_display_h);
        addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
        spu->pts = spu->m_delay;

    }
    doc.Clear();
    return 0;
}

int SmpteTtmlParser::getTTMLSpu() {
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

int SmpteTtmlParser::hwDemuxParse(std::shared_ptr<AML_SPUVAR> spu) {
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
            spu->subtitle_type = TYPE_SUBTITLE_SMPTE_TTML;
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
                    ret = SmpteTtmlDecodeFrame(spu, buf, packetLen);
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

int SmpteTtmlParser::softDemuxParse(std::shared_ptr<AML_SPUVAR> spu) {
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

        spu->subtitle_type = TYPE_SUBTITLE_SMPTE_TTML;
        spu->pts = mDataSource->getSyncTime();
        if (isMore32Bit(spu->pts) && !isMore32Bit(mDataSource->getSyncTime())) {
            SUBTITLE_LOGI("SUB PTS is greater than 32 bits, before subpts: %lld, vpts:%lld", spu->pts, mDataSource->getSyncTime());
            spu->pts &= TSYNC_32_BIT_PTS;
        }
        //If gap time is large than 9 secs, think pts skip,notify info
        //if (abs(mDataSource->getSyncTime() - spu->pts) > 9*90000) {
        //    SUBTITLE_LOGE("[%s::%d]pts skip,vpts:%lld, spu->pts:%lld notify time error!\n", __FUNCTION__, __LINE__, mDataSource->getSyncTime(), spu->pts);
        //}
        //SUBTITLE_LOGI("[%s::%d]pts ---------------,vpts:%lld, spu->pts:%lld\n", __FUNCTION__, __LINE__, mDataSource->getSyncTime(), spu->pts);

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

        ret = SmpteTtmlDecodeFrame(spu, data, dataLen);
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

int SmpteTtmlParser::atvHwDemuxParse(std::shared_ptr<AML_SPUVAR> spu) {
        char tmpbuf[256] = {0};
        int64_t dvbPts = 0, ptsDiff = 0;
        int ret = 0;
        int lineNum = 0;
        char *data = NULL;

        // read package header info
        if (mDataSource->read(tmpbuf, 4) == 4) {
            spu->subtitle_type = TYPE_SUBTITLE_SMPTE_TTML;
            data = (char *)malloc(ATV_SMPTE_TTML_DATA_LEN);
            if (!data) {
                SUBTITLE_LOGE("[%s::%d]malloc error! \n", __FUNCTION__,__LINE__);
                return -1;
            }
            memset(data, 0x0, ATV_SMPTE_TTML_DATA_LEN);
            ret = mDataSource->read(data, ATV_SMPTE_TTML_DATA_LEN);
            if (mDumpSub) SUBTITLE_LOGI("[%s::%d] ret=%d,dataLen=%d, %x,%x,%x,%x,%x,%x,%x,%x,---------\n",
                    __FUNCTION__, __LINE__, ret, ATV_SMPTE_TTML_DATA_LEN, data[0], data[1], data[2], data[3],
                    data[4], data[5], data[6], data[7]);

            ret = SmpteTtmlDecodeFrame(spu, data, ATV_SMPTE_TTML_DATA_LEN);

            if (ret != -1) {
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

void SmpteTtmlParser::callbackProcess() {
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

int SmpteTtmlParser::getSpu() {
    if (mState == SUB_INIT) {
        mState = SUB_PLAYING;
    } else if (mState == SUB_STOP) {
        SUBTITLE_LOGI(" mState == SUB_STOP \n\n");
        return 0;
    }

    return getTTMLSpu();
}

int SmpteTtmlParser::getInterSpu() {
    return getSpu();
}

int SmpteTtmlParser::parse() {
    while (!mThreadExitRequested) {
        if (getInterSpu() < 0) {
            // advance input, if parse failed, wait and retry.
            //std::this_thread::sleep_for(std::chrono::milliseconds(1));
            usleep(100);
        }
    }
    return 0;
}

void SmpteTtmlParser::dump(int fd, const char *prefix) {
    //TODO: dump run in binder thread, may need add lock!
    dprintf(fd, "%s TTML Parser\n", prefix);
    dumpCommon(fd, prefix);

    if (mContext != nullptr) {
        dprintf(fd, "%s  formatId: %d (0 = bitmap, 1 = text/ass)\n", prefix, mContext->formatId);
        dprintf(fd, "%s  pts:%lld\n", prefix, mContext->pts);
        dprintf(fd, "\n");
    }
}
