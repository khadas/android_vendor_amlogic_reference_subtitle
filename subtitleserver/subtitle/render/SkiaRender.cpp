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
#include <utils/Log.h>

#include "SkiaRender.h"


#include <core/SkBitmap.h>
#include <core/SkStream.h>
#include <core/SkCanvas.h>

// color, bpp, text size, font ...
SkPaint collectPaintInfo() {
    SkPaint paint;
    //paint.setXfermodeMode(SkXfermode::kSrc_Mode);

    paint.setARGB(255, 198, 0, 255);
    paint.setTextSize(32);
    paint.setAntiAlias(true);
    paint.setSubpixelText(true);
    paint.setLCDRenderText(true);
    return paint;
}

// TODO: auto wrap to next line, if text too long
bool drawTextAt(int x, int y, const char *text, SkCanvas &canvas, SkPaint &paint) {

    SkRect textRect;
    int textlen = paint.measureText(text, strlen(text), &textRect);

    canvas.drawText(text, strlen(text), x, y, paint);

    return true;
}

SkiaRender::SkiaRender(std::shared_ptr<Display> display) : mDisplay(display) {

}

bool SkiaRender::drawAndPost() {
    int width =1920, height = 1080, stride=0;
    int bytesPerPixel;

    void *buf = mDisplay->lockDisplayBuffer(&width, &height, &stride, &bytesPerPixel);
    //ssize_t bpr = stride * bytesPerPixel;

    SkBitmap surfaceBitmap;
    surfaceBitmap.setInfo(SkImageInfo::Make(width, height, kN32_SkColorType, kPremul_SkAlphaType));
    surfaceBitmap.setPixels(buf);

    SkPaint paint = collectPaintInfo();
    SkCanvas surfaceCanvas(surfaceBitmap);

    int delta = 50;
    for (auto it = mShowingSubs.begin(); it != mShowingSubs.end(); it++) {
        drawTextAt(50, height-delta -50, (const char *)((*it)->spu_data), surfaceCanvas, paint);
        delta += 50;
    }

    mDisplay->unlockAndPostDisplayBuffer(nullptr);
    return true;

}

// TODO: the subtitle may has some params, config how to render
//       Need impl later.
bool SkiaRender::showSubtitleItem(std::shared_ptr<AML_SPUVAR> spu,int type) {
    ALOGD("showSubtitleItem");
    mShowingSubs.push_back(spu);
    return drawAndPost();
}

void AndroidHidlRemoteRender::resetSubtitleItem() {
    mShowingSubs.clear();
}

bool SkiaRender::hideSubtitleItem(std::shared_ptr<AML_SPUVAR> spu) {
    ALOGD("hideSubtitleItem");
    mShowingSubs.remove(spu);
    return drawAndPost();
}

