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

#ifndef __SUBTITLE_SOCKET_CLIENT_API_H__
#define __SUBTITLE_SOCKET_CLIENT_API_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef void *SubSourceHandle;

typedef enum {
    SUB_STAT_INV = -1,
    SUB_STAT_FAIL = 0,
    SUB_STAT_OK,
}SubSourceStatus;

SubSourceHandle SubSource_Create(int sId);
SubSourceStatus SubSource_Destroy(SubSourceHandle handle);

SubSourceStatus SubSource_Reset(SubSourceHandle handle);
SubSourceStatus SubSource_Stop(SubSourceHandle handle);

SubSourceStatus SubSource_ReportRenderTime(SubSourceHandle handle, int64_t timeUs);
SubSourceStatus SubSource_ReportStartPts(SubSourceHandle handle, int64_t type);

SubSourceStatus SubSource_ReportTotalTracks(SubSourceHandle handle, int trackNum);
SubSourceStatus SubSource_ReportType(SubSourceHandle handle, int type);

SubSourceStatus SubSource_ReportSubTypeString(SubSourceHandle handle, const char *type);
SubSourceStatus SubSource_ReportLanguageString(SubSourceHandle handle, const char *lang);


SubSourceStatus SubSource_SendData(SubSourceHandle handle, const char *data, int size);


#ifdef __cplusplus
}
#endif


#endif
