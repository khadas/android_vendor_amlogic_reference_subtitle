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

#ifndef __SUBTITLE_PRESENTATION_H__
#define __SUBTITLE_PRESENTATION_H__
#include <thread>
#include <utils/Looper.h>
#include <mutex>
// maybe we can use std::chrono::steady_clock
// how to program it with longlong time?
#include <utils/Timers.h>
#include <utils/Mutex.h>

#include "Parser.h"

#include "Render.h"
#include "Display.h"

using android::sp;
using android::Looper;
using android::Message;
using android::Mutex;

class Presentation : public ParserSubdataNotifier {
public:
    Presentation(std::shared_ptr<Display> disp);
    virtual ~Presentation();

    bool syncCurrentPresentTime(int64_t pts);
    bool notifyStartTimeStamp(int64_t startTime);

    bool startPresent(std::shared_ptr<Parser> parser);
    bool stopPresent();
    bool resetForSeek();
    bool combineSamePtsSubtitle(std::shared_ptr<AML_SPUVAR> spu1, std::shared_ptr<AML_SPUVAR> spu2);
    bool compareBitAndSyncPts(std::shared_ptr<AML_SPUVAR> spu, int64_t pts);


    virtual void notifySubdataAdded();

    void dump(int fd, const char *prefix);

private:
    class MessageProcess : public android::MessageHandler {
    public:
        static const int MSG_PTS_TIME_UPDATED     = 0xBB10100;
        static const int MSG_PTS_TIME_CHECK_SPU   = 0xBB10101;
        static const int MSG_RESET_MESSAGE_QUEUE  = 0xCB10100;

        MessageProcess(Presentation *present, bool isExtSub);
        virtual ~MessageProcess();
        void looperLoop();
        void join();
        bool notifyMessage(int what);

    private:
        std::shared_ptr<std::thread> mLooperThread;
        std::mutex mLock;
        std::condition_variable mCv;
        Presentation *mPresent; // this use shared_ptr, decouple with sp raw pointer here
        bool mIsExtSub;
        bool mRequestThreadExit;
        void handleMessage(const Message& message);
        void handleExtSub(const Message& message);
        void handleStreamSub(const Message& message);
        std::shared_ptr<AML_SPUVAR> mLastShowingSpu;
        sp<Looper> mLooper;
    };

    int64_t mCurrentPresentRelativeTime;
    int64_t mStartPresentMonoTimeNs;
    int64_t mStartTimeModifier;

    std::shared_ptr<Parser> mParser;
    std::shared_ptr<Display> mDisplay;
    std::shared_ptr<Render> mRender;

    // for present subtitle.
    std::list<std::shared_ptr<AML_SPUVAR>> mEmittedShowingSpu;
    std::list<std::shared_ptr<AML_SPUVAR>> mEmittedFaddingSpu;

    /* this use android sp, so de-couple with shared_ptr */
    MessageProcess *mMsgProcess;

    // since mMsgProcess is a pointer here
    // we need a mutex to protect it
    std::mutex mMutex;


};



#endif
