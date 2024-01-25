/**************************************************** ****************************
 * Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief demux device
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-05-21: create the document
 **************************************************** ************************/

#ifndef _AM_VBI_INTERNAL_H
#define _AM_VBI_INTERNAL_H

#include <vbi_dmx.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#ifdef __cplusplus
extern "C"
{
#endif

    typedef int vbi_bool;
    /**************************************************** ****************************
 * Macro definitions
 **************************************************** ************************/

#define DMX_FILTER_COUNT (1)

#define DMX_DRIVER_LENGTH (32)

#define DMX_FL_RUN_CB (1)

    /**************************************************** ****************************
 * Type definitions
 **************************************************** ************************/

    /**\brief demultiplexing device*/
    typedef struct AM_VBI_Device AM_VBI_Device_t;

    /**\brief filter*/
    typedef struct AM_VBI_Filter AM_VBI_Filter_t;

    /**\brief filter bit mask */
    typedef uint32_t AM_VBI_FilterMask_t;

#define AM_DMX_FILTER_MASK_ISEMPTY(m) (!(*(m)))
#define AM_DMX_FILTER_MASK_CLEAR(m) (*(m) = 0)
#define AM_DMX_FILTER_MASK_ISSET(m, i) (*(m) & (1 << (i)))
#define AM_DMX_FILTER_MASK_SET(m, i) (*(m) |= (1 << (i)))

    /**\brief demultiplexing device driver*/
    typedef struct
    {
        AM_ErrorCode_t (*open)(AM_VBI_Device_t *dev, const AM_VBI_DMX_OpenPara_t *para);
        AM_ErrorCode_t (*close)(AM_VBI_Device_t *dev);
        AM_ErrorCode_t (*alloc_filter)(AM_VBI_Device_t *dev, AM_VBI_Filter_t *filter);
        AM_ErrorCode_t (*free_filter)(AM_VBI_Device_t *dev, AM_VBI_Filter_t *filter);
        AM_ErrorCode_t (*enable_filter)(AM_VBI_Device_t *dev, AM_VBI_Filter_t *filter, vbi_bool enable);
        AM_ErrorCode_t (*set_buf_size)(AM_VBI_Device_t *dev, AM_VBI_Filter_t *filter, int size);
        AM_ErrorCode_t (*poll)(AM_VBI_Device_t *dev, AM_VBI_FilterMask_t *mask, int timeout);
        AM_ErrorCode_t (*read)(AM_VBI_Device_t *dev, AM_VBI_Filter_t *filter, uint8_t *buf, int *size);
        AM_ErrorCode_t (*set_source)(AM_VBI_Device_t *dev, AM_VBI_DMX_Source_t src);
    } AM_VBI_Driver_t;

    /**\brief Section filter*/
    struct AM_VBI_Filter
    {
        void *drv_data;   /**< driver private data*/
        vbi_bool used;    /**<Whether this Filter has been allocated*/
        vbi_bool enable;  /**<Whether this Filter device is enabled*/
        int id;           /**< Filter ID*/
        AM_DMX_DataCb cb; /**< Demultiplexing data callback function*/
        void *user_data;  /**< data callback function user parameters*/
    };

    /**\brief demultiplexing device*/
    struct AM_VBI_Device
    {
        int dev_no;                                /**< device number*/
        const AM_VBI_Driver_t *drv;                /**< device driver*/
        void *drv_data;                            /**<Driver private data*/
        AM_VBI_Filter_t filters[DMX_FILTER_COUNT]; /**< Filter in device*/
        vbi_bool opened;                          /**< device has been opened */
        vbi_bool enable_thread;                    /**< data thread is already running*/
        int flags;                                 /**< thread running state control flags */
        pthread_t thread;                          /**< data detection thread*/
        pthread_mutex_t lock;                      /**< device protection mutex */
        pthread_cond_t cond;                       /**< condition variable*/
        AM_VBI_DMX_Source_t src;                   /**< TS input source*/
    };

    /**************************************************** ****************************
 * Function prototypes
 **************************************************** ************************/

#ifdef __cplusplus
}
#endif

#endif