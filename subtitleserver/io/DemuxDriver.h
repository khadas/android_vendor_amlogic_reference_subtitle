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
 *      Demux module, basic data structures definition in "linux/dvb/dmx.h"
 ***************************************************************************/

#include <linux/dvb/dmx.h>
#include <stdbool.h>

#include "AmlogicUtil.h"

#ifdef __cplusplus
extern "C"
{
#endif


/** Error code of the demux module*/
enum DEMUX_ERROR_CODE
{
    DEMUX_ERROR_BASE=ERROR_BASE(AM_MOD_DMX),
    DEMUX_ERROR_INVALID_DEV_NO,              /** Invalid device number*/
    DEMUX_ERROR_INVALID_ID,                  /** Invalid filer handle*/
    DEMUX_ERROR_BUSY,                        /** The device has already been opened*/
    DEMUX_ERROR_NOT_ALLOCATED,               /** The device has not been allocated*/
    DEMUX_ERROR_CANNOT_CREATE_THREAD,        /** Cannot create new thread*/
    DEMUX_ERROR_CANNOT_OPEN_DEV,             /** Cannot open device*/
    DEMUX_ERROR_NOT_SUPPORTED,               /** Not supported*/
    DEMUX_ERROR_NO_FREE_FILTER,              /** No free filter*/
    DEMUX_ERROR_NO_MEM,                      /** Not enough memory*/
    DEMUX_ERROR_TIMEOUT,                     /** Timeout*/
    DEMUX_ERROR_SYS,                         /** System error*/
    DEMUX_ERROR_NO_DATA,                     /** No data received*/
    DEMUX_ERROR_END
};

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/** Input source of the demux*/
typedef enum
{
    DEMUX_SRC_TS0,                           /** TS input port 0*/
    DEMUX_SRC_TS1,                           /** TS input port 1*/
    DEMUX_SRC_TS2,                           /** TS input port 2*/
    DEMUX_SRC_TS3,                           /** TS input port 3*/
    DEMUX_SRC_HIU,                           /** HIU input (memory)*/
    DEMUX_SRC_HIU1
} AmlogicDemuxSourceType;

/** Demux device open parameters*/
typedef struct
{
    bool      use_sw_filter;                    /** M_TRUE to use DVR software filters.*/
    int       dvr_fifo_no;                      /** Async fifo number if use software filters.*/
    int       dvr_buf_size;                     /** Async fifo buffer size if use software filters.*/
} AmlogicDemuxOpenParameterType;

/** Filter received data callback function
 * \a fhandle is the filter's handle.
 * \a data is the received data buffer pointer.
 * \a len is the data length.
 * If \a data == null, means timeout.
 */
typedef void (*DemuxDataCb) (int dev_no, int fhandle, const uint8_t *data, int len, void *user_data);


/****************************************************************************
 * Function prototypes
 ***************************************************************************/

/** Open a demux device
 * dev_no Demux device number
 * \param[in] para Demux device's open parameters
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int DemuxOpen(int dev_no, const AmlogicDemuxOpenParameterType *para);

/** Close a demux device
 * dev_no Demux device number
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int DemuxClose(int dev_no);

/** Allocate a filter
 * dev_no Demux device number
 * \param[out] fhandle Return the allocated filter's handle
 * \retval AM_SUCCESS On success
 * \return Error Code
 */
extern int DemuxAllocateFilter(int dev_no, int *fhandle);

/** Set a section filter's parameters
 * dev_no Demux device number
 * fhandle Filter handle
 * \param[in] params Section filter's parameters
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int DemuxSetSecFilter(int dev_no, int fhandle, const struct dmx_sct_filter_params *params);

/** Set a PES filter's parameters
 * dev_no Demux device number
 * fhandle Filter handle
 * \param[in] params PES filter's parameters
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int DemuxSetPesFilter(int dev_no, int fhandle, const struct dmx_pes_filter_params *params);

/** Release an unused filter
 * dev_no Demux device number
 * fhandle Filter handle
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int DemuxFreeFilter(int dev_no, int fhandle);

/** Start filtering
 * dev_no Demux device number
 * fhandle Filter handle
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int DemuxStartFilter(int dev_no, int fhandle);

/** Stop filtering
 * dev_no Demux device number
 * fhandle Filter handle
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int DemuxStopFilter(int dev_no, int fhandle);

/** Set the ring queue buffer size of a filter
 * dev_no Demux device number
 * fhandle Filter handle
 * size Ring queue buffer size in bytes
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int DemuxSetBufferSize(int dev_no, int fhandle, int size);

/** Get a filter's data callback function
 * dev_no Demux device number
 * fhandle Filter handle
 * \param[out] cb Return the data callback function of the filter
 * \param[out] data Return the user defined parameter of the callback function
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int DemuxGetCallback(int dev_no, int fhandle, DemuxDataCb *cb, void **data);

/** Set a filter's data callback function
 * dev_no Demux device number
 * fhandle Filter handle
 * \param[in] cb New data callback function
 * \param[in] data User defined parameter of the callback function
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int DemuxSetCallback(int dev_no, int fhandle, DemuxDataCb cb, void *data);

/** Set the demux input source
 * dev_no Demux device number
 * src Input source
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int DemuxSetSource(int dev_no, AmlogicDemuxSourceType src);

/**\cond */
/** Sync the demux data
 * dev_no Demux device number
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int DemuxSync(int dev_no);
/**\endcond */

/**
 * Get the scramble status of the AV channels in the demux
 * dev_no Demux device number
 * \param[out] dev_status Return the AV channel scramble status.
 * dev_status[0] is the video status, dev_status[1] is the audio status.
 * true means the channel is scrambled, 0 means the channel is not scrambled.
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int DemuxGetScrambleStatus(int dev_no, bool dev_status[2]);

#define DEMUX_FILTER_COUNT      (32)
#define DEMUX_WAKE_POLL_EVENT   (1)
#define DEMUX_FILTER_COUNT_EXT  DEMUX_FILTER_COUNT + DEMUX_WAKE_POLL_EVENT


#define DMX_FL_RUN_CB         (1)

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/** Demultiplexing equipment*/
typedef struct DemuxDevice DemuxDeviceType;

/** Filter*/
typedef struct DemuxFilter DemuxFilterType;

/** Filter bit mask*/
typedef uint32_t Demux_FilterMaskType;

#define DEMUX_FILTER_MASK_ISEMPTY(m)    (!(*(m)))
#define DEMUX_FILTER_MASK_CLEAR(m)      (*(m)=0)
#define DEMUX_FILTER_MASK_ISSET(m,i)    (*(m)&(((unsigned int)1<<(i)) & 0xFFFFFFFF))
#define DEMUX_FILTER_MASK_SET(m,i)      (*(m)|=((unsigned int)1<<(i)))

/** Demultiplexing device drivers*/
typedef struct
{
    int (*open)(DemuxDeviceType *dev, const AmlogicDemuxOpenParameterType *para);
    int (*close)(DemuxDeviceType *dev);
    int (*wake)(DemuxDeviceType *dev);
    int (*alloc_filter)(DemuxDeviceType *dev, DemuxFilterType *filter);
    int (*free_filter)(DemuxDeviceType *dev, DemuxFilterType *filter);
    int (*set_sec_filter)(DemuxDeviceType *dev, DemuxFilterType *filter, const struct dmx_sct_filter_params *params);
    int (*set_pes_filter)(DemuxDeviceType *dev, DemuxFilterType *filter, const struct dmx_pes_filter_params *params);
    int (*enable_filter)(DemuxDeviceType *dev, DemuxFilterType *filter, bool enable);
    int (*set_buf_size)(DemuxDeviceType *dev, DemuxFilterType *filter, int size);
    int (*poll)(DemuxDeviceType *dev, Demux_FilterMaskType *mask, int timeout);
    int (*read)(DemuxDeviceType *dev, DemuxFilterType *filter, uint8_t *buf, int *size);
    int (*set_source)(DemuxDeviceType *dev, AmlogicDemuxSourceType src);
} DemuxDriverType;

/** Section filter*/
struct DemuxFilter
{
     void *drv_data;        /** Driver private data*/
     bool used;             /** Whether this Filter has been allocated*/
     bool enable;           /** Whether this Filter device is enabled*/
     int id;                /** Filter ID*/
     DemuxDataCb cb;      /** Demultiplexing data callback function*/
     void *user_data;       /** Data callback function user parameters*/
};

/** Demultiplexing device*/
struct DemuxDevice
{
     int dev_no;                                /** device number*/
     const DemuxDriverType *drv;                /** device driver*/
     void *drv_data;                            /** Driver private data*/
     DemuxFilterType filters[DEMUX_FILTER_COUNT]; /** Filter in the device*/
     int open_count;                            /** The number of times the device has been opened*/
     bool enable_thread;                        /** The data thread is already running*/
     int flags;                                 /** Thread running status control flag*/
     pthread_t thread;                          /** data detection thread*/
     pthread_mutex_t lock;                      /** device protection mutex*/
     pthread_cond_t cond;                       /** condition variable*/
     AmlogicDemuxSourceType src;                /** TS input source*/
};

#ifdef __cplusplus
}
#endif
