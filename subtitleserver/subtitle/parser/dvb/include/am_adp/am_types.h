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
 * \brief Basic datatypes
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-05-19: create the document
 ***************************************************************************/

#ifndef _AM_TYPES_H
#define _AM_TYPES_H

#include <stdint.h>
#include <sys/types.h>
#include "pthread.h"

#ifdef __cplusplus
extern "C"
{
#endif


/*****************************************************************************
* Global Definitions
*****************************************************************************/

/**\brief Boolean value*/
typedef uint8_t        AM_Bool_t;


/**\brief Error code of the function result,
 * Low 24 bits store the error number.
 * High 8 bits store the module's index.
 */
typedef int            AM_ErrorCode_t;

/**\brief The module's index */
enum AM_MOD_ID
{
    AM_MOD_EVT,    /**< Event module*/
    AM_MOD_DMX,    /**< Demux module*/
    AM_MOD_DVR,    /**< DVR module*/
    AM_MOD_NET,    /**< Network manager module*/
    AM_MOD_OSD,    /**< OSD module*/
    AM_MOD_AV,     /**< AV decoder module*/
    AM_MOD_AOUT,   /**< Audio output device module*/
    AM_MOD_VOUT,   /**< Video output device module*/
    AM_MOD_SMC,    /**< Smartcard module*/
    AM_MOD_INP,    /**< Input device module*/
    AM_MOD_FEND,   /**< DVB frontend device module*/
    AM_MOD_DSC,    /**< Descrambler device module*/
    AM_MOD_CFG,    /**< Configure file manager module*/
    AM_MOD_SI,     /**< SI decoder module*/
    AM_MOD_SCAN,   /**< Channel scanner module*/
    AM_MOD_EPG,    /**< EPG scanner module*/
    AM_MOD_IMG,    /**< Image loader module*/
    AM_MOD_FONT,   /**< Font manager module*/
    AM_MOD_DB,     /**< Database module*/
    AM_MOD_GUI,    /**< GUI module*/
    AM_MOD_REC,    /**< Recorder module*/
    AM_MOD_TV,     /**< TV manager module*/
    AM_MOD_SUB,    /**< Subtitle module*/
    AM_MOD_SUB2,   /**< Subtitle(version 2) module*/
    AM_MOD_TT,     /**< Teletext module*/
    AM_MOD_TT2,    /**< Teletext(version 2) module*/
    AM_MOD_FEND_DISEQCCMD,/**< Diseqc command module*/
    AM_MOD_FENDCTRL, /**< DVB frontend high level control module*/
    AM_MOD_PES,    /**< PES parser module*/
    AM_MOD_CAMAN,  /**< CA manager module*/
    AM_MOD_CI,     /**< DVB-CI module*/
    AM_MOD_USERDATA, /**< MPEG user data reader device module*/
    AM_MOD_CC,     /**< Close caption module*/
    AM_MOD_AD,     /**< Audio description module*/
    AM_MOD_UPD,    /**< Uploader module*/
    AM_MOD_TFILE, /*File wrapper module*/
    AM_MOD_SCTE27,
    AM_MOD_MAX
};

/**\brief Get the error code base of each module
 * \param _mod The module's index
 */
#define AM_ERROR_BASE(_mod)    ((_mod)<<24)

#ifndef AM_SUCCESS
/**\brief Function result: Success*/
#define AM_SUCCESS     (0)
#endif

#ifndef AM_FAILURE
/**\brief Function result: Unknown error*/
#define AM_FAILURE     (-1)
#endif

#ifndef AM_TRUE
/**\brief Boolean value: true*/
#define AM_TRUE        (1)
#endif

#ifndef AM_FALSE
/**\brief Boolean value: false*/
#define AM_FALSE       (0)
#endif

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#ifdef __cplusplus
}
#endif

#endif

