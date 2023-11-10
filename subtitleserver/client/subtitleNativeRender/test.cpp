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

#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include "SubtitleLog.h"

#include "AmlNativeSubRender.h"

#include "SubtitleLog.h"
#include "SubtitleContext.h"

char *gMemory;

static int32_t lock(SubNativeRenderHnd hnd, SubNativeRenderBuffer *buf) {
    if (buf == nullptr) {
        SUBTITLE_LOGE("Error! no buffer!");
        return -1;
    }
    buf->format = 1;
    buf->height = 1080;
    buf->stride  = buf->width = 1920;
    if (gMemory == nullptr) {
        gMemory = (char *)malloc(1920*1080*4);
    }
    buf->bits = gMemory;

    return 0;
}

static int32_t unlockAndPost(SubNativeRenderHnd hnd) {
    return 0;
}

static int32_t setBuffersGeometry(SubNativeRenderHnd hnd, int w, int h, int format) {
    return 0;
}


int main(int argc __unused, char** argv __unused) {
    SubNativeRenderCallback callback;
    callback.lock = lock;
    callback.unlockAndPost = unlockAndPost;
    callback.setBuffersGeometry = setBuffersGeometry;


    aml_RegisterNativeWindowCallback(callback);

    SubtitleContext::GetInstance().addSubWindow(1, nullptr);
    SubtitleContext::GetInstance().startPlaySubtitle(1, nullptr);

    while (1) usleep(10000);
    return 0;
}
