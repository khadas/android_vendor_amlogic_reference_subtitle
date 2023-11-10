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
 *      MPEG user data device module
 ***************************************************************************/

#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>

#include "AmlogicUtil.h"
#include "AmlogicEvent.h"
#include "AmlogicTime.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define USERDATA_BUF_SIZE (5*1024)
#define USERDATA_DEV_COUNT      (1)

/** Error code of the user data module*/
enum USERDATA_ERROR_CODE {
    USERDATA_ERROR_BASE=ERROR_BASE(AM_MOD_USERDATA),
    USERDATA_ERROR_INVALID_ARG,                            /** Invalid argument*/
    USERDATA_ERROR_INVALID_DEV_NO,                         /** Invalid device number*/
    USERDATA_ERROR_BUSY,                                   /** The device is busy*/
    USERDATA_ERROR_CANNOT_OPEN_DEV,                        /** Cannot open the device*/
    USERDATA_ERROR_NOT_SUPPORTED,                          /** Not supported*/
    USERDATA_ERROR_NO_MEM,                                 /** Not enough memory*/
    USERDATA_ERROR_TIMEOUT,                                /** Timeout*/
    USERDATA_ERROR_SYS,                                    /** System error*/
    USERDATA_ERROR_END
};

enum USERDATA_EVEN_TTYPE {
    USERDATA_EVENT_BASE = AM_EVT_TYPE_BASE(AM_MOD_USERDATA),
    USERDATA_EVENT_AFD,
};

enum USERDATA_MODE {
    USERDATA_MODE_CC = 0x1,
    USERDATA_MODE_AFD = 0x2,
};

/** MPEG user data device open parameters*/
typedef struct {
    int foo;
    int vfmt;
    int cc_default_stop;
    int playerid;
    int mediasyncid;
} UserdataOpenParaType;

typedef struct {
    uint8_t  af_flag:1;
    uint8_t  af     :4;
    //uint16_t reserved;
    //uint32_t pts;
} UserdataAFDType;

/** USERDATA device*/
typedef struct UserdataDevice UserdataDeviceType;

typedef struct {
    uint8_t           *data;
    ssize_t           size;
    ssize_t           pread;
    ssize_t           pwrite;
    int               error;

    pthread_cond_t    cond;
}UserdataRingBufferType;

/** USERDATA devicedriver*/
typedef struct {
    int (*open)(UserdataDeviceType *dev, const UserdataOpenParaType *para);
    int (*close)(UserdataDeviceType *dev);
    int (*set_mode)(UserdataDeviceType *dev, int mode);
    int (*get_mode)(UserdataDeviceType *dev, int *mode);
    int (*set_param)(UserdataDeviceType *dev, int para);
} UserdataDriverType;

/** USERDATA device*/
struct UserdataDevice {
     int dev_no;                            /** device number*/
     const UserdataDriverType *drv;       /** device driver*/
     void *drv_data;                        /** driver private data*/
     int open_cnt;                          /** device open count*/
     pthread_mutex_t lock;                  /** device protection mutex*/
     UserdataRingBufferType pkg_buf;

     int (*write_package)(UserdataDeviceType *dev, const uint8_t *buf, size_t size);

};


/** Open the MPEG user data device
 * dev_no Device number
 * \param[in] para Device open parameters
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int UserdataOpen(int dev_no, const UserdataOpenParaType *para);

/** Close the MPEG user data device
 * dev_no Device number
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int UserdataClose(int dev_no);


extern int UserdataSetParameters(int dev_no, int para);

/** Read MPEG user data from the device
 * dev_no Device number
 * \param[out] buf Output buffer to store the user data
 * size  Buffer length in bytes
 * timeout_ms Timeout time in milliseconds
 * \return Read data length in bytes
 */
extern int UserdataRead(int dev_no, uint8_t *buf, int size, int timeout_ms);

extern int UserdataSetMode(int dev_no, int mode);
extern int UserdataGetMode(int dev_no, int *mode);

#ifdef __cplusplus
}
#endif
