#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
/**\file
 * \brief AMLogic adaptor layer global lock
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-07-05: create the document
 ***************************************************************************/

#include "am_adp_internal.h"

/****************************************************************************
 * Data definitions
 ***************************************************************************/

pthread_mutex_t am_gAdpLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t am_gHwDmxLock = PTHREAD_MUTEX_INITIALIZER;

