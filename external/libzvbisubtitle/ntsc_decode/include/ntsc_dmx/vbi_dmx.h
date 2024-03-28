/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief demultiplexing device modules
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-05-21: create the document
 ***************************************************************************/

#ifndef _VBI_DMX_H
#define _VBI_DMX_H

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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sched.h>
#include <signal.h>
#include <android/log.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/****************************************************************************
 * Error code definitions
 ****************************************************************************/

/**\brief Demultiplexing module error code*/
enum AM_VBI_ErrorCode
{
    AM_VBI_DMX_ERROR_BASE = 0,
    AM_VBI_DMX_ERR_INVALID_DEV_NO,       /**< The device number is invalid*/
    AM_VBI_DMX_ERR_INVALID_ID,           /**< Invalid filter ID*/
    AM_VBI_DMX_ERR_BUSY,                 /**< The device has been opened*/
    AM_VBI_DMX_ERR_NOT_ALLOCATED,        /**< Device not allocated*/
    AM_VBI_DMX_ERR_CANNOT_CREATE_THREAD, /**< Could not create thread*/
    AM_VBI_DMX_ERR_CANNOT_OPEN_DEV,      /**< Could not open device*/
    AM_VBI_DMX_ERR_NOT_SUPPORTED,        /**< Unsupported operation*/
    AM_VBI_DMX_ERR_NO_FREE_FILTER,       /**< No free section filter*/
    AM_VBI_DMX_ERR_NO_MEM,               /**< Insufficient free memory*/
    AM_VBI_DMX_ERR_TIMEOUT,              /**< Timeout waiting for device data*/
    AM_VBI_DMX_ERR_SYS,                  /**< System operation error*/
    AM_VBI_DMX_ERR_NO_DATA,              /**< No data received*/
    AM_VBI_DMX_ERR_END
};
#define VBI_IOC_MAGIC 'X'
#define VBI_IOC_CC_EN       _IO (VBI_IOC_MAGIC, 0x01)
#define VBI_IOC_CC_DISABLE  _IO (VBI_IOC_MAGIC, 0x02)
#define VBI_IOC_SET_FILTER  _IOW(VBI_IOC_MAGIC, 0x03, int)
#define VBI_IOC_S_BUF_SIZE  _IOW(VBI_IOC_MAGIC, 0x04, int)
#define VBI_IOC_START       _IO (VBI_IOC_MAGIC, 0x05)
#define VBI_IOC_STOP        _IO (VBI_IOC_MAGIC, 0x06)
#define DECODER_VBI_VBI_SIZE                0x1000

#define VBI_DEV_NO  0

#ifndef AM_SUCCESS
#define AM_SUCCESS     (0)
#endif

#ifndef AM_FAILURE
#define AM_FAILURE     (-1)
#endif

#ifndef AM_TRUE
#define AM_TRUE        (1)
#endif

#ifndef AM_FALSE
#define AM_FALSE       (0)
#endif


#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG    "VBI_DEMUX"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)


//#define AM_DEBUG  printf
#define AM_DEBUG  LOGI


typedef int            AM_ErrorCode_t;

/**************************************************** ****************************
 * Type definitions
 **************************************************** ************************/

/**\brief Demultiplex device input source*/
typedef enum
{
    DUAL_FD1, /**< vbi input 0*/
    DUAL_FD2, /**< vbi input 1*/
} AM_VBI_DMX_Source_t;

/**\brief Demultiplexing device enable parameters*/
typedef struct
{
    int foo;
} AM_VBI_DMX_OpenPara_t;

/**\brief data callback function
 * data is the data buffer pointer, and len is the data length. If data==NULL, it means that demux has timed out to receive data.
 */
typedef void (*AM_DMX_DataCb)(int dev_no, int fhandle, const uint8_t *data, int len, void *user_data);

/**************************************************** ****************************
 * Function prototypes
 **************************************************** ************************/

/**\brief Open the demultiplexing device
 * \param dev_no demultiplexing device number
 * \param[in] para Demultiplexing device startup parameters
 * \return
 * - AM_SUCCESS success
 * - other value error code (see am_dmx.h)
 */
extern AM_ErrorCode_t AM_NTSC_DMX_Open(int dev_no, const AM_VBI_DMX_OpenPara_t *para);

/**\brief close the demultiplexing device
 * \param dev_no demultiplexing device number
 * \return
 * - AM_SUCCESS success
 * - other value error code (see am_dmx.h)
 */
extern AM_ErrorCode_t AM_NTSC_DMX_Close(int dev_no);

/**\brief assigns a filter
 * \param dev_no demultiplexing device number
 * \param[out] fhandle returns the filter handle
 * \return
 * - AM_SUCCESS success
 * - other value error code (see am_dmx.h)
 */
extern AM_ErrorCode_t AM_NTSC_DMX_AllocateFilter(int dev_no, int *fhandle);

/**\brief set Section filter
 * \param dev_no demultiplexing device number
 * \param fhandle filter handle
 * \param[in] params Section filter parameters
 * \return
 * - AM_SUCCESS success
 * - other value error code (see am_dmx.h)
 */
//extern AM_ErrorCode_t AM_DMX_SetSecFilter(int dev_no, int fhandle, const struct dmx_sct_filter_params *params);

/**\brief set PES filter
 * \param dev_no demultiplexing device number
 * \param fhandle filter handle
 * \param[in] params PES filter parameters
 * \return
 * - AM_SUCCESS success
 * - other value error code (see am_dmx.h)
 */
//extern AM_ErrorCode_t AM_DMX_SetPesFilter(int dev_no, int fhandle, const struct dmx_pes_filter_params *params);

/**\brief release a filter
 * \param dev_no demultiplexing device number
 * \param fhandle filter handle
 * - AM_SUCCESS success
 * - other value error code (see am_dmx.h)
 */
//extern AM_ErrorCode_t AM_DMX_FreeFilter(int dev_no, int fhandle);

/**\brief start a filter
 * \param dev_no demultiplexing device number
 * \param fhandle filter handle
 * \return
 * - AM_SUCCESS success
 * - other value error code (see am_dmx.h)
 */
extern AM_ErrorCode_t AM_NTSC_DMX_StartFilter(int dev_no, int fhandle);

/**\brief stop a filter
 * \param dev_no demultiplexing device number
 * \param fhandle filter handle
 * \return
 * - AM_SUCCESS success
 * - other value error code (see am_dmx.h)
 */
//extern AM_ErrorCode_t AM_DMX_StopFilter(int dev_no, int fhandle);

/**\brief Set a filter buffer size
 * \param dev_no demultiplexing device number
 * \param fhandle filter handle
 * \param size buffer size
 * \return
 * - AM_SUCCESS success
 * - other value error code (see am_dmx.h)
 */
extern AM_ErrorCode_t AM_NTSC_DMX_SetBufferSize(int dev_no, int fhandle, int size);

/**\brief Get the callback function and user parameters corresponding to a filter
 * \param dev_no demultiplexing device number
 * \param fhandle filter handle
 * \param[out] cb returns the callback function corresponding to the filter
 * \param[out] data returns user parameters
 * \return
 * - AM_SUCCESS success
 * - other value error code (see am_dmx.h)
 */
//extern AM_ErrorCode_t AM_DMX_GetCallback(int dev_no, int fhandle, AM_DMX_DataCb *cb, void **data);

/**\brief Set the callback function and user parameters corresponding to a filter
 * \param dev_no demultiplexing device number
 * \param fhandle filter handle
 * \param[in] cb callback function
 * \param[in] data The user parameter of the callback function
 * \return
 * - AM_SUCCESS success
 * - other value error code (see am_dmx.h)
 */
extern AM_ErrorCode_t AM_NTSC_DMX_SetCallback(int dev_no, int fhandle, AM_DMX_DataCb cb, void *data);

/**\brief Set the input source of the demux device
 * \param dev_no demultiplexing device number
 * \param src input source
 * \return
 * - AM_SUCCESS success
 * - other value error code (see am_dmx.h)
 */
//extern AM_ErrorCode_t AM_DMX_SetSource(int dev_no, AM_DMX_Source_t src);

/**\brief DMX synchronization, which can be used to wait for the callback function to finish executing
 * \param dev_no demultiplexing device number
 * \return
 * - AM_SUCCESS success
 * - other value error code (see am_dmx.h)
 */
//extern AM_ErrorCode_t AM_DMX_Sync(int dev_no);

#ifdef __cplusplus
}
#endif

#endif

