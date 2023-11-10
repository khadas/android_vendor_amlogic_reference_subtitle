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

#pragma once

enum {
    eTypeSubtitleTotal      = 0x53544F54,  // 'STOT'
    eTypeSubtitleStartPts   = 0x53505453,  // 'SPTS'
    eTypeSubtitleRenderTime = 0x53524454,  // 'SRDT'
    eTypeSubtitleType       = 0x53545950,  // 'STYP'

    eTypeSubtitleTypeString = 0x54505352,  // 'TPSR'
    eTypeSubtitleLangString = 0x4C475352,  // 'LGSR'
    eTypeSubtitleData       = 0x504C4454,  // 'PLDT'

    eTypeSubtitleResetServ  = 0x43545244,  // 'CDRT'
    eTypeSubtitleExitServ   = 0x43444558   // 'CDEX'
};


struct IpcPackageHeader {
    int syncWord;
    int sessionId;
    int magicWord;
    int dataSize;
    int pkgType;
};

const static unsigned int START_FLAG = 0xF0D0C0B1;
const static unsigned int MAGIC_FLAG = 0xCFFFFFFB;

static inline unsigned int peekAsSocketWord(const char *buffer) {
#if BYTE_ORDER == LITTLE_ENDIAN
    return buffer[0] | (buffer[1]<<8) | (buffer[2]<<16) | (buffer[3]<<24);
#else
    return buffer[3] | (buffer[2]<<8) | (buffer[1]<<16) | (buffer[0]<<24);
#endif
}

