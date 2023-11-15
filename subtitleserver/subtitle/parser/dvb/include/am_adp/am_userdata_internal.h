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
 * \brief USER DATA module
 *
 * \author Amlogic
 * \date 2013-3-13: create the document
 ***************************************************************************/

#ifndef _AM_USERDATA_INTERNAL_H
#define _AM_USERDATA_INTERNAL_H

#include <am_userdata.h>
#include <am_util.h>
#include <am_thread.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define USERDATA_BUF_SIZE (5*1024)

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief USERDATA device*/
typedef struct AM_USERDATA_Device AM_USERDATA_Device_t;


typedef struct
{
    uint8_t           *data;
    ssize_t           size;
    ssize_t           pread;
    ssize_t           pwrite;
    int               error;

    pthread_cond_t    cond;
}AM_USERDATA_RingBuffer_t;

/**\brief USERDATA devicedriver*/
typedef struct
{
    AM_ErrorCode_t (*open)(AM_USERDATA_Device_t *dev, const AM_USERDATA_OpenPara_t *para);
    AM_ErrorCode_t (*close)(AM_USERDATA_Device_t *dev);
    AM_ErrorCode_t (*set_mode)(AM_USERDATA_Device_t *dev, int mode);
    AM_ErrorCode_t (*get_mode)(AM_USERDATA_Device_t *dev, int *mode);
    AM_ErrorCode_t (*set_param)(AM_USERDATA_Device_t *dev, int para);
} AM_USERDATA_Driver_t;

/**\brief USERDATA device*/
struct AM_USERDATA_Device
{
    int                 dev_no;  /**< 设备号*/
    const AM_USERDATA_Driver_t *drv;  /**< 设备driver*/
    void               *drv_data;/**< driver private data*/
    int                 open_cnt;   /**< 设备打开计数*/
    pthread_mutex_t     lock;    /**< 设备保护互斥体*/
    AM_USERDATA_RingBuffer_t pkg_buf;

    int (*write_package)(AM_USERDATA_Device_t *dev, const uint8_t *buf, size_t size);

};



/****************************************************************************
 * Function prototypes
 ***************************************************************************/


#ifdef __cplusplus
}
#endif

#endif

