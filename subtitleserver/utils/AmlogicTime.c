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
 *      Clock, time related functions
 ***************************************************************************/

#define  LOG_TAG "AM_TIME"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "SubtitleLog.h"

#include "AmlogicTime.h"


/** Get the time from boot to current system running, in milliseconds
  *  clock returns the system running time
  * - AM_SUCCESS Success
  * - other values error code (see AmlogicTime.h)
  */
int AmlogicGetClock(int *clock)
{
    struct timespec ts;
    int ms;

    assert(clock);

    clock_gettime(CLOCK_MONOTONIC, &ts);
    ms = ts.tv_sec*1000+ts.tv_nsec/1000000;
    *clock = ms;

    return AM_SUCCESS;
}

/** Get the time from boot to current system running, the format is struct timespec
  * ts returns the current timespec value
  * - AM_SUCCESS Success
  * - other values error code (see AmlogicTime.h)
  */
int AmlogicGetTimeSpec(struct timespec *ts)
{
    assert(ts);

    clock_gettime(CLOCK_MONOTONIC, ts);

    return AM_SUCCESS;
}

/** Get the timespec value after a number of milliseconds
  * This function is mainly used for pthread_cond_timedwait, sem_Functions such as timedwait calculate the timeout time.
  * timeout timeout in milliseconds
  * ts returns timespec value
  * - AM_SUCCESS Success
  * - other values error code (see AmlogicTime.h)
  */
int AmlogicGetTimeSpecTimeout(int timeout, struct timespec *ts)
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

