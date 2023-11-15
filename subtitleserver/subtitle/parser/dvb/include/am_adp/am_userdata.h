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
 /**\file am_userdata.h
 * \brief MPEG user data device module
 *
 * \author Amlogic
 * \date 2013-3-13: create the document
 ***************************************************************************/

#ifndef _AM_USERDATA_H
#define _AM_USERDATA_H

#include <unistd.h>
#include <sys/types.h>
#include "am_types.h"
#include "am_evt.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief Error code of the user data module*/
enum AM_USERDATA_ErrorCode
{
    AM_USERDATA_ERROR_BASE=AM_ERROR_BASE(AM_MOD_USERDATA),
    AM_USERDATA_ERR_INVALID_ARG,             /**< Invalid argument*/
    AM_USERDATA_ERR_INVALID_DEV_NO,          /**< Invalid device number*/
    AM_USERDATA_ERR_BUSY,                    /**< The device is busy*/
    AM_USERDATA_ERR_CANNOT_OPEN_DEV,         /**< Cannot open the device*/
    AM_USERDATA_ERR_NOT_SUPPORTED,           /**< Not supported*/
    AM_USERDATA_ERR_NO_MEM,                  /**< Not enough memory*/
    AM_USERDATA_ERR_TIMEOUT,                 /**< Timeout*/
    AM_USERDATA_ERR_SYS,                     /**< System error*/
    AM_USERDATA_ERR_END
};

enum AM_USERDATA_EventType
{
    AM_USERDATA_EVT_BASE = AM_EVT_TYPE_BASE(AM_MOD_USERDATA),
    AM_USERDATA_EVT_AFD,
};

enum AM_USERDATA_Mode
{
    AM_USERDATA_MODE_CC = 0x1,
    AM_USERDATA_MODE_AFD = 0x2,
};

/**\brief MPEG user data device open parameters*/
typedef struct
{
    int    foo;
    int vfmt;
    int cc_default_stop;
    int playerid;
    int mediasyncid;
} AM_USERDATA_OpenPara_t;

typedef struct
{
    uint8_t         :6;
    uint8_t  af_flag:1;
    uint8_t         :1;
    uint8_t  af     :4;
    uint8_t         :4;
    //uint16_t reserved;
    //uint32_t pts;
} AM_USERDATA_AFD_t;

/****************************************************************************
 * Function prototypes
 ***************************************************************************/

/**\brief Open the MPEG user data device
 * \param dev_no Device number
 * \param[in] para Device open parameters
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern AM_ErrorCode_t AM_USERDATA_Open(int dev_no, const AM_USERDATA_OpenPara_t *para);

/**\brief Close the MPEG user data device
 * \param dev_no Device number
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern AM_ErrorCode_t AM_USERDATA_Close(int dev_no);


extern AM_ErrorCode_t AM_USERDATA_SetParameters(int dev_no, int para);

/**\brief Read MPEG user data from the device
 * \param dev_no Device number
 * \param[out] buf Output buffer to store the user data
 * \param size  Buffer length in bytes
 * \param timeout_ms Timeout time in milliseconds
 * \return Read data length in bytes
 */
extern int AM_USERDATA_Read(int dev_no, uint8_t *buf, int size, int timeout_ms);

extern AM_ErrorCode_t AM_USERDATA_SetMode(int dev_no, int mode);
extern AM_ErrorCode_t AM_USERDATA_GetMode(int dev_no, int *mode);

#ifdef __cplusplus
}
#endif

#endif

