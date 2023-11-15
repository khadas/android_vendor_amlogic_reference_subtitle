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

#ifndef __SUBTITLE_STREAM_UTILS_H__
#define __SUBTITLE_STREAM_UTILS_H__



//TODO: move to utils directory

/**
 *  return the literal value of ascii printed char
 */
static inline int subAscii2Value(char ascii) {
    return ascii - '0';
}

/**
 *  Peek the buffer data, consider it to int 32 value.
 *
 *  Peek, do not affect the buffer contents and pointer
 */
static inline int subPeekAsInt32(const char *buffer) {
    int value = 0;
    for (int i = 0; i < 4; i++) {
        value <<= 8;
        value |= buffer[i];
    }
    return value;
}

/**
 *  Peek the buffer data, consider it to int 64 value.
 *
 *  Peek, do not affect the buffer contents and pointer
 */

static inline uint64_t subPeekAsInt64(const char *buffer) {
    uint64_t value = 0;
    for (uint64_t i = 0; i < 8; i++) {
        value <<= 8;
        value |= buffer[i];
    }
    return value;
}

#endif
