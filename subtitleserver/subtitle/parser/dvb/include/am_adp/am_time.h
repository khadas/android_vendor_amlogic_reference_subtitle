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
 * \brief 时钟、时间相关函数
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-05-21: create the document
 ***************************************************************************/

#ifndef _AM_TIME_H
#define _AM_TIME_H

#include "am_types.h"
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/
#ifdef NEED_MIN_DVB
/*extern int  __pthread_cond_timedwait(pthread_cond_t*,
                                     pthread_mutex_t*,
                                     const struct timespec*,
                                     clockid_t);
#define pthread_cond_timedwait(c, m, a) __pthread_cond_timedwait(c, m, a, CLOCK_MONOTONIC);
*/
#endif

/****************************************************************************
 * Type definitions
 ***************************************************************************/


/****************************************************************************
 * Function prototypes
 ***************************************************************************/

/**\brief 得到开机到当前系统运行的时间，单位为毫秒
 * \param[out] clock 返回系统运行时间
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_time.h)
 */
extern AM_ErrorCode_t AM_TIME_GetClock(int *clock);

/**\brief 得到开机到当前系统运行的时间，格式为struct timespec
 * \param[out] ts 返回当前timespec值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_time.h)
 */
extern AM_ErrorCode_t AM_TIME_GetTimeSpec(struct timespec *ts);

/**\brief 得到若干毫秒后的timespec值
 * 此函数主要用于pthread_cond_timedwait, sem_Functions such as timedwait calculate the timeout time。
 * \param timeout 以毫秒为单位的超时时间
 * \param[out] ts 返回timespec值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_time.h)
 */
extern AM_ErrorCode_t AM_TIME_GetTimeSpecTimeout(int timeout, struct timespec *ts);

#ifdef __cplusplus
}
#endif

#endif

