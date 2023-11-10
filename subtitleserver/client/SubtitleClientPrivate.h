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

#ifndef _SUBTITLE_CLIENT_PRIVATE_H_
#define _SUBTITLE_CLIENT_PRIVATE_H_

#include <media/stagefright/foundation/ADebug.h>

#include "ClientAdapter.h"
#include "SubtitleClient.h"
#include "subtitleServerHidlClient/SubtitleServerHidlClient.h"
#include "SubtitleReportAPI.h"

using namespace android;

class SubtitleClientPrivate : public RefBase
{
public:
    SubtitleClientPrivate();
    ~SubtitleClientPrivate();

    uint32_t getVersion() const;
    status_t connect(bool attachMode);
    void disconnect();

    status_t setViewWindow(int x, int y, int width, int height);
    status_t setVisible(bool visible);
    bool isVisible() const;
    status_t setViewAttribute(const SubtitleClient::ViewAttribute& attr);
    status_t init(const subtitle::Subtitle_Param& param);
    status_t getSourceAttribute(subtitle::Subtitle_Param* param);
    void registerCallback(const SubtitleClient::CallbackParam& cbParam);
    void setInputSource(SubtitleClient::InputSource source);
    SubtitleClient::InputSource getInputSource() const;
    SubtitleClient::TransferType getDefaultTransferType() const;
    status_t openTransferChannel(SubtitleClient::TransferType transferType);
    void closeTransferChannel();
    bool isTransferChannelOpened() const;
    size_t getHeaderSize() const;
    status_t constructPacketHeader(void* header, size_t headerBufferSize, const SubtitleClient::PackHeaderAttribute& attr);
    ssize_t send(const void* buf, size_t len);
    status_t flush();
    status_t start();
    status_t stop();
    status_t goHome();
    status_t gotoPage(int pageNo, int subPageNo);
    status_t nextPage(int direction);
    status_t nextSubPage(int direction);
    bool isPlaying() const;

protected:
    static void subtitle_evt_callback(SUBTITLE_EVENT evt, int index);
    static void subtitle_available_callback(SUBTITLE_STATE state, int val);
    status_t convertSubTypeToSubDecodeType(subtitle::SubtitleType type);
    status_t convertSubTypeToFFmpegType(subtitle::SubtitleType type);

private:
    android::SubtitleServerHidlClient::SUB_Para_t msubtitleCtx;
    SubtitleClient::InputSource mInputSource;

    SubSourceHandle mSourceHandle;


    //attach to ready subtitleserver
    bool mAttachMode = false;

    DISALLOW_EVIL_CONSTRUCTORS(SubtitleClientPrivate);
};



#endif
