/****************************************************************************
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
 * \brief Condition variable.
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2017-11-07: create the document
 ***************************************************************************/

#ifndef _AM_COND_H
#define _AM_COND_H

#include "am_types.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define pthread_cond_init(c, a)\
    ({\
        pthread_condattr_t attr;\
        pthread_condattr_t *pattr;\
        int r;\
        if (a) {\
            pattr = a;\
        } else {\
            pattr = &attr;\
            pthread_condattr_init(pattr);\
        }\
        pthread_condattr_setclock(pattr, CLOCK_MONOTONIC);\
        r = pthread_cond_init(c, pattr);\
        if (pattr == &attr) {\
            pthread_condattr_destroy(pattr);\
        }\
        r;\
     })

#ifdef __cplusplus
}
#endif

#endif

