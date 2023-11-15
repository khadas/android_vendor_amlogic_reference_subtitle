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

#ifndef __SUBTITLE_PARSER_EVENT_NOTIFIER_H__
#define __SUBTITLE_PARSER_EVENT_NOTIFIER_H__

static const int ERROR_DECODER_TIMEOUT = 2;
static const int ERROR_DECODER_LOSEDATA = 3;
static const int ERROR_DECODER_INVALIDDATA = 4;
static const int ERROR_DECODER_TIMEERROR = 5;
static const int ERROR_DECODER_END = 6;


enum subtitle_info_type {
    //for extension
    SUBTITLE_INFO_TELETEXT_LOAD_STATE = 900,
    //.....
};



static inline int __notifierErrorRemap(int error) {
    switch (error) {
        case 0:
            return ERROR_DECODER_LOSEDATA;  //event: 3  decoder error
        case 1:
            return ERROR_DECODER_INVALIDDATA;
        case 2:
            return ERROR_DECODER_TIMEERROR;
        case 3:
            return ERROR_DECODER_END;
        default:
            return error;
    }
}

class ParserEventNotifier {
public:
    virtual ~ParserEventNotifier(){};
    virtual void onSubtitleDataEvent(int event, int id) = 0;

    virtual void onSubtitleDimension(int width, int height) = 0;
    // report available state, may error
    virtual void onSubtitleAvailable(int available) = 0;

    virtual void onVideoAfdChange(int afd, int playerid) = 0;

    virtual void onMixVideoEvent(int val) = 0;

    virtual void onSubtitleLanguage(char* lang) = 0;

    //what: info type, extra: value, for info extension
    virtual void onSubtitleInfo(int what, int extra) = 0;
};

// TODO: revise the spu list and impl a common solution for decoded
// data process path. integrate in Parser.h's SPU manager class TODO list
class ParserSubdataNotifier {
public:
    virtual ~ParserSubdataNotifier(){}
    // We decoded new subdata...
    virtual void notifySubdataAdded() = 0;
};

#endif
