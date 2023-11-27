/***************************************************************************
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
 *
 * Description:
 */
/**\file
 * \brief iconv functions
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2014-03-18: create the document
 ***************************************************************************/
#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif

#define  LOG_TAG "AM_ICONV"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include "SubtitleLog.h"
#include <stdbool.h>

#include <am_iconv.h>


UConverter* (*am_ucnv_open_ptr)(const char *converterName, UErrorCode *err);
void (*am_ucnv_close_ptr)(UConverter * converter);
void (*am_ucnv_convertEx_ptr)(UConverter *targetCnv, UConverter *sourceCnv,
        char **target, const char *targetLimit,
        const char **source, const char *sourceLimit,
        UChar *pivotStart, UChar **pivotSource,
        UChar **pivotTarget, const UChar *pivotLimit,
        UBool reset, UBool flush,
        UErrorCode *pErrorCode);
void (*am_u_setDataDirectory_ptr)(const char *directory);
void (*am_u_init_ptr)(long *status);

#ifdef USE_VENDOR_ICU
bool actionFlag = false;
void am_first_action(void)
{
    if (!actionFlag) {
        setenv("ICU_DATA", "/system/usr/icu", 1);
        u_setDataDirectory("/system/usr/icu");
        long status = 0;
        u_init(&status);
        if (status > 0)
            SUBTITLE_LOGE("icu init fail. [%ld]", status);
        actionFlag = true;
    }
}
#endif

void
am_ucnv_dlink(void)
{
    static void* handle = NULL;

    if (handle == NULL) {
        handle = dlopen("libicuuc.so", RTLD_LAZY);
        setenv("ICU_DATA", "/system/usr/icu", 1);
    }
    assert(handle);

#define LOAD_UCNV_SYMBOL(name, post)\
    if (!am_##name##_ptr)\
        am_##name##_ptr = dlsym(handle, #name post);
#define LOAD_UCNV_SYMBOLS(post)\
    LOAD_UCNV_SYMBOL(ucnv_open, post)\
    LOAD_UCNV_SYMBOL(ucnv_close, post)\
    LOAD_UCNV_SYMBOL(ucnv_convertEx, post)\
    LOAD_UCNV_SYMBOL(u_setDataDirectory, post)\
    LOAD_UCNV_SYMBOL(u_init, post)

#define CHECK_LOAD_SYMBOL(name)\
    if (!am_##name##_ptr) {\
        SUBTITLE_LOGE(#name" not found. ucnv init fail.");}
#define CHECK_LOAD_SYMBOLS()\
    CHECK_LOAD_SYMBOL(ucnv_open)\
    CHECK_LOAD_SYMBOL(ucnv_close)\
    CHECK_LOAD_SYMBOL(ucnv_convertEx)\
    CHECK_LOAD_SYMBOL(u_setDataDirectory)\
    CHECK_LOAD_SYMBOL(u_init)

    LOAD_UCNV_SYMBOLS("")
    LOAD_UCNV_SYMBOLS("_48")
    LOAD_UCNV_SYMBOLS("_51")
    LOAD_UCNV_SYMBOLS("_53")
    LOAD_UCNV_SYMBOLS("_55")
    LOAD_UCNV_SYMBOLS("_56")
    LOAD_UCNV_SYMBOLS("_58")
    LOAD_UCNV_SYMBOLS("_60")

    CHECK_LOAD_SYMBOLS()

    if (am_u_setDataDirectory_ptr)
        am_u_setDataDirectory_ptr("/system/usr/icu");
    if (am_u_init_ptr) {
        long status = 0;
        am_u_init_ptr(&status);
        if (status > 0) {
            SUBTITLE_LOGE("icu init fail. [%ld]", status);
        }
    }
}

