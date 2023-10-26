/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
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

