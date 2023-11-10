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

#include <time.h>

#include "AmlogicUtil.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Get the time from boot to current system running, in milliseconds
  *  clock returns the system running time
  * - AM_SUCCESS Success
  * - other values error code (see AmlogicTime.h)
  */
extern int AmlogicGetClock(int *clock);

/** Get the time from boot to current system running, the format is struct timespec
  * ts returns the current timespec value
  * - AM_SUCCESS Success
  * - other values error code (see AmlogicTime.h)
  */
extern int AmlogicGetTimeSpec(struct timespec *ts);
/** Get the timespec value after a number of milliseconds
  * This function is mainly used for pthread_cond_timedwait, sem_Functions such as timedwait calculate the timeout time.
  * timeout timeout in milliseconds
  * ts returns timespec value
  * - AM_SUCCESS Success
  * - other values error code (see AmlogicTime.h)
  */
extern int AmlogicGetTimeSpecTimeout(int timeout, struct timespec *ts);

#ifdef __cplusplus
}
#endif
