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
 * \brief aml config
 * \author amlogic
 * \date 2017-04-25: create the document
 ***************************************************************************/

#ifndef _AM_CONFIG_H
#define _AM_CONFIG_H

#ifndef CONFIG_AMLOGIC_DVB_COMPAT
/**\brief int value: 1*/
#define CONFIG_AMLOGIC_DVB_COMPAT       (1)
#endif

#ifndef __DVB_CORE__
/**\brief int value: 1*/
#define __DVB_CORE__       (1)
#endif

#include <linux/dvb/frontend.h>

typedef struct atv_status_s atv_status_t;
typedef struct tuner_param_s tuner_param_t;
//typedef enum fe_ofdm_mode fe_ofdm_mode_t;

#define fe_ofdm_mode_t enum fe_ofdm_mode
#endif

