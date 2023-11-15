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

#include <string>

#include "DataSource.h"
#include "FileSource.h"
#include "DeviceSource.h"
#include "SocketSource.h"
#include "ExternalDataSource.h"
#include "DemuxSource.h"
#include "VbiSource.h"
#include "DataSourceFactory.h"

std::shared_ptr<DataSource> DataSourceFactory::create(SubtitleIOType type) {
    switch (type) {
        case SubtitleIOType::E_SUBTITLE_FMQ:
            return std::shared_ptr<DataSource>(new ExternalDataSource());

        case SubtitleIOType::E_SUBTITLE_SOCK:
            return std::shared_ptr<DataSource>(new SocketSource());

        case SubtitleIOType::E_SUBTITLE_DEV:
            return std::shared_ptr<DataSource>(new DeviceSource());

        case SubtitleIOType::E_SUBTITLE_FILE:
            return std::shared_ptr<DataSource>(new FileSource());
        case SubtitleIOType::E_SUBTITLE_DEMUX:
            return std::shared_ptr<DataSource>(new DemuxSource());
        case SubtitleIOType::E_SUBTITLE_VBI:
            return std::shared_ptr<DataSource>(new VbiSource());
        default:
            break;
    }
    return nullptr;
}

std::shared_ptr<DataSource> DataSourceFactory::create(int fd, int fdExtra, SubtitleIOType type) {
    // TODO: check external subtitle
    // maybe we can impl a misc-IO impl for support both internal/ext subtitles
    switch (type) {
        case SubtitleIOType::E_SUBTITLE_FMQ:
            [[fallthrough]];
        case SubtitleIOType::E_SUBTITLE_SOCK:
            [[fallthrough]];
        case SubtitleIOType::E_SUBTITLE_DEMUX:
            [[fallthrough]];
        case SubtitleIOType::E_SUBTITLE_DEV:
            return create(type);
        case SubtitleIOType::E_SUBTITLE_FILE:
            return std::shared_ptr<DataSource>(new FileSource(fd, fdExtra));
        case SubtitleIOType::E_SUBTITLE_VBI:
            return std::shared_ptr<DataSource>(new VbiSource());
        default:
            break;
    }
    return nullptr;
}
