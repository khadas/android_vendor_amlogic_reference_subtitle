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
