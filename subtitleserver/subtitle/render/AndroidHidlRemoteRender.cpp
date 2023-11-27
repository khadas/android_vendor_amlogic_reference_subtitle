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

#define LOG_TAG "SubtitleRender"

#include <unistd.h>
#include <fcntl.h>
#include <string>
#include "SubtitleLog.h"

#include "AndroidHidlRemoteRender.h"
#include "AndroidCallbackMessageQueue.h"
#include "ParserFactory.h"

//using ::vendor::amlogic::hardware::subtitleserver::V1_0::implementation::SubtitleServerHal;
using amlogic::AndroidCallbackMessageQueue;

static const int FADING_SUB = 0;
static const int SHOWING_SUB = 1;

bool AndroidHidlRemoteRender::postSubtitleData() {
    // TODO: share buffers. if need performance
    //std:: string out;
    int width=0, height=0, size=0,x =0, y=0, videoWidth = 0, videoHeight = 0, objectSegmentId = 0;

    sp<AndroidCallbackMessageQueue> queue = AndroidCallbackMessageQueue::Instance();

    if (mShowingSubs.size() <= 0) {
        if (queue != nullptr) {
            for (int i=0; i<=mCurrentMaxObjectId; i++) {
                SUBTITLE_LOGI("AndroidHidlRemoteRender:%s objectId=%d",__func__, i);
                queue->postDisplayData(nullptr, mParseType, 0, 0, 0, 0, 0, 0, 0, FADING_SUB, i);
            }
            mCurrentMaxObjectId = 0;
            return true;
        } else {
            SUBTITLE_LOGE("Error! should not null here!");
            return false;
        }
    }

    // only show the newest, since only 1 line for subtitle.
    for (auto it = mShowingSubs.rbegin(); it != mShowingSubs.rend(); it++) {

        if (((*it)->spu_data) == nullptr) {
            if ((*it)->dynGen) {
                (*it)->genSubtitle();
                SUBTITLE_LOGI("dynamic gen and return!");
                if (((*it)->spu_data) == nullptr) {
                    SUBTITLE_LOGE("Error! why still no decoded spu_data???");
                    continue;
                }
            } else {
                SUBTITLE_LOGE("Error! why not decoded spu_data, but push to show???");
                continue;
            }
        }

        width = (*it)->spu_width;
        height = (*it)->spu_height;
        if (mParseType == TYPE_SUBTITLE_DTVKIT_TELETEXT || mParseType == TYPE_SUBTITLE_DVB_TELETEXT) {
            x = (*it)->disPlayBackground;
        } else {
            x = (*it)->spu_start_x;
        }
        y = (*it)->spu_start_y;
        videoWidth = (*it)->spu_origin_display_w;
        videoHeight = (*it)->spu_origin_display_h;
        objectSegmentId = (*it)->objectSegmentId;
        if (mCurrentMaxObjectId < objectSegmentId) mCurrentMaxObjectId = objectSegmentId;
        size = (*it)->buffer_size;

        SUBTITLE_LOGI(" in AndroidHidlRemoteRender:%s type:%d, width=%d, height=%d data=%p size=%d",
            __func__, mParseType,  width, height, (*it)->spu_data, (*it)->buffer_size);
        DisplayType  displayType = ParserFactory::getDisplayType(mParseType);
        if ((SUBTITLE_IMAGE_DISPLAY == displayType) && ((0 == width) || (0 == height))) {
           continue;
        }

        if (mParseType == TYPE_SUBTITLE_EXTERNAL) {
            if (width > 0 && height > 0) {
                displayType = SUBTITLE_IMAGE_DISPLAY;
            } else {
                displayType = SUBTITLE_TEXT_DISPLAY;
            }
        }

        if (SUBTITLE_TEXT_DISPLAY == displayType) {
            width = height = 0; // no use for text!
        }

        /* The same as vlc player. showing and fading policy */
        if (queue != nullptr) {
            mParseType = ((*it)->isQtoneData) ? TYPE_SUBTITLE_Q_TONE_DATA: mParseType;
            queue->postDisplayData((const char *)((*it)->spu_data), mParseType, x, y, width, height, videoWidth, videoHeight, size, SHOWING_SUB, objectSegmentId);
            return true;
        } else {
            return false;
        }
    }

    return false;
}



// TODO: the subtitle may has some params, config how to render
//       Need impl later.
bool AndroidHidlRemoteRender::showSubtitleItem(std::shared_ptr<AML_SPUVAR> spu, int type) {
    SUBTITLE_LOGI("showSubtitleItem");
    mShowingSubs.push_back(spu);
    mParseType = type;

    return postSubtitleData();
}

void AndroidHidlRemoteRender::resetSubtitleItem() {
    mShowingSubs.clear();

    // flush showing
    postSubtitleData();
}

bool AndroidHidlRemoteRender::hideSubtitleItem(std::shared_ptr<AML_SPUVAR> spu) {
    SUBTITLE_LOGI("hideSubtitleItem");
    //some stream is special.some subtitles have pts, but some subtitles don't have pts.
    //In this situation if use the remove() function,it may cause the subtitle contains
    //pts don't hide until the subtitle without pts disappear.
    //mShowingSubs.remove(spu);
    mShowingSubs.clear();
    return postSubtitleData();
}

void AndroidHidlRemoteRender::removeSubtitleItem(std::shared_ptr<AML_SPUVAR> spu)  {
    SUBTITLE_LOGI("removeSubtitleItem");
    mShowingSubs.remove(spu);
}

