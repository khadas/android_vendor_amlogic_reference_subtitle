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
 *      Driver event module
 *      Event is driver synchronized message callback mechanism.
 *      User can register multiply callback functions in one event.
 ***************************************************************************/

#ifndef _AM_EVT_H
#define _AM_EVT_H

#include "AmlogicUtil.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define AM_EVT_TYPE_BASE(no)    ((no)<<24)

/****************************************************************************
 * Error code definitions
 ****************************************************************************/

/** Error code of the event module*/
enum AM_EVT_ErrorCode
{
    AM_EVENT_ERROR_BASE=ERROR_BASE(AM_MOD_EVT),
    AM_EVENT_ERROR_NO_MEM,                  /**< Not enough memory*/
    AM_EVENT_ERROR_NOT_SUBSCRIBED,          /**< The event is not subscribed*/
    AM_EVENT_ERROR_END
};

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/** Event callback function
 * \a dev_no The device number.
 * \a event_type The event type.
 * \a param The event callback's parameter.
 * \a data User defined data registered when event subscribed.
 */
typedef void (*AM_EVT_Callback_t)(long dev_no, int event_type, void *param, void *data);

/****************************************************************************
 * Function prototypes
 ***************************************************************************/

/** Subscribe an event
 * dev_no Device number generated the event
 * event_type Event type
 * cb Callback function's pointer
 * data User defined parameter of the callback function
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int AmlogicEventSubscribe(long dev_no, int event_type, AM_EVT_Callback_t cb, void *data);

/** Unsubscribe an event
 * dev_no Device number generated the event
 * event_type Event type
 * cb Callback function's pointer
 * data User defined parameter of the callback function
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int AmlogicEventUnsubscribe(long dev_no, int event_type, AM_EVT_Callback_t cb, void *data);

/** Signal an event occurred
 * dev_no The device number generate the event
 * event_type Event type
 * \param[in] param Parameter of the event
 */
extern int AmlogicEventSignal(long dev_no, int event_type, void *param);

    extern int AmlogicEventInit();
    extern int AmlogicEventDestroy();

#ifdef __cplusplus
}
#endif

#endif

