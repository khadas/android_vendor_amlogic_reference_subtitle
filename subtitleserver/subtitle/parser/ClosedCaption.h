/***************************************************************************
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
 *
 * Description:
 */
/**\file am_cc.h
 *  Close caption parser module
 *
 * \author Amlogic
 * \date 2011-12-27: create the document
 ***************************************************************************/

#ifndef _CLOSED_CAPTION_H
#define _CLOSED_CAPTION_H

#include <pthread.h>
#include <libzvbi.h>
#include <dtvcc.h>

#include "ClosedCaptionJson.h"
#include "AmlogicUtil.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Error code of close caption parser*/
enum CLOSED_CAPTION_ERROR_CODE {
    CLOSED_CAPTION_ERROR_BASE=ERROR_BASE(AM_MOD_CC),
    CLOSED_CAPTION_ERROR_INVALID_PARAM,    /**< Invalid parameter*/
    CLOSED_CAPTION_ERROR_SYS,                  /**< System error*/
    CLOSED_CAPTION_ERROR_NO_MEM,               /**< Not enough memory*/
    CLOSED_CAPTION_ERROR_LIBZVBI,              /**< libzvbi error*/
    CLOSED_CAPTION_ERROR_BUSY,                 /**< Device is busy*/
    CLOSED_CAPTION_ERROR_END
};

/** caption mode*/
typedef enum {
    CLOSED_CAPTION_NONE = -1,
    CLOSED_CAPTION_DEFAULT,
    /*NTSC CC channels*/
    CLOSED_CAPTION_CC1,
    CLOSED_CAPTION_CC2,
    CLOSED_CAPTION_CC3,
    CLOSED_CAPTION_CC4,
    CLOSED_CAPTION_TEXT1,
    CLOSED_CAPTION_TEXT2,
    CLOSED_CAPTION_TEXT3,
    CLOSED_CAPTION_TEXT4,
    /*DTVCC services*/
    CLOSED_CAPTION_SERVICE1,
    CLOSED_CAPTION_SERVICE2,
    CLOSED_CAPTION_SERVICE3,
    CLOSED_CAPTION_SERVICE4,
    CLOSED_CAPTION_SERVICE5,
    CLOSED_CAPTION_SERVICE6,
    CLOSED_CAPTION_XDS,
    CLOSED_CAPTION_DATA,
    CLOSED_CAPTION_MAX
}ClosedCaptionMode;

/**Close caption parser handle*/
typedef void* ClosedCaptionHandleType;
/**Draw parameter of close caption*/
typedef struct ClosedCaptionDrawPara ClosedCaptionDrawPara_t;
/**Callback function of beginning of drawing*/
typedef void (*ClosedCaptionDrawBeginType)(ClosedCaptionHandleType handle, ClosedCaptionDrawPara_t *draw_para);
/**Callback function of end of drawing*/
typedef void (*ClosedCaptionDrawEndType)(ClosedCaptionHandleType handle, ClosedCaptionDrawPara_t *draw_para);
/**VBI program information callback.*/
typedef void (*ClosedCaptionVBIProgInfoCbType)(ClosedCaptionHandleType handle, vbi_program_info *pi);
/**VBI network callback.*/
typedef void (*ClosedCaptionVBINetworkCbType)(ClosedCaptionHandleType handle, vbi_network *n);
typedef void (*ClosedCaptionVBIRatingCbType)(ClosedCaptionHandleType handle, vbi_rating *rating);
/**CC data callback.*/
typedef void (*ClosedCaptionDataCbType)(ClosedCaptionHandleType handle, int mask);
/**Q_tone data callback.*/
typedef void (*ClosedCaptionQ_Tone_DataCbType)(ClosedCaptionHandleType handle, char* buffer, int size);
typedef void (*ClosedCaptionUpdataJsonType)(ClosedCaptionHandleType handle);
typedef void (*ClosedCaptionReportError)(ClosedCaptionHandleType handle, int error);

/** Font size, see details in CEA-708**/
typedef enum {
    CLOSED_CAPTION_FONT_SIZE_DEFAULT,
    CLOSED_CAPTION_FONT_SIZE_SMALL,
    CLOSED_CAPTION_FONT_SIZE_STANDARD,
    CLOSED_CAPTION_FONT_SIZE_BIG,
    CLOSED_CAPTION_FONT_SIZE_MAX
}ClosedCaptionFontSize;

/** Font style, see details in CEA-708*/
typedef enum {
    CLOSED_CAPTION_FONT_STYLE_DEFAULT,
    CLOSED_CAPTION_FONT_STYLE_MONO_SERIF,
    CLOSED_CAPTION_FONT_STYLE_PROP_SERIF,
    CLOSED_CAPTION_FONT_STYLE_MONO_NO_SERIF,
    CLOSED_CAPTION_FONT_STYLE_PROP_NO_SERIF,
    CLOSED_CAPTION_FONT_STYLE_CASUAL,
    CLOSED_CAPTION_FONT_STYLE_CURSIVE,
    CLOSED_CAPTION_FONT_STYLE_SMALL_CAPITALS,
    CLOSED_CAPTION_FONT_STYLE_MAX
}ClosedCaptionFontStyle;

/** Color opacity, see details in CEA-708**/
typedef enum {
    CLOSED_CAPTION_OPACITY_DEFAULT,
    CLOSED_CAPTION_OPACITY_TRANSPARENT,
    CLOSED_CAPTION_OPACITY_TRANSLUCENT,
    CLOSED_CAPTION_OPACITY_SOLID,
    CLOSED_CAPTION_OPACITY_FLASH,
    CLOSED_CAPTION_OPACITY_MAX
}ClosedCaptionOpacity;

/** Color, see details in CEA-708-D**/
typedef enum {
    CLOSED_CAPTION_COLOR_DEFAULT,
    CLOSED_CAPTION_COLOR_WHITE,
    CLOSED_CAPTION_COLOR_BLACK,
    CLOSED_CAPTION_COLOR_RED,
    CLOSED_CAPTION_COLOR_GREEN,
    CLOSED_CAPTION_COLOR_BLUE,
    CLOSED_CAPTION_COLOR_YELLOW,
    CLOSED_CAPTION_COLOR_MAGENTA,
    CLOSED_CAPTION_COLOR_CYAN,
    CLOSED_CAPTION_COLOR_MAX
}ClosedCaptionColor;

/**Close caption drawing parameters*/
struct ClosedCaptionDrawPara {
    int caption_width;   /**< Width of the caption*/
    int caption_height;  /**< Height of the caption*/
};

/** User options of close caption*/
typedef struct {
    ClosedCaptionFontSize        font_size;  /**< Font size*/
    ClosedCaptionFontStyle       font_style; /**< Font style*/
    ClosedCaptionColor           fg_color;   /**< Frontground color*/
    ClosedCaptionOpacity         fg_opacity; /**< Frontground opacity*/
    ClosedCaptionColor           bg_color;   /**< Background color*/
    ClosedCaptionOpacity         bg_opacity; /**< Background opacity*/
}ClosedCaptionUserOptions;

/** Close caption's input.*/
typedef enum {
    CLOSED_CAPTION_INPUT_USERDATA, /**< Input from MPEG userdata.*/
    CLOSED_CAPTION_INPUT_VBI       /**< Input from VBI.*/
}ClosedCaptionInput;

/** Close caption parser create parameters*/
typedef struct {
    uint8_t            *bmp_buffer;    /**< Drawing buffer*/
    int                 pitch;         /**< Line pitch of the drawing buffer*/
    int                 bypass_cc_enable; /**< Bypass CC data flag*/
    int                 data_timeout;  /**< Data timeout value in ms*/
    int                 switch_timeout;/**< Caption 1/2 switch timeout in ms.*/
    unsigned int       decoder_param;
    void               *user_data;     /**< User defined data*/
    char                lang[10];
    ClosedCaptionInput       input;         /**< Input type.*/
    ClosedCaptionVBIProgInfoCbType pinfo_cb;    /**< VBI program information callback.*/
    ClosedCaptionVBINetworkCbType  network_cb;  /**< VBI network callback.*/
    ClosedCaptionVBIRatingCbType   rating_cb;   /**< VBI rating callback.*/
    ClosedCaptionDataCbType      data_cb;       /**< Received data callback.*/
    ClosedCaptionDrawBeginType   draw_begin;    /**< Drawing beginning callback*/
    ClosedCaptionDrawEndType     draw_end;      /**< Drawing end callback*/
    ClosedCaptionUpdataJsonType json_update;
    ClosedCaptionReportError report;
    ClosedCaptionQ_Tone_DataCbType q_tone_cb;
    char *json_buffer;
    bool auto_detect_play;
}ClosedCaptionCreatePara_t;

/** Close caption parser start parameter*/
typedef struct {
    int                 vfmt;
    int                      player_id;
    int                      mediaysnc_id;
    ClosedCaptionMode    caption1;     /**< Mode 1*/
    ClosedCaptionMode    caption2;     /**< Mode 2.*/
    ClosedCaptionUserOptions    user_options; /**< User options*/
    bool          auto_detect_play;
}ClosedCaptionStartPara_t;


/****************************************************************************
 * Function prototypes
 ***************************************************************************/

/** Create a new close caption parser
 * [in] para Create parameters
 * [out] handle Return the parser handle
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int ClosedCaptionCreate(ClosedCaptionCreatePara_t *para, ClosedCaptionHandleType *handle);

/** Release a close caption parser
 * [out] handle Close caption parser handle
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int ClosedCaptionDestroy(ClosedCaptionHandleType handle);

extern int ClosedCaptionStart(ClosedCaptionHandleType handle, ClosedCaptionStartPara_t *para);

/** Stop close caption parsing
 * handle Close caption parser handle
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int ClosedCaptionStop(ClosedCaptionHandleType handle);

enum {
    CLOSED_CAPTION__EVENT_SET_USER_OPTIONS,
};

enum {
    FLASH_NONE,
    FLASH_SHOW,
    FLASH_HIDE
};

enum {
    UNKNOWN = -1,
    MPEG12,
    MPEG4,
    H264,
    MJPEG,
    REAL,
    JPEG,
    VC1,
    AVS,
    SW,
    H264MVC,
    H264_4K2K,
    HEVC,
    H264_ENC,
    JPEG_ENC,
    VP9,
};


typedef struct ClosedCaptionDecoder ClosedCaptionDecoder_t;

typedef struct json_chain ClosedCaptionJsonChain_t;
#define JSON_STRING_LENGTH 8192
struct json_chain {
    char* buffer;
    uint32_t pts;
    struct timespec decode_time;
    struct timespec display_time;
    struct json_chain* json_chain_next;
    struct json_chain* json_chain_prior;
    int count;
};

/** ClosedCaption数据*/
struct ClosedCaptionDecoder {
    int vfmt;
    int evt;
    int vbi_pgno;
    int flash_stat;
    int timeout;
    bool running;
    bool hide;
    bool render_flag;
    bool need_clear;
    bool auto_detect_play;
    bool auto_set_play_flag;
    bool cea_708_cc_flag;
    int auto_detect_last_708_pgno;
    int auto_detect_last_pgno;
    int auto_detect_pgno_count;
    unsigned int decoder_param;
    char lang[12];
    pthread_t render_thread;
    pthread_t data_thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct tvcc_decoder decoder;

    ClosedCaptionJsonChain_t* json_chain_head;
    ClosedCaptionCreatePara_t cpara;
    ClosedCaptionStartPara_t spara;

    int curr_data_mask;
    int curr_switch_mask;
    int process_update_flag;
    int media_sync_id;
    int player_id;
    uint32_t decoder_cc_pts;
    uint32_t video_pts;
};

int tvcc_to_json (struct tvcc_decoder *td, int pgno, char *buf, size_t len);
int tvcc_empty_json (int pgno, char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
