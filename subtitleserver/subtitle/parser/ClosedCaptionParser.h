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

#ifndef __SUBTITLE_CLOSED_CAPTION_PARSER_H__
#define __SUBTITLE_CLOSED_CAPTION_PARSER_H__
#include "Parser.h"
#include "DataSource.h"
#include "SubtitleTypes.h"
#include "DvbCommon.h"
#include "AmlogicUtil.h"
#include "ClosedCaptionJson.h"


#include <pthread.h>
#include <libzvbi.h>
#include <dtvcc.h>

class ClosedCaptionParser: public Parser {
public:
    ClosedCaptionParser(std::shared_ptr<DataSource> source);
    virtual ~ClosedCaptionParser();
    virtual int parse();
    virtual bool updateParameter(int type, void *data);
    virtual void dump(int fd, const char *prefix);
    virtual void setPipId (int mode, int id);

    static inline char sJsonStr[CC_JSON_BUFFER_SIZE];
    static inline ClosedCaptionParser *getCurrentInstance();
    void notifyChannelState(int stat, int channelId);
    void notifyAvailable(int err);

private:
    static void saveJsonStr(char * str);
    //void afd_evt_callback(long dev_no, int event_type, void *param, void *user_data);
    //void json_update_cb(ClosedCaptionHandleType handle);
    int startAtscCc(int source, int vfmt, int caption, int fgColor,int fgOpacity,
            int bgColor, int bgOpacity, int fontStyle, int fontSize);

    int startAmlCC();
    int stopAmlCC();

    int mChannelId = 0;
    int mDevNo = 0; // userdata device number
    int mVfmt = -1;
    int mPlayerId = 0;//-1;
    int mMediaSyncId = -1;
    TVSubtitleData *mCcContext;

    char *mLang = nullptr;
    static ClosedCaptionParser *sInstance;
};
#endif
