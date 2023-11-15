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
 * \brief pthread signal registration
 *
 * \author Yan Yan <Yan.Yan@amlogic.com>
 * \date 2018-04-03: create the document
 ***************************************************************************/
#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif

#define  LOG_TAG "AM_SIG_HANDLER"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <utils/Log.h>
#include <signal.h>

static pthread_once_t once = PTHREAD_ONCE_INIT;

static void sig_handler(int signo)
{
    pthread_t tid =pthread_self();
    ALOGD("signal handler, tid %ld, signo %d", tid, signo);
}

static void register_sig_handler()
{
    struct sigaction action = {0};
    action.sa_flags = 0;
    action.sa_handler = sig_handler;
    sigaction(SIGALRM, &action, NULL);
}

void AM_SigHandlerInit()
{
    pthread_once(&once, register_sig_handler);
}

