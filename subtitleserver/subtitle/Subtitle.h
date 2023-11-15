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

#ifndef __SUBTITLE_SUBTITLE_H__
#define __SUBTITLE_SUBTITLE_H__

#include <thread>
#include "InfoChangeListener.h"
#include "Parser.h"
#include "Presentation.h"
#include "ParserFactory.h"
#include <UserDataAfd.h>



class Subtitle : public InfoChangeListener {

public:
    Subtitle();
    Subtitle(bool isExtSub, int idxSubId, ParserEventNotifier *notifier);
    virtual ~Subtitle();

    void attachDataSource(std::shared_ptr<DataSource> source, std::shared_ptr<InfoChangeListener>listener);
    void dettachDataSource(std::shared_ptr<InfoChangeListener>listener);

    int getTotalSubtitles();
    int getSubtitleType();
    std::string currentLanguage();

    void scheduleStart();
    bool resetForSeek();


// from Info Listener:
    void onSubtitleChanged(int newTotal);
    void onRenderStartTimestamp(int64_t startTime);
    void onRenderTimeChanged(int64_t renderTime);
    void onTypeChanged(int newType);
    void onGetLanguage(std::string &lang);

    //ttx control api
    int ttGoHome();
    int ttGotoPage(int pageNo, int subPageNo);
    int ttNextPage(int dir);
    int ttNextSubPage(int dir);


    int onMediaCurrentPresentationTime(int ptsMills);

    bool setParameter(void *params);

    void dump(int fd);

private:
    static const int ACTION_SUBTITLE_RECEIVED_SUBTYPE = 1;
    static const int ACTION_SUBTITLE_RESET_FOR_SEEK = 2;
    static const int ACTION_SUBTITLE_SET_PARAM = 3;
    static const int ACTION_SUBTITLE_RESET_MEDIASYNC = 4;
    void run();

    std::shared_ptr<DataSource> mDataSource;

    int mSubtitleTracks;
    int mCurrentSubtitleType;
    std::string mCurrentLanguage;

    int64_t mRenderTime;

    std::shared_ptr<Presentation> mPresentation;

    // TODO: create a manager for this. also, not use this
    std::shared_ptr<Parser> mParser;
    std::shared_ptr<UserDataAfd> mUserDataAfd;

    ParserEventNotifier *mParserNotifier;

    bool mExitRequested;
    std::shared_ptr<std::thread> mThread;

    std::shared_ptr<SubtitleParamType> mSubPrams;

    int mPendingAction;
    std::mutex mMutex;
    std::condition_variable mCv;

    bool mIsExtSub;
    int mIdxSubTrack;// TODO: maybe we can use other API for track
};

#endif
