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

#define LOG_TAG "SocketAPI"

#include <mutex>
#include <utils/RefBase.h>
#include <fmq/EventFlag.h>
#include <fmq/MessageQueue.h>

#include "SubtitleLog.h"
#include <utils/CallStack.h>

#include <vendor/amlogic/hardware/subtitleserver/1.0/ISubtitleServer.h>
#include "SubtitleReportAPI.h"


//namespace android {
using ::android::CallStack;
using ::android::sp;

using ::android::hardware::MessageQueue;
using ::android::hardware::kSynchronizedReadWrite;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::vendor::amlogic::hardware::subtitleserver::V1_0::ISubtitleServer;
using ::vendor::amlogic::hardware::subtitleserver::V1_0::Result;

typedef MessageQueue<uint8_t, kSynchronizedReadWrite> DataMQ;

typedef struct SubtitleContext {
    sp<ISubtitleServer> mRemote;
    std::unique_ptr<DataMQ> mDataMQ;
    int sId;
    std::mutex mLock;   // TODO: maybe we need global lock
    SubtitleContext() {
        sId = -1;
    }
} SubtitleContext;

typedef enum {
    SUBTITLE_TOTAL_TRACK    = 0x53544F54,  // 'STOT'
    SUBTITLE_START_PTS      = 0x53505453,  // 'SPTS'
    SUBTITLE_RENDER_TIME    = 0x53524454,  // 'SRDT'
    SUBTITLE_SUB_TYPE       = 0x53545950,  // 'STYP'
    SUBTITLE_TYPE_STRING    = 0x54505352,  // 'TPSR'
    SUBTITLE_LANG_STRING    = 0x4C475352,  // 'LGSR'
    SUBTITLE_SUB_DATA       = 0x504C4454,  // 'PLDT'

    SUBTITLE_RESET_SERVER   = 0x43545244,  // 'CDRT'
    SUBTITLE_EXIT_SERVER    = 0x43444558   // 'CDEX'

} PayloadType;

/**
    payload is:
        startFlag   : 4bytes
        sessionID   : 4Bytes (TBD)
        magic       : 4bytes (for double confirm)
        payload size: 4Bytes (indicate the size of payload, size is total_send_bytes - 4*5)
                      exclude this and above 4 items.
        payload Type: defined in PayloadType_t
        payload data: TO BE PARSED data
 */

#define LOGIT 1
const static unsigned int START_FLAG = 0xF0D0C0B1;
const static unsigned int MAGIC_FLAG = 0xCFFFFFFB;

const static unsigned int HEADER_SIZE = 4*5;

static void inline makeHeader(char *buf, int sessionId, PayloadType t, int payloadSize) {
    unsigned int val = START_FLAG;
    memcpy(buf, &val, sizeof(val));

    memcpy(buf+sizeof(int), &sessionId, sizeof(int));

    val = MAGIC_FLAG; // magic
    memcpy(buf+2*sizeof(int), &val, sizeof(int));

    memcpy(buf+3*sizeof(int), &payloadSize, sizeof(int));

    val = t;
    memcpy(buf+4*sizeof(val), &val, sizeof(val));
}

static bool prepareWritingQueueLocked(SubtitleContext *ctx) {
    std::unique_ptr<DataMQ> tempDataMQ;
    Result retval;
    Return<void> ret = ctx->mRemote->prepareWritingQueue(
            ctx->sId,
            2*1024*1024, // 2M buffer.
            [&](Result r, const DataMQ::Descriptor& dataMQ) {
                retval = r;
                if (retval == Result::OK) {
                    tempDataMQ.reset(new DataMQ(dataMQ));
                }
            });
    SUBTITLE_LOGI("prepareWritingQueueLocked");

    if (!ret.isOk() || retval != Result::OK) {
        SUBTITLE_LOGE("Error! prepare message Queue failed!");
        return false;
    }

    if (!tempDataMQ || !tempDataMQ->isValid()) {
        SUBTITLE_LOGE("Error! cannot get valid message Queue from service");
        return false;
    }
    ctx->mDataMQ = std::move(tempDataMQ);
    return true;
}

static bool fmqSendDataLocked(SubtitleContext *ctx, const char *data, size_t size) {
    size_t wrote = 0;
    size_t remainSize = size;
    //SUBTITLE_LOGI("fmqSendDataLocked %p %d", ctx->mDataMQ.get(), size);
    if (data != nullptr) {
        while (wrote < size) {
            size_t availableToWrite = ctx->mDataMQ->availableToWrite();
            size_t needWrite = (remainSize > availableToWrite) ? availableToWrite : remainSize;

            if (!ctx->mDataMQ->writeBlocking((uint8_t *)data, needWrite, 100*1000*1000LL)) {
                SUBTITLE_LOGE("data message queue write failed!");
                return false;
            } else {
                remainSize -= needWrite;
                wrote += needWrite;
                //SUBTITLE_LOGI("availableToWrite:%d needWrite:%d", availableToWrite, needWrite);
                // TODO: notify!!!
            }
        }
        return true;
    }

    return false;
}

// TODO: move all the report API to subtitlebinder
SubSourceHandle SubSource_Create(int sId) {
    SubtitleContext *ctx = new SubtitleContext();
    if (ctx == nullptr) return nullptr;
    SUBTITLE_LOGI("SubSource Create %d", sId);
    ctx->mLock.lock();
    sp<ISubtitleServer> service =  ISubtitleServer::tryGetService();
    int retry = 0;
    while ((service == nullptr) && (retry++ < 100)) {
        ctx->mLock.unlock();
        usleep(50*1000);//sleep 50ms
        ctx->mLock.lock();
        service = ISubtitleServer::tryGetService();
    }
    if (service == nullptr) {
        SUBTITLE_LOGE("Error, Cannot connect to remote Subtitle Service");
        ctx->mLock.unlock();
        delete ctx;
        return nullptr;
    }

    ctx->mRemote = service;
    ctx->sId = sId;

    if (!prepareWritingQueueLocked(ctx)) {
        SUBTITLE_LOGE("Error, Cannot get MessageQueue from remote Subtitle Service");
        ctx->mLock.unlock();
        delete ctx;
        return nullptr;
    }

    ctx->mLock.unlock();
    return ctx;
}

SubSourceStatus SubSource_Destroy(SubSourceHandle handle) {
    SubtitleContext *ctx = (SubtitleContext *)handle;
    if (ctx == nullptr) return SUB_STAT_INV;

    SUBTITLE_LOGI("SubSource destroy %d", ctx->sId);

    ctx->mLock.lock();
    ctx->mRemote = nullptr;
    ctx->mDataMQ = nullptr;
    ctx->mLock.unlock();

    delete ctx;
    return SUB_STAT_OK;
}


SubSourceStatus SubSource_Reset(SubSourceHandle handle) {
    SubtitleContext *ctx = (SubtitleContext *)handle;
    if (ctx == nullptr) return SUB_STAT_INV;

    SUBTITLE_LOGI("SubSource reset %d", ctx->sId);

    std::lock_guard<std::mutex> guard(ctx->mLock);

    if (ctx->mRemote == nullptr) {
        SUBTITLE_LOGE("Error! not connect to Remote Service!");
        return SUB_STAT_INV;
    }

    char buffer[64];
    memset(buffer, 0, 64);
    makeHeader(buffer, ctx->sId, SUBTITLE_RESET_SERVER, 7);
    strcpy(buffer+HEADER_SIZE, "reset\n");
    fmqSendDataLocked(ctx, buffer, HEADER_SIZE+7);
    return SUB_STAT_OK;
}

SubSourceStatus SubSource_Stop(SubSourceHandle handle) {
    SubtitleContext *ctx = (SubtitleContext *)handle;
    if (ctx == nullptr) return SUB_STAT_INV;

    SUBTITLE_LOGI("SubSource destroy %d", ctx->sId);
    std::lock_guard<std::mutex> guard(ctx->mLock);
    char buffer[64];
    memset(buffer, 0, 64);
    makeHeader(buffer, ctx->sId, SUBTITLE_EXIT_SERVER, 6);
    strcpy(buffer+HEADER_SIZE, "exit\n");
    fmqSendDataLocked(ctx, buffer, HEADER_SIZE+6);
    return SUB_STAT_OK;

}

SubSourceStatus SubSource_ReportRenderTime(SubSourceHandle handle, int64_t timeUs) {
    SubtitleContext *ctx = (SubtitleContext *)handle;
    static int count = 0;
    if (ctx == nullptr) return SUB_STAT_INV;

    {
        if (count++ %1000 == 0) SUBTITLE_LOGI("SubSource ReportRenderTime %d 0x%llx", ctx->sId, timeUs);
    }

    std::lock_guard<std::mutex> guard(ctx->mLock);
    char buffer[64];
    makeHeader(buffer, ctx->sId, SUBTITLE_RENDER_TIME, sizeof(int64_t));
    memcpy(buffer+HEADER_SIZE, &timeUs, sizeof(int64_t));
    fmqSendDataLocked(ctx, buffer, HEADER_SIZE + sizeof(int64_t));
    return SUB_STAT_OK;
}

SubSourceStatus SubSource_ReportStartPts(SubSourceHandle handle, int64_t pts) {
    SubtitleContext *ctx = (SubtitleContext *)handle;
    if (ctx == nullptr) return SUB_STAT_INV;

    SUBTITLE_LOGI("SubSource ReportStartPts %d 0x%llx", ctx->sId, pts);

    std::lock_guard<std::mutex> guard(ctx->mLock);
    char buffer[64];
    makeHeader(buffer, ctx->sId, SUBTITLE_START_PTS, sizeof(pts));
    memcpy(buffer+HEADER_SIZE, &pts, sizeof(pts));
    fmqSendDataLocked(ctx, buffer, HEADER_SIZE + sizeof(pts));

    return SUB_STAT_OK;
}

SubSourceStatus SubSource_ReportTotalTracks(SubSourceHandle handle, int trackNum) {
    SubtitleContext *ctx = (SubtitleContext *)handle;
    if (ctx == nullptr) return SUB_STAT_INV;

    SUBTITLE_LOGI("SubSource ReportTotalTracks %d 0x%x", ctx->sId, trackNum);

    std::lock_guard<std::mutex> guard(ctx->mLock);
    char buffer[64];
    makeHeader(buffer, ctx->sId, SUBTITLE_TOTAL_TRACK, sizeof(trackNum));
    memcpy(buffer+HEADER_SIZE, &trackNum, sizeof(trackNum));
    fmqSendDataLocked(ctx, buffer, HEADER_SIZE + sizeof(trackNum));

    return SUB_STAT_OK;
}


SubSourceStatus SubSource_ReportType(SubSourceHandle handle, int type) {
    SubtitleContext *ctx = (SubtitleContext *)handle;
    if (ctx == nullptr) return SUB_STAT_INV;

    SUBTITLE_LOGI("SubSource ReportType %d 0x%x", ctx->sId, type);

    std::lock_guard<std::mutex> guard(ctx->mLock);
    char buffer[64];
    makeHeader(buffer, ctx->sId, SUBTITLE_SUB_TYPE, sizeof(type));
    memcpy(buffer+HEADER_SIZE, &type, sizeof(type));
    fmqSendDataLocked(ctx, buffer, HEADER_SIZE + sizeof(type));

    return SUB_STAT_OK;
}


SubSourceStatus SubSource_ReportSubTypeString(SubSourceHandle handle, const char *type) {
    SubtitleContext *ctx = (SubtitleContext *)handle;
    std::lock_guard<std::mutex> guard(ctx->mLock);
    if (ctx == nullptr) return SUB_STAT_INV;

    char *buffer = new char[strlen(type)+1+HEADER_SIZE]();
    if (buffer == nullptr) return SUB_STAT_INV;

    SUBTITLE_LOGI("SubSource ReportTypeString %d %s", ctx->sId, type);

    makeHeader(buffer, ctx->sId, SUBTITLE_TYPE_STRING, strlen(type)+1);
    memcpy(buffer+HEADER_SIZE, type, strlen(type));
    fmqSendDataLocked(ctx, buffer, HEADER_SIZE + strlen(type)+1);

    delete[] buffer;
    return SUB_STAT_OK;
}
SubSourceStatus SubSource_ReportLanguageString(SubSourceHandle handle, const char *lang) {
    SubtitleContext *ctx = (SubtitleContext *)handle;
    std::lock_guard<std::mutex> guard(ctx->mLock);
    if (ctx == nullptr) return SUB_STAT_INV;
    SUBTITLE_LOGI("SubSource ReportLangString %d %s", ctx->sId, lang);

    char *buffer = new char[strlen(lang)+1+HEADER_SIZE]();
    if (buffer == nullptr) return SUB_STAT_INV;

    makeHeader(buffer, ctx->sId, SUBTITLE_LANG_STRING, strlen(lang)+1);
    memcpy(buffer+HEADER_SIZE, lang, strlen(lang));
    fmqSendDataLocked(ctx, buffer, HEADER_SIZE + strlen(lang)+1);

    delete[] buffer;
    return SUB_STAT_OK;
}


SubSourceStatus SubSource_SendData(SubSourceHandle handle, const char *data, int size) {
    SubtitleContext *ctx = (SubtitleContext *)handle;
    if (ctx == nullptr) return SUB_STAT_INV;
    SUBTITLE_LOGI("SubSource SubSource_SendData %d %d", ctx->sId, size);

    std::lock_guard<std::mutex> guard(ctx->mLock);
    char buffer[64];
    makeHeader(buffer, ctx->sId, SUBTITLE_SUB_DATA, size);
    fmqSendDataLocked(ctx, buffer, HEADER_SIZE);
    fmqSendDataLocked(ctx, data, size);
    SUBTITLE_LOGI("SubSource SubSource_SendData end %d %d", ctx->sId, size);

    return SUB_STAT_OK;
}

SubSourceStatus SubSource_GetVersion(SubSourceHandle handle, int *version) {
    *version = 1;
    return SUB_STAT_OK;
};

//}