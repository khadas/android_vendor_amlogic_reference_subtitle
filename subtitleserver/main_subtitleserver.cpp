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
 *
 * Description:
 */
/**\file
 * \brief Subtitle module (version 2)
 *
 * \author amlogic
 * \date 2019-07-22: create the document
 ***************************************************************************/

#define LOG_TAG "subtitleserver"

#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <cutils/properties.h>
#include <hidl/HidlTransportSupport.h>

#include "SubtitleLog.h"

#include "SubtitleServer.h"

using namespace android;
using ::android::hardware::configureRpcThreadpool;
using ::android::hardware::joinRpcThreadpool;
using ::vendor::amlogic::hardware::subtitleserver::V1_0::ISubtitleServer;
using ::vendor::amlogic::hardware::subtitleserver::V1_0::implementation::SubtitleServer;

int main(int argc __unused, char** argv __unused) {
    SUBTITLE_LOGE("subtitleserver daemon starting");
    bool treble = true;//property_get_bool("persist.subtitle.treble", true);
    if (treble) {
        android::ProcessState::initWithDriver("/dev/vndbinder");
    }

    SUBTITLE_LOGI("subtitle daemon starting in %s mode", treble ? "treble" : "normal");
    configureRpcThreadpool(4, false);
    sp<ProcessState> proc(ProcessState::self());

    if (treble) {
        sp<ISubtitleServer> subtitleServer = new SubtitleServer();

        if (subtitleServer == nullptr) {
            SUBTITLE_LOGE("Cannot create ISubtitleServer service");
        } else if (subtitleServer->registerAsService() != OK) {
            SUBTITLE_LOGE("Cannot register ISubtitleServer service.");
        } else {
            SUBTITLE_LOGI("Treble SubtitleServerHal service created.--");
        }
    }
    ProcessState::self()->startThreadPool();
    SUBTITLE_LOGI("Treble SubtitleServerHal service created end-2-!!");
    //IPCThreadState::self()->joinThreadPool();
    joinRpcThreadpool();
    SUBTITLE_LOGI("Treble SubtitleServerHal service created end-3-!!");
}
