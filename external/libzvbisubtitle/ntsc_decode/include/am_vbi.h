/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief Teletext module(version 2)
 *
 * \author kui.zhang <kui.zhang@amlogic.com>
 * \date 2012-09-03: create the document
 ***************************************************************************/

#ifndef _AM_NTSC_CC_H
#define _AM_NTSC_CC_H


#include <unistd.h>
#include "misc.h"
#include "trigger.h"
#include "format.h"
#include "lang.h"
#include "hamm.h"
#include "tables.h"
#include "vbi.h"
#include <android/log.h>
#include <am_xds.h>
#include "vbi_dmx.h"
#ifdef __cplusplus
extern "C"
{
#endif

    enum AM_CC_ErrorCode
    {
        AM_CC_ERROR_BASE = 0,
        AM_CC_ERR_INVALID_PARAM,        /**< parameter invalid */
        AM_CC_ERR_INVALID_HANDLE,       /**< Invalid handle*/
        AM_CC_ERR_NOT_SUPPORTED,        /**< Unsupported operation*/
        AM_CC_ERR_CREATE_DECODE,        /**< Failed to open cc decoder*/
        AM_CC_ERR_OPEN_PES,             /**< Failed to open PES channel*/
        AM_CC_ERR_SET_BUFFER,           /**< Failed to set PES buffer*/
        AM_CC_ERR_NO_MEM,               /**< Insufficient free memory*/
        AM_CC_ERR_CANNOT_CREATE_THREAD, /**< Could not create thread*/
        AM_CC_ERR_NOT_RUN,              /**< Could not create thread */
        AM_CC_INIT_DISPLAY_FAILED,      /**< Failed to initialize display screen*/
        AM_CC_ERR_END
    };

    /****************************************************************************
 * Type definitions
 ***************************************************************************/
    /**\brief Teletext parser handle*/
    typedef void *AM_VBI_Handle_t;

    /**\brief start drawing*/
    typedef void (*AM_VBI_CC_DrawBegin_t)(AM_VBI_Handle_t handle);

    /**\brief end drawing*/
    typedef void (*AM_VBI_CC_DrawEnd_t)(AM_VBI_Handle_t handle);

    /**\brief notify xds data*/
    typedef void (*AM_VBI_xds_callback_t)(AM_VBI_Handle_t handle, vbi_xds_subclass_program xds_program, vbi_program_info prog_info);

    typedef struct
    {
        AM_VBI_CC_DrawBegin_t draw_begin; /**< start drawing*/
        AM_VBI_CC_DrawEnd_t draw_end;     /**< end drawing*/
        vbi_bool is_subtitle;             /**< Is it a subtitle*/
        uint8_t *bitmap;                  /**< drawing buffer*/
        int pitch;                        /**< number of bytes per line in the drawing buffer*/
        void *user_data;                  /**< user defined data*/
}AM_VBI_CC_Para_t;

 typedef struct
{
    AM_VBI_xds_callback_t        xds_callback;      /**< notify to user */
}AM_VBI_XDS_Para_t;



typedef enum {
    VBI_CC1 = 0x01,
    VBI_CC2,
    VBI_CC3,
    VBI_CC4,
    VBI_TT1,
    VBI_TT2,
    VBI_TT3,
    VBI_TT4,

} VBI_CC_TYPE;


extern vbi_bool AM_VBI_CC_Create(AM_VBI_Handle_t *handle, AM_VBI_CC_Para_t *para);

extern  AM_ErrorCode_t AM_VBI_CC_Start(AM_VBI_Handle_t handle);

extern  AM_ErrorCode_t AM_VBI_CC_Stop(AM_VBI_Handle_t handle);

extern vbi_bool
decode_vbi  (int dev_no, int fid, const uint8_t *data, int len, void *user_data);

extern void* AM_VBI_CC_GetUserData(AM_VBI_Handle_t handle);

extern vbi_bool AM_VBI_CC_set_type(AM_VBI_Handle_t handle,VBI_CC_TYPE cc_type);  //*cc_type caption 1-4, text 1-4, garbage */

extern vbi_bool AM_VBI_CC_set_status(AM_VBI_Handle_t handle,vbi_bool flag);

extern vbi_bool AM_VBI_XDS_Create(AM_VBI_Handle_t *handle,AM_VBI_XDS_Para_t *para);



#ifdef __cplusplus
}
#endif



#endif
