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
 * \brief PES packet parser module
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2012-08-01: create the document
 ***************************************************************************/


#ifndef _AM_PES_H
#define _AM_PES_H

#include "am_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Type definitions
 ***************************************************************************/


/**\brief PES parser module's error code*/
enum AM_PES_ErrorCode
{
    AM_PES_ERROR_BASE=AM_ERROR_BASE(AM_MOD_PES),
    AM_PES_ERR_INVALID_PARAM,   /**< Invalid parameter*/
    AM_PES_ERR_INVALID_HANDLE,  /**< Invalid handle*/
    AM_PES_ERR_NO_MEM,          /**< Not enough memory*/
    AM_PES_ERR_END
};

/**\brief PES parser handle*/
typedef void* AM_PES_Handle_t;

/**\brief PES packet callback function
 * \a handle The PES parser handle.
 * \a buf The PES packet.
 * \a size The packet size in bytes.
 */
typedef void (*AM_PES_PacketCb_t)(AM_PES_Handle_t handle, uint8_t *buf, int size);

/**\brief PES parser parameters*/
typedef struct
{
    AM_PES_PacketCb_t packet;       /**< PES packet callback function*/
    AM_Bool_t         payload_only; /**< Only read PES payload*/
    void             *user_data;    /**< User defined data*/
    int               afmt;         /**< audio fmt*/
}AM_PES_Para_t;

/**\brief Create a new PES parser
 * \param[out] handle Return the new PES parser handle
 * \param[in] para PES parser parameters
 * \retval AM_SUCCESS On success
 * \return Error code
 */
AM_ErrorCode_t AM_PES_Create(AM_PES_Handle_t *handle, AM_PES_Para_t *para);

/**\brief Release an unused PES parser
 * \param handle The PES parser handle
 * \retval AM_SUCCESS On success
 * \return Error code
 */
AM_ErrorCode_t AM_PES_Destroy(AM_PES_Handle_t handle);

/**\brief Parse the PES data
 * \param handle PES parser handle
 * \param[in] buf PES data's buffer
 * \param size Data in the buffer
 * \retval AM_SUCCESS On success
 * \return Error code
 */
AM_ErrorCode_t AM_PES_Decode(AM_PES_Handle_t handle, uint8_t *buf, int size);

/**\brief Get the user defined data of the PES parser
 * \param handle PES parser handle
 * \return The user defined data
 */
void*          AM_PES_GetUserData(AM_PES_Handle_t handle);

#ifdef __cplusplus
}
#endif



#endif
