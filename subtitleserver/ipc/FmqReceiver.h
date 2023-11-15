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

#include <mutex>
#include <thread>
#include<vector>

#include <utils/Log.h>
#include <utils/Thread.h>

#include "DataSource.h"
#include "IpcDataTypes.h"
#include "ringbuffer.h"
#include "FmqReader.h"

static inline uint32_t __min(uint32_t a, uint32_t b) {
    return (((a) < (b)) ? (a) : (b));
}
class FmqReceiver {
public:
    FmqReceiver() = delete;
    FmqReceiver(std::unique_ptr<FmqReader> reader);
    ~FmqReceiver();

    bool registClient(std::shared_ptr<DataListener> client) {
        std::lock_guard<std::mutex> guard(mLock);
        mClients.push_back(client);
        ALOGD("registClient: %p size=%d", client.get(), mClients.size());
        return true;
    }

    bool unregisterClient(std::shared_ptr<DataListener> client) {
        // obviously, BUG here! impl later, support multi-client.
        // TODO: revise the whole mClient, if we want to support multi subtitle
        std::lock_guard<std::mutex> guard(mLock);

        std::vector<std::shared_ptr<DataListener>> &vecs = mClients;
        if (vecs.size() > 0) {
            for (auto it = vecs.cbegin(); it != vecs.cend(); it++) {
                if ((*it).get() == client.get()) {
                    vecs.erase(it);
                    break;
                }
            }

            //GetInstance()->mClients.pop_back();
            ALOGD("unregisterClient: %p size=%d", client.get(), mClients.size());
        }
        return true;
    }

    void dump(int fd, const char *prefix);

private:
    bool readLoop();
    bool mStop;
    std::unique_ptr<FmqReader> mReader;
    std::thread mReaderThread;


    std::mutex mLock;
    std::vector<std::shared_ptr<DataListener>> mClients; //TODO: for clients
};
