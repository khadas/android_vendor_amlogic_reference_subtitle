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

// currently, we use android thread
// TODO: impl with std lib, make it portable
#ifndef __SUBTITLE_SOCKETSERVER_H__
#define __SUBTITLE_SOCKETSERVER_H__
#include <mutex>
#include <thread>
#include<vector>

#include <utils/Log.h>
#include <utils/Thread.h>

#include "IpcDataTypes.h"
#include "DataSource.h"
#include "ringbuffer.h"

// TODO: use portable impl
using android::Thread;
using android::Mutex;

/*socket communication, this definite need redesign. now only adaptor the older version */

class SubSocketServer {
public:
    SubSocketServer();
    ~SubSocketServer();

    // TODO: use smart ptr later
    static SubSocketServer* GetInstance();

    // TODO: error number
    int serve();

    static const int LISTEN_PORT = 10100;
    static const int  QUEUE_SIZE = 10;

    static bool registClient(DataListener *client) {
        std::lock_guard<std::mutex> guard(GetInstance()->mLock);
        GetInstance()->mClients.push_back(client);
        ALOGD("registClient: %p size=%d", client, GetInstance()->mClients.size());
        return true;
    }

    static bool unregisterClient(DataListener *client) {
        // obviously, BUG here! impl later, support multi-client.
        // TODO: revise the whole mClient, if we want to support multi subtitle

        std::lock_guard<std::mutex> guard(GetInstance()->mLock);

        std::vector<DataListener *> &vecs = GetInstance()->mClients;
        if (vecs.size() > 0) {
            for (auto it = vecs.cbegin(); it != vecs.cend(); it++) {
                if ((*it) == client) {
                    vecs.erase(it);
                    break;
                }
            }

            //GetInstance()->mClients.pop_back();
            ALOGD("unregisterClient: %p size=%d", client, GetInstance()->mClients.size());
        }
        return true;
    }

private:
    // mimicked from android, free to rewrite if you don't like it
    bool threadLoop();
    void __threadLoop();
    void dispatch();
    size_t read(void *buffer, size_t size);

    std::list<std::shared_ptr<std::thread>> mClientThreads;
    int clientConnected(int sockfd);
    //int handleClientConnected(int sockfd);

    // todo: impl client, not just a segment Receiver
    // todo: use key-value pair
    std::vector<DataListener *> mClients; //TODO: for clients

    std::thread mThread;
    bool mExitRequested;

    std::thread mDispatchThread;

    std::mutex mLock;
    static SubSocketServer* mInstance;

};

#endif
