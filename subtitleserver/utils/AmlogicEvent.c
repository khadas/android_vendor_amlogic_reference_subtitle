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
 *      brief driver module event interface
 *      Event is an asynchronous triggering mechanism that replaces the Callback function.
 *      Multiple callback functions can be registered for each event at the same time.
 ***************************************************************************/
#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif

#define  LOG_TAG "AM_EVT"
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include "SubtitleLog.h"

#include "AmlogicEvent.h"

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define AM_EVENT_BUCKET_COUNT    50

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/** event*/
typedef struct AM_Event AM_Event_t;
struct AM_Event
{
     AM_Event_t *next; /**< points to the next event in the linked list*/
     AM_EVT_Callback_t cb; /**< callback function*/
     int type; /**< event type*/
     long dev_no; /**< device number*/
     void *data; /**< User callback parameters*/
};

/****************************************************************************
 * Static data
 ***************************************************************************/

static AM_Event_t *events[AM_EVENT_BUCKET_COUNT];

#ifndef NEED_MIN_DVB
static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#else
#ifdef PTHREAD_RECURSIVE_MUTEX_INITIALIZER
static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
#else
static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#endif
#endif

pthread_mutexattr_t attr;

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

/****************************************************************************
 * API functions
 ***************************************************************************/

/** Register an event callback function
  * dev_no device ID corresponding to the callback function
  * event_type The event type corresponding to the callback function
  * cb callback function pointer
  * data User-defined parameters passed to the callback function

  * - AM_SUCCESS Success
  * - other values Error code
  */
int AmlogicEventSubscribe(long dev_no, int event_type, AM_EVT_Callback_t cb, void *data)
{
    AM_Event_t *evt;
    int pos;
    int i;

    assert(cb);

    // Same callback registration, filter out.
    // There is the possibility of the same dev_no. (The malloc function allocates again after it is released, often the same pointer)
    // For the time being, it is still the original design, using dev_no as one of the event flags, but in fact it has little meaning. The main thing is to use event_type, for example, it is reasonable.
    // Similarly, the binding of cb and dev_no is of little significance, causing the upper layer to need to call Subscribe/unSubscribe every time it is paired.
    // Subscribe/unSubscribe if done in the cb function, it is very easy to cause logical problems (delayed processing) or deadlock.
    // Unless the upper layer uses msg to restart and use threads to process it, this is an unreasonable forced design, pass
    // Is there a many-to-many requirement for events and monitoring? It seems not

    pthread_rwlock_rdlock(&rwlock);
    for (i = 0; i < AM_EVENT_BUCKET_COUNT; i++)
    for (evt=events[i]; evt; evt=evt->next) {
        //--If you use dev_no filtering, there may be a small amount of memory waste. If you don't use it, but dev_no is dynamic (handle), fk
        if (evt->dev_no == dev_no && evt->cb == cb && evt->type == event_type) {
            SUBTITLE_LOGI("the same cb set");
            pthread_rwlock_unlock(&rwlock);
            return AM_SUCCESS;
        }
    }
    pthread_rwlock_unlock(&rwlock);


    /*Assign events*/
    evt = malloc(sizeof(AM_Event_t));
    if (!evt) {
        SUBTITLE_LOGI("not enough memory");
        return AM_EVENT_ERROR_NO_MEM;
    }
    evt->dev_no = dev_no;
    evt->type   = event_type;
    evt->cb     = cb;
    evt->data   = data;

    pos = event_type%AM_EVENT_BUCKET_COUNT;

    /*Add to event hash table*/
    //pthread_mutex_lock(&lock);
    pthread_rwlock_wrlock(&rwlock);
    evt->next   = events[pos];
    events[pos] = evt;

    //pthread_mutex_unlock(&lock);
    pthread_rwlock_unlock(&rwlock);
    return AM_SUCCESS;
}

/** Unregister an event callback function
  * dev_no device ID corresponding to the callback function
  * event_type The event type corresponding to the callback function
  * cb callback function pointer
  * data User-defined parameters passed to the callback function

  * - AM_SUCCESS Success
  * - other values Error code
  */
int AmlogicEventUnsubscribe(long dev_no, int event_type, AM_EVT_Callback_t cb, void *data)
{
    AM_Event_t *evt, *eprev;
    int pos;

    assert(cb);

    pos = event_type%AM_EVENT_BUCKET_COUNT;

    //pthread_mutex_lock(&lock);
    pthread_rwlock_wrlock(&rwlock);
    for (eprev=NULL,evt=events[pos]; evt; eprev=evt,evt=evt->next)
    {
        if ((evt->dev_no == dev_no) && (evt->type == event_type) && (evt->cb == cb) &&
                (evt->data == data))
        {
            if (eprev)
                eprev->next = evt->next;
            else
                events[pos] = evt->next;
            break;
        }
    }

    //pthread_mutex_unlock(&lock);
    pthread_rwlock_unlock(&rwlock);
    if (evt)
    {
        free(evt);
        return AM_SUCCESS;
    }

    return AM_EVENT_ERROR_NOT_SUBSCRIBED;
}

/** triggers an event
  * dev_no device ID that generated the event
  * event_type The type of event generated
  * \param[in] param event parameters
  */
int AmlogicEventSignal(long dev_no, int event_type, void *param)
{
    AM_Event_t *evt;
    int pos = event_type%AM_EVENT_BUCKET_COUNT;

    //1. Use read-write lock,
    //2. Multiple threads can send signal events at the same time,
    //3. If multiple threads send the same event at the same time and trigger the same callback at the same time, there will be risks, but generally speaking, the same event can only be sent by the same thread, let alone at the same time.
    //4. If there are different threads at the same time, sending the same event and triggering the same callback, then it is a quite case, and you can lock it yourself in the cb function of the upper case.
    //5. But the upper layer should avoid going to the AmlogicEventSubscribe/AmlogicEventUnsubscribe event list in the cb event response function, because this will cause a deadlock.
    //6. It is not necessary or necessary to go to the AmlogicEventSubscribe/AmlogicEventUnsubscribe event list in the cb function, remember!!!
    pthread_rwlock_rdlock(&rwlock);
    for (evt=events[pos]; evt; evt=evt->next)
    {
        if ((evt->dev_no == dev_no) && (evt->type == event_type))
        {
            //pthread_mutex_lock(&lock);
            evt->cb(dev_no, event_type, param, evt->data);
            //pthread_mutex_unlock(&lock);
        }
    }
    pthread_rwlock_unlock(&rwlock);
    return AM_SUCCESS;
}
/*
*/
int AmlogicEventInit()
{
    int i;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&lock, &attr);
    pthread_mutexattr_destroy(&attr);

    for (i = 0; i < AM_EVENT_BUCKET_COUNT; i++) events[i] = NULL;
    return AM_SUCCESS;
}
/*
*/
int AmlogicEventDestroy()
{
    AM_Event_t *evt;
    int i;
    pthread_mutex_destroy(&lock);

    for ( i = 0; i < AM_EVENT_BUCKET_COUNT; i++ )
    {
        AM_Event_t *tmp, *head = events[i];
        while (head != NULL)
        {
            tmp = head;
            head = head->next;
            free(tmp);
        }
    }
    return AM_SUCCESS;
}
