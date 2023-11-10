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

#ifndef __SUBTITLE_SUBTITLESERVICE_H__
#define __SUBTITLE_SUBTITLESERVICE_H__

#include <vector>

#include "SocketServer.h"
#include "FmqReceiver.h"
#include "Subtitle.h"
#include "ParserFactory.h"
#include "ParserEventNotifier.h"

enum {
    SUBTITLE_DUMP_SOURCE = 0,
};

class SubtitleService {
public:
    SubtitleService();
    ~SubtitleService();

    // TODO: revise params and defines
    // some subtitle, such as idx-sub, has tracks. if no tracks, ignore this parameter
    bool startSubtitle(std::vector<int> fds, int trackId, SubtitleIOType type, ParserEventNotifier *notifier);
    bool stopSubtitle();
    bool resetForSeek();

    /* return total subtitles associate with current video */
    int totalSubtitles();
    int subtitleType();
    std::string currentLanguage();
    void setLanguage(std::string lang);


    int updateVideoPosAt(int timeAt);
    void setSubType(int type);
    void setSubPid(int pid, int onid, int tsid);
    void setSubPageId(int pageId);
    void setSubAncPageId(int ancPageId);
    void setClosedCaptionVfmt(int vfmt);
    void setClosedCaptionLang(const char *lang);
    void setChannelId(int channelId);
    void setDemuxId(int demuxId);
    void setSecureLevel(int flag);
    void setPipId(int mode, int id);
    //ttx control api
    bool ttControl(int cmd, int magazine, int pageNo, int regionId, int param);
    int ttGoHome();
    int ttGotoPage(int pageNo, int subPageNo);
    int ttNextPage(int dir);
    int ttNextSubPage(int dir);

    bool userDataOpen(ParserEventNotifier *notifier);
    bool userDataClose();


    // debug inteface.
    void dump(int fd);
    void setupDumpRawFlags(int flagMap);

    bool startFmqReceiver(std::unique_ptr<FmqReader> reader);
    bool stopFmqReceiver();

private:
    std::shared_ptr<Subtitle> mSubtiles;

    SubtitleParamType mSubParam;

    std::shared_ptr<DataSource> mDataSource;
    std::unique_ptr<FmqReceiver> mFmqReceiver;
    std::shared_ptr<UserDataAfd> mUserDataAfd;

    int mDumpMaps;

    std::mutex mLock;
    bool mStarted;

    // API may request subtitle info immediately, but
    // parser may not parsed the info, so here we delayed a while.
    // but, if the subtitle not have this, may continuing delay.
    // here, this flag, only delay once.
    bool mIsInfoRetrieved;
};

#endif
