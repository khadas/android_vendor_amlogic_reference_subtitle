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

#ifndef __SUBTITLE_SCTE27_PARSER_H__
#define __SUBTITLE_SCTE27_PARSER_H__
#include "Parser.h"
#include "DataSource.h"
#include "sub_types2.h"
#include "sub_types.h"

#define BUFFER_W 720
#define BUFFER_H 480
#define VIDEO_W BUFFER_W
#define VIDEO_H BUFFER_H

#define DEMUX_ID 2
#define BITMAP_PITCH BUFFER_W*4


class Scte27Parser: public Parser {
public:
    Scte27Parser(std::shared_ptr<DataSource> source);
    virtual ~Scte27Parser();
    virtual int parse();
    virtual bool updateParameter(int type, void *data);
    virtual void setPipId (int mode, int id);
    virtual void dump(int fd, const char *prefix);

    TVSubtitleData *getContexts() {
        return mScteContext;
    }
    void notifySubtitleDimension(int width, int height);
    static inline Scte27Parser *getCurrentInstance();
    void notifyCallerAvail(int avail);
    void notifyCallerLanguage(char *lang);
    TVSubtitleData *mScteContext;
    std::shared_ptr<uint8_t> mData;
private:
    int getSpu();
    int stopScte27();
    int startScte27(int dmxId, int pid);
    void checkDebug();

   // TVSubtitleData *mScteContext;
    int mPid;
    int mPlayerId = -1;
    int mMediaSyncId = -1;

    static Scte27Parser *sInstance;
};


#endif

