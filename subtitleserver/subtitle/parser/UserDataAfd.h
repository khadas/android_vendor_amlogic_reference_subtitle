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

#ifndef __SUBTITLE_USERDATA_AFD_H__
#define __SUBTITLE_USERDATA_AFD_H__

#include "sub_types2.h"
#include "ParserFactory.h"
#include "ParserEventNotifier.h"
#include <thread>


#define USERDATA_DEVICE_NUM 0 // userdata device number

class UserDataAfd /*: public Parser*/{
public:
    UserDataAfd();
    virtual ~UserDataAfd();
    //virtual void dump(int fd, const char *prefix);
    int start(ParserEventNotifier *notify);
    int stop();
    void run();

    int parse() {return -1;};
    void dump(int fd, const char *prefix) {return;};
    void notifyCallerAfdChange(int afd);
    void setPipId(int mode, int id);
    static inline UserDataAfd *getCurrentInstance();

    static int sNewAfdValue;


private:
    static UserDataAfd *sInstance;
    ParserEventNotifier *mNotifier;
    int mPlayerId;
    int mMediasyncId;
    int mMode;
    std::mutex mMutex;
    std::shared_ptr<std::thread> mThread;
};


#endif

