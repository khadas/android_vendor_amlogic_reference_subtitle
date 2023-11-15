/*
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
 */

#ifndef __RINGBUFFER_H_
#define __RINGBUFFER_H_

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/************************************************
 * TYPES
 ************************************************/
typedef enum {
       RBUF_MODE_BLOCK = 0,
       RBUF_MODE_NBLOCK
 } rbuf_mode_t;

typedef void * rbuf_handle_t;

/************************************************
 * FUNCTIONS
 ************************************************/

rbuf_handle_t ringbuffer_create(int size, const char*name);
void ringbuffer_free(rbuf_handle_t handle);

int ringbuffer_read(rbuf_handle_t handle, char *dst, int count, rbuf_mode_t mode);
int ringbuffer_write(rbuf_handle_t handle, const char *src, int count, rbuf_mode_t mode);
int ringbuffer_peek(rbuf_handle_t handle, char *dst, int count, rbuf_mode_t mode);

int ringbuffer_cleanreset(rbuf_handle_t handle);

int ringbuffer_read_avail(rbuf_handle_t handle);
int ringbuffer_write_avail(rbuf_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* BUFFER_H_ */
