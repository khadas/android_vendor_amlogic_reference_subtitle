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

#ifndef _SUBTITLE_PARSER_FACTORY_H__
#define _SUBTITLE_PARSER_FACTORY_H__

#include "DataSource.h"
#include "Parser.h"

#define TYPE_SUBTITLE_Q_TONE_DATA 0xAAAA
#define SUPPORT_KOREA

enum ExtSubtitleType {
    SUB_INVALID   = -1,
    SUB_MICRODVD,
    SUB_SUBRIP,
    SUB_SUBVIEWER,
    SUB_SAMI,
    SUB_VPLAYER,  //4
    SUB_RT,
    SUB_SSA,      //6
    SUB_PJS,
    SUB_MPSUB,    //8
    SUB_AQTITLE,
    SUB_SUBVIEWER2,
    SUB_SUBVIEWER3,
    SUB_SUBRIP09,
    SUB_JACOSUB,
    SUB_MPL1,
    SUB_MPL2,
    SUB_XML,
    SUB_TTML,
    SUB_LRC,
    SUB_DIVX,
    SUB_WEBVTT,
    SUB_IDXSUB,
};



//from ffmpeg avcodec.h
enum SubtitleCodecID {
    /* subtitle codecs */
    AV_CODEC_ID_DVD_SUBTITLE = 0x17000,
    AV_CODEC_ID_DVB_SUBTITLE,
    AV_CODEC_ID_TEXT,  ///< raw UTF-8 text
    AV_CODEC_ID_XSUB,
    AV_CODEC_ID_SSA,
    AV_CODEC_ID_MOV_TEXT,
    AV_CODEC_ID_HDMV_PGS_SUBTITLE,
    AV_CODEC_ID_DVB_TELETEXT,
    AV_CODEC_ID_SRT,

    AV_CODEC_ID_VOB_SUBTITLE = 0x1700a, //the same as AV_CODEC_ID_DVD_SUBTITLE

    AV_CODEC_ID_MICRODVD   = 0x17800,
    AV_CODEC_ID_EIA_608,
    AV_CODEC_ID_JACOSUB,
    AV_CODEC_ID_SAMI,
    AV_CODEC_ID_REALTEXT,
    AV_CODEC_ID_STL,
    AV_CODEC_ID_SUBVIEWER1,
    AV_CODEC_ID_SUBVIEWER,
    AV_CODEC_ID_SUBRIP,
    AV_CODEC_ID_WEBVTT,
    AV_CODEC_ID_MPL2,
    AV_CODEC_ID_VPLAYER,
    AV_CODEC_ID_PJS,
    AV_CODEC_ID_ASS,
    AV_CODEC_ID_HDMV_TEXT_SUBTITLE,

};



enum DisplayType {
    SUBTITLE_IMAGE_DISPLAY       = 1,
    SUBTITLE_TEXT_DISPLAY,
};

typedef enum {
    DTV_SUB_INVALID         = -1,
    DTV_SUB_CC              = 2,
    DTV_SUB_SCTE27          = 3,
    DTV_SUB_DVB             = 4,
    DTV_SUB_DTVKIT_DVB      = 5,
    DTV_SUB_DTVKIT_TELETEXT = 6,
    DTV_SUB_DTVKIT_SCTE27   = 7,
    DTV_SUB_ARIB24          = 8,
    DTV_SUB_DTVKIT_ARIB24   = 9,
    DTV_SUB_TTML            = 10,
    DTV_SUB_DTVKIT_TTML     = 11,
    DTV_SUB_SMPTE_TTML      = 12,
    DTV_SUB_DTVKIT_SMPTE_TTML = 13,
} DtvSubtitleType;

enum VideoFormat {
    INVALID_CC_TYPE    = -1,
    MPEG_CC_TYPE       = 1,
    H264_CC_TYPE       = 2,
    H265_CC_TYPE       = 3,
};

typedef struct {
    int ChannelID = 0 ;
    int vfmt = 0;  //Video format
    char lang[64] = {0};  //channel language
} ClosedCaptionParam;

typedef struct {
    int SCTE27_PID = 0;
    int demuxId = 0;
    int flag = 0;
} Scte27Param;

typedef struct {
   int demuxId = 0;
   int pid = 0;
   int flag = 0;
   int compositionId = 0;
   int ancillaryId = 0;
}DvbParam;

typedef struct {
   int demuxId = 0;
   int pid = 0;
   int flag = 0;
   int languageCodeId = 0;
}Arib24Param;

typedef struct {
   int demuxId = 0;
   int pid = 0;
   int flag = 0;
}TtmlParam;

typedef struct {
   int demuxId = 0;
   int pid = 0;
   int flag = 0;
} SmpteTtmlParam;


typedef enum{
   TT_EVENT_INVALID          = -1,
   // These are the four FastText shortcuts, usually represented by red, green,
   // yellow and blue keys on the handset.
   TT_EVENT_QUICK_NAVIGATE_1 = 0,
   TT_EVENT_QUICK_NAVIGATE_2,
   TT_EVENT_QUICK_NAVIGATE_3,
   TT_EVENT_QUICK_NAVIGATE_4, //3//3
   // The ten numeric keys used to input page indexes.
   TT_EVENT_0,
   TT_EVENT_1,
   TT_EVENT_2,
   TT_EVENT_3,
   TT_EVENT_4,
   TT_EVENT_5,
   TT_EVENT_6,
   TT_EVENT_7,
   TT_EVENT_8,
   TT_EVENT_9,//13
   // This is the home key, which returns to the nominated index page for this
   //   service.
   TT_EVENT_INDEXPAGE,
   // These are used to quickly increment/decrement the page index.
   TT_EVENT_NEXTPAGE,
   TT_EVENT_PREVIOUSPAGE,
   // These are used to navigate the sub-pages when in 'hold' mode.
   TT_EVENT_NEXTSUBPAGE,
   TT_EVENT_PREVIOUSSUBPAGE,
   // These are used to traverse the page history (if caching requested).
   TT_EVENT_BACKPAGE,
   TT_EVENT_FORWARDPAGE, //20
   // This is used to toggle hold on the current page.
   TT_EVENT_HOLD,
   // Reveal hidden page content (as defined in EBU specification)
   TT_EVENT_REVEAL,
   // This key toggles 'clear' mode (page hidden until updated)
   TT_EVENT_CLEAR,
   // This key toggles 'clock only' mode (page hidden until updated)
   TT_EVENT_CLOCK,
   // Used to toggle transparent background ('video mix' mode)
   TT_EVENT_MIX_VIDEO, //25
   // Used to toggle double height top / double-height bottom / normal height display.
   TT_EVENT_DOUBLE_HEIGHT,
   // Functional enhancement may offer finer scrolling of double-height display.
   TT_EVENT_DOUBLE_SCROLL_UP,
   TT_EVENT_DOUBLE_SCROLL_DOWN,
   // Used to initiate/cancel 'timer' mode (clear and re-display page at set time)
   TT_EVENT_TIMER,
   TT_EVENT_GO_TO_PAGE,
   TT_EVENT_GO_TO_SUBTITLE,
   TT_EVENT_SET_REGION_ID,
   TT_EVENT_SUBPAGE_MODE
} TeletextEvent;

typedef struct {
   int demuxId    = -1;
   int pid        = -1;
   int flag       = -1; // Flag of whether to encrypt or not, 0 means unencrypted, 1 means encrypted
   int magazine   = -1; // Teletext usually uses magazines to organize information. There are at most 8 different magazines, 100-199 (M=1), 200-299 (M=2), 300-399 (M=3), 400-499 (M =4), 500-599 (M=5), 600-699 (M=6), 700-799 (M=7), 800-899 (M=0)
   int pageNo     = -1; // Home page number
   int subPageNo  = -1; // Subpage page number
   int pageDir    =  0; // +1:next page, -1: last page
   int subPageDir =  0; // +1:next sub page, -1: last sub page
   int regionId   = -1; // Country subset specification, range (0~87), default G0 and G2
   TeletextEvent event = TT_EVENT_INVALID;
   // when play the same program, vbi is cached, must support quick reference page
   int onid       = -1; // origin network id for check need close vbi or not
   int tsid       = -1; // tsid. for check need close vbi or not
} TeletextParam;

struct SubtitleParamType {
    SubtitleType subType;
    DtvSubtitleType dtvSubType;

    // TODO: maybe, use union for params
    Scte27Param scte27Param;
    ClosedCaptionParam closedCaptionParam;
    TeletextParam teletextParam;
    Arib24Param arib24Param; // the pes pid for filter subtitle data from demux
    TtmlParam ttmlParam;     // the pes pid for filter subtitle data from demux
    DvbParam dvbParam;       // the pes pid for filter subtitle data from demux
    SmpteTtmlParam smpteTtmlParam; //the pes pid for filter subtitle data from demux

    int playerId;
    int mediaId;
    int idxSubTrackId; // only for idxsub

    SubtitleParamType() : playerId(-1), mediaId(-1), idxSubTrackId(0) {
        subType = TYPE_SUBTITLE_INVALID;
        memset(&closedCaptionParam, 0, sizeof(closedCaptionParam));
    }

    void update() {
        switch (dtvSubType) {
            case DTV_SUB_CC:
                subType = TYPE_SUBTITLE_CLOSED_CAPTION;
                break;
            case DTV_SUB_SCTE27:
            case DTV_SUB_DTVKIT_SCTE27:
                subType = TYPE_SUBTITLE_SCTE27;
                break;
            case DTV_SUB_DVB:
            case DTV_SUB_DTVKIT_DVB:
                subType = TYPE_SUBTITLE_DVB;
                break;
            case DTV_SUB_DTVKIT_TELETEXT:
                subType = TYPE_SUBTITLE_DVB_TELETEXT;
                break;
            case DTV_SUB_TTML:
            case DTV_SUB_DTVKIT_TTML:
                subType = TYPE_SUBTITLE_DVB_TTML;
                break;
            case DTV_SUB_ARIB24:
            case DTV_SUB_DTVKIT_ARIB24:
                subType = TYPE_SUBTITLE_ARIB_B24;
                break;
            case DTV_SUB_SMPTE_TTML:
            case DTV_SUB_DTVKIT_SMPTE_TTML:
                subType = TYPE_SUBTITLE_SMPTE_TTML;
                break;
            default:
                break;
        }
    }

    bool isValidDtvParams () {
        return dtvSubType == DTV_SUB_CC || dtvSubType == DTV_SUB_SCTE27 || dtvSubType == DTV_SUB_DTVKIT_SCTE27 ; //only cc or scte27 valid
    }

    void dump(int fd, const char * prefix) {
        dprintf(fd, "%s subType: %d\n", prefix, subType);
        dprintf(fd, "%s dtvSubType: %d\n", prefix, dtvSubType);
        dprintf(fd, "%s   SCTE27 (PID: %d)\n", prefix, scte27Param.SCTE27_PID);
        dprintf(fd, "%s   CC     (ChannelID: %d vfmt: %d)\n", prefix, closedCaptionParam.ChannelID, closedCaptionParam.vfmt);
        dprintf(fd, "%s   TelTxt (PageNo: %d subPageNo: %d PageDir: %d subPageDir: %d)\n",
            prefix, teletextParam.pageNo, teletextParam.subPageNo, teletextParam.pageDir, teletextParam.subPageDir);
    }
};


class ParserFactory {

public:
    static std::shared_ptr<Parser> create(std::shared_ptr<SubtitleParamType>, std::shared_ptr<DataSource> source);
    static DisplayType getDisplayType(int type);

};


#endif
