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
 * \brief Clock, time related functions
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-05-21: create the document
 ***************************************************************************/
#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif

#define  LOG_TAG "AM_TIME"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <utils/Log.h>

#include <am_time.h>


/****************************************************************************
 * Macro definitions
 ***************************************************************************/
#ifdef NEED_MIN_DVB
//#undef CLOCK_REALTIME
//#define CLOCK_REALTIME    CLOCK_MONOTONIC
#endif

/****************************************************************************
 * API functions
 ***************************************************************************/

/**\brief 得到开机到当前系统运行的时间，单位为毫秒
 * \param[out] clock 返回系统运行时间
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_time.h)
 */
AM_ErrorCode_t AM_TIME_GetClock(int *clock)
{
    struct timespec ts;
    int ms;

    assert(clock);

    clock_gettime(CLOCK_MONOTONIC, &ts);
    ms = ts.tv_sec*1000+ts.tv_nsec/1000000;
    *clock = ms;

    return AM_SUCCESS;
}

/**\brief 得到开机到当前系统运行的时间，格式为struct timespec
 * \param[out] ts 返回当前timespec值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_time.h)
 */
AM_ErrorCode_t AM_TIME_GetTimeSpec(struct timespec *ts)
{
    assert(ts);

    clock_gettime(CLOCK_MONOTONIC, ts);

    return AM_SUCCESS;
}

/**\brief 得到若干毫秒后的timespec值
 * 此函数主要用于pthread_cond_timedwait, sem_timedwait等函数计算超时时间。
 * \param timeout 以毫秒为单位的超时时间
 * \param[out] ts 返回timespec值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_time.h)
 */
AM_ErrorCode_t AM_TIME_GetTimeSpecTimeout(int timeout, struct timespec *ts)
{
    struct timespec ots;
    int left, diff;

    assert(ts);

    clock_gettime(CLOCK_MONOTONIC, &ots);

    ts->tv_sec  = ots.tv_sec + timeout/1000;
    ts->tv_nsec = ots.tv_nsec;

    left = timeout % 1000;
    left *= 1000000;
    diff = 1000000000-ots.tv_nsec;

    if (diff <= left)
    {
        ts->tv_sec++;
        ts->tv_nsec = left-diff;
    }
    else
    {
        ts->tv_nsec += left;
    }

    return AM_SUCCESS;
}

