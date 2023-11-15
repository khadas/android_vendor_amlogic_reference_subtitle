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
 * \brief Subtitle module (version 2)
 *
 * \author Yan Yan <yan.yan@amlogic.com>
 * \date 2019-07-22: create the document
 ***************************************************************************/


#ifndef _AM_SCTE27_H
#define _AM_SCTE27_H

#include "am_types.h"
#include "am_osd.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Type definitions
 ***************************************************************************/
#define SCTE27_TID 0xC6

enum AM_SCTE27_ErrorCode
{
    AM_SCTE27_ERROR_BASE=AM_ERROR_BASE(AM_MOD_SCTE27),
    AM_SCTE27_ERR_INVALID_PARAM,   /**< Invalid parameter*/
    AM_SCTE27_ERR_INVALID_HANDLE,  /**< Invalid handle*/
    AM_SCTE27_ERR_NOT_SUPPORTED,   /**< not support action*/
    AM_SCTE27_ERR_CREATE_DECODE,   /**< open subtitle decode error*/
    AM_SCTE27_ERR_SET_BUFFER,      /**< set pes buffer error*/
    AM_SCTE27_ERR_NO_MEM,                  /**< out of memmey*/
    AM_SCTE27_ERR_CANNOT_CREATE_THREAD,    /**< cannot creat thread*/
    AM_SCTE27_ERR_NOT_RUN,            /**< thread run error*/
    AM_SCTE27_INIT_DISPLAY_FAILED,    /**< init display error*/
    AM_SCTE27_PACKET_INVALID,
    AM_SCTE27_ERR_END
};

enum AM_SCTE27_Decoder_Error
{
    AM_SCTE27_Decoder_Error_LoseData,
    AM_SCTE27_Decoder_Error_InvalidData,
    AM_SCTE27_Decoder_Error_TimeError,
    AM_SCTE27_Decoder_Error_END
};


typedef void*      AM_SCTE27_Handle_t;
typedef void (*AM_SCTE27_DrawBegin_t)(AM_SCTE27_Handle_t handle);
typedef void (*AM_SCTE27_DrawEnd_t)(AM_SCTE27_Handle_t handle);
typedef void (*AM_SCTE27_LangCb_t)(AM_SCTE27_Handle_t handle, char* buffer, int size);
typedef void (*AM_SCTE27_ReportError)(AM_SCTE27_Handle_t handle, int error);
typedef void (*AM_SCTE27_PicAvailable)(AM_SCTE27_Handle_t handle);
typedef void (*AM_SCTE27_UpdateSize)(AM_SCTE27_Handle_t handle, int width, int height);


typedef struct
{
    AM_SCTE27_DrawBegin_t   draw_begin;
    AM_SCTE27_DrawEnd_t     draw_end;
    AM_SCTE27_LangCb_t  lang_cb;
    AM_SCTE27_ReportError report;
    AM_SCTE27_PicAvailable report_available;
    AM_SCTE27_UpdateSize update_size;
    uint8_t         **bitmap;         /**< draw bitmap buffer*/
    int              pitch;          /**< the length of draw bitmap buffer per line*/
    int width;
    int height;
    int media_sync;
    void                    *user_data;      /**< user private data*/
    AM_Bool_t hasReportAvailable;
    AM_Bool_t hasReportLang;
    int lastWidth;
    int lastHeight;
}AM_SCTE27_Para_t;

AM_ErrorCode_t AM_SCTE27_Create(AM_SCTE27_Handle_t *handle, AM_SCTE27_Para_t *para);
AM_ErrorCode_t AM_SCTE27_Destroy(AM_SCTE27_Handle_t handle);
void*          AM_SCTE27_GetUserData(AM_SCTE27_Handle_t handle);
AM_ErrorCode_t AM_SCTE27_Decode(AM_SCTE27_Handle_t handle, const uint8_t *buf, int size);
AM_ErrorCode_t AM_SCTE27_Start(AM_SCTE27_Handle_t handle);
AM_ErrorCode_t AM_SCTE27_Stop(AM_SCTE27_Handle_t handle);

#ifdef __cplusplus
}
#endif

#endif

