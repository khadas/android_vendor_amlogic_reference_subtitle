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

#ifndef SUBTITLE_SERVICE_UTILS_H
#define SUBTITLE_SERVICE_UTILS_H
#if ANDROID_PLATFORM_SDK_VERSION > 27
#include "subtitleServerHidlClient/SubtitleServerHidlClient.h"
#endif

#if ANDROID_PLATFORM_SDK_VERSION > 27
    typedef enum
    {
        SUBTITLE_UNAVAILABLE,
        SUBTITLE_AVAILABLE
    }SUBTITLE_STATE;

    typedef enum
    {
         SUBTITLE_EVENT_DATA,
         SUBTITLE_EVENT_NONE
    }SUBTITLE_EVENT;

    typedef void (*AM_SUBTITLEEVT)(SUBTITLE_EVENT evt, int index);
    typedef void (*AM_SUBTITLELIS)(SUBTITLE_STATE state, int val);

    typedef struct
    {
        AM_SUBTITLEEVT sub_evt;
        AM_SUBTITLELIS  available;
    }AM_SUBTITLE_Para_t;

    typedef void (*notifyAvailable) (int available);
    void subtitleCreat();
    void subtitleDestroy();
    void subtitleOpen(char* path, void *pthis, android::SubtitleServerHidlClient::SUB_Para_t * para);
    void subtitleSetSubPid(int pid);
    int subtitleGetSubHeight();
    const char* subtitleGetLanguage();
    int subtitleGetSubWidth();
    void subtitleSetSubType(int type);
    void registerSubtitleMiddleListener();
    void subtitle_register_available(AM_SUBTITLELIS sub);
    void subtitle_register_event(AM_SUBTITLEEVT evt);
    void switchSubtitle(android::SubtitleServerHidlClient::SUB_Para_t * para);
#else
    void subtitleOpen(char* path, void *pthis);
    char* subtitleGetLanguage(int idx);
#endif

    void subtitleShow();

    void subtitleOpenIdx(int idx);
    void subtitleClose();
    int subtitleGetTotal();
    void subtitleNext();
    void subtitlePrevious();
    void subtitleShowSub(int pos);
    void subtitleOption();
    int subtitleGetType();
    char* subtitleGetTypeStr();
    int subtitleGetTypeDetail();
    void subtitleSetTextColor(int color);
    void subtitleSetTextSize(int size);
    void subtitleSetGravity(int gravity);
    void subtitleSetTextStyle(int style);
    void subtitleSetPosHeight(int height);
    void subtitleSetImgRatio(float ratioW, float ratioH, int maxW, int maxH);
    void subtitleClear();
    void subtitleResetForSeek();
    void subtitleHide();
    void subtitleDisplay();
    char* subtitleGetCurName();
    char* subtitleGetName(int idx);
    void subtitleLoad(char* path);
    void subtitleSetSurfaceViewParam(int x, int y, int w, int h);

#endif
