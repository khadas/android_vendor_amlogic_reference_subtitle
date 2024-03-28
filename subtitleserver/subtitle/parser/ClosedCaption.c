#define  LOG_TAG "ClosedCaption"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include "math.h"
#include <stdbool.h>

#include "AmlogicTime.h"
#include "UserdataDriver.h"
#include "AmlogicUtil.h"
#include "AmlogicUtil.h"
#include "ClosedCaption.h"
#include "VbiDriver.h"

#include "SubtitleLog.h"

#include "MediaSyncInterface.h"

#define CC_POLL_TIMEOUT 16
#define CC_FLASH_PERIOD 1000
#define CC_CLEAR_TIME   5000
#define LINE_284_TIMEOUT 4000

#define AMSTREAM_USERDATA_PATH "/dev/amstream_userdata"
#define VBI_DEV_FILE "/dev/vbi"
#define VIDEO_WIDTH_FILE "/sys/class/video/frame_width"
#define VIDEO_HEIGHT_FILE "/sys/class/video/frame_height"
#define FLAG_Q_TONE_DATA 0x0f

#define _TM_T 'V'
struct vout_CCparam_s {
    unsigned int type;
    unsigned char data1;
    unsigned char data2;
};
#define VOUT_IOC_CC_OPEN           _IO(_TM_T, 0x01)
#define VOUT_IOC_CC_CLOSE          _IO(_TM_T, 0x02)
#define VOUT_IOC_CC_DATA           _IOW(_TM_T, 0x03, struct vout_CCparam_s)
#define SAFE_TITLE_AREA_WIDTH (672) /* 16 * 42 */
#define SAFE_TITLE_AREA_HEIGHT (390) /* 26 * 15 */
#define ROW_W (SAFE_TITLE_AREA_WIDTH/42)
#define ROW_H (SAFE_TITLE_AREA_HEIGHT/15)

extern void vbi_decode_caption(vbi_decoder *vbi, int line, const uint8_t *buf);
extern void vbi_refresh_cc(vbi_decoder *vbi);
extern void vbi_caption_reset(vbi_decoder *vbi);

/****************************************************************************
 * Static data
 ***************************************************************************/
static int vout_fd = -1;
static const vbi_opacity opacity_map[CLOSED_CAPTION_OPACITY_MAX] = {
    VBI_OPAQUE,             /*not used, just take a position*/
    VBI_TRANSPARENT_SPACE,  /*CLOSED_CAPTION_OPACITY_TRANSPARENT*/
    VBI_SEMI_TRANSPARENT,   /*CLOSED_CAPTION_OPACITY_TRANSLUCENT*/
    VBI_OPAQUE,             /*CLOSED_CAPTION_OPACITY_SOLID*/
    VBI_OPAQUE,             /*CLOSED_CAPTION_OPACITY_FLASH*/
};

static const vbi_color color_map[CLOSED_CAPTION_COLOR_MAX] = {
    VBI_BLACK, /*not used, just take a position*/
    VBI_WHITE,
    VBI_BLACK,
    VBI_RED,
    VBI_GREEN,
    VBI_BLUE,
    VBI_YELLOW,
    VBI_MAGENTA,
    VBI_CYAN,
};

static int chn_send[CLOSED_CAPTION_MAX];

static int am_cc_calc_caption_size(int *w, int *h) {
    int rows, vw, vh;
    char wbuf[32];
    char hbuf[32];
    int ret;

#if 1
    vw = 0;
    vh = 0;
    ret  = AmlogicFileRead(VIDEO_WIDTH_FILE, wbuf, sizeof(wbuf));
    ret |= AmlogicFileRead(VIDEO_HEIGHT_FILE, hbuf, sizeof(hbuf));
    if (ret != AM_SUCCESS || sscanf(wbuf, "%d", &vw) != 1 || sscanf(hbuf, "%d", &vh) != 1) {
        //SUBTITLE_LOGI("Get video size failed, default set to 16:9");
        vw = 1920;
        vh = 1080;
    }
#else
    vw = 1920;
    vh = 1080;
#endif
    if (vh == 0) {
        vw = 1920;
        vh = 1080;
    }
    rows = (vw * 3 * 32) / (vh * 4);
    if (rows < 32) {
        rows = 32;
    } else if (rows > 42) {
        rows = 42;
    }

    // SUBTITLE_LOGI("Video size: %d X %d, rows %d", vw, vh, rows);
    *w = rows * ROW_W;
    *h = SAFE_TITLE_AREA_HEIGHT;
    return AM_SUCCESS;
}

uint32_t am_cc_get_video_pts(void* media_sync) {
    #define VIDEO_PTS_PATH "/sys/class/tsync/pts_video"
    int64_t value;
    if (media_sync != NULL) {
        MediaSync_getTrackMediaTime(media_sync, &value);
        value = 0xFFFFFFFF & ((9*value)/100);
        return value;
    } else {
        char buffer[16] = {0};
        AmlogicFileRead(VIDEO_PTS_PATH,buffer,16);
        return strtoul(buffer, NULL, 16);
    }
}

static void am_cc_get_page_canvas(ClosedCaptionDecoder_t *cc, struct vbi_page *pg, int* x_point, int* y_point) {
    int safe_width, safe_height;

    am_cc_calc_caption_size(&safe_width, &safe_height);
    if (pg->pgno <= 8) {
        *x_point = 0;
        *y_point = 0;
    } else {
        int x, y, r, b;
        struct dtvcc_service *ds = &cc->decoder.dtvcc.service[pg->pgno- 1 - 8];
        struct dtvcc_window *dw = &ds->window[pg->subno];

        if (dw->anchor_relative) {
            x = dw->anchor_horizontal * safe_width / 100;
            y = dw->anchor_vertical * SAFE_TITLE_AREA_HEIGHT/ 100;
        } else {
            x = dw->anchor_horizontal * safe_width / 210;
            y = dw->anchor_vertical * SAFE_TITLE_AREA_HEIGHT/ 75;
        }

        switch (dw->anchor_point) {
            case 0:
            default:
                break;
            case 1:
                x -= ROW_W*dw->column_count/2;
                break;
            case 2:
                x -= ROW_W*dw->column_count;
                break;
            case 3:
                y -= ROW_H*dw->row_count/2;
                break;
            case 4:
                x -= ROW_W*dw->column_count/2;
                y -= ROW_H*dw->row_count/2;
                break;
            case 5:
                x -= ROW_W*dw->column_count;
                y -= ROW_H*dw->row_count/2;
                break;
            case 6:
                y -= ROW_H*dw->row_count;
                break;
            case 7:
                x -= ROW_W*dw->column_count/2;
                y -= ROW_H*dw->row_count;
                break;
            case 8:
                x -= ROW_W*dw->column_count;
                y -= ROW_H*dw->row_count;
                break;
        }

        r = x + dw->column_count * ROW_W;
        b = y + dw->row_count * ROW_H;

        SUBTITLE_LOGI("x %d, y %d, r %d, b %d", x, y, r, b);

        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (r > safe_width) r = safe_width;
        if (b > SAFE_TITLE_AREA_HEIGHT) b = SAFE_TITLE_AREA_HEIGHT;
        *x_point = x;
        *y_point = y;
    }
}

static void am_cc_override_by_user_options(ClosedCaptionDecoder_t *cc, struct vbi_page *pg) {
    int i, j, opacity;
    vbi_char *ac;

#define OVERRIDE_ATTR(_uo, _uon, _text, _attr, _map)\
    AM_MACRO_BEGIN\
        if (_uo > _uon##_DEFAULT && _uo < _uon##_MAX) {\
            _text->_attr = _map[_uo];\
        }\
    AM_MACRO_END

    for (i=0; i<pg->rows; i++) {
        for (j=0; j<pg->columns; j++) {
            ac = &pg->text[i*pg->columns + j];
            if (ac->unicode == 0x20 && ac->opacity == VBI_TRANSPARENT_SPACE) {
                ac->opacity = (VBI_TRANSPARENT_SPACE<<4) | VBI_TRANSPARENT_SPACE;
            } else {
                /*DTV CC style, override by user options*/
                OVERRIDE_ATTR(cc->spara.user_options.fg_color, CLOSED_CAPTION_COLOR, ac, foreground, color_map);
                OVERRIDE_ATTR(cc->spara.user_options.bg_color, CLOSED_CAPTION_COLOR, ac, background, color_map);
                OVERRIDE_ATTR(cc->spara.user_options.fg_opacity, CLOSED_CAPTION_OPACITY, ac, opacity, opacity_map);
                opacity = ac->opacity;
                OVERRIDE_ATTR(cc->spara.user_options.bg_opacity, CLOSED_CAPTION_OPACITY, ac, opacity, opacity_map);
                ac->opacity = ((opacity&0x0F) << 4) | (ac->opacity&0x0F);

                /*flash control*/
                if (cc->spara.user_options.bg_opacity == CLOSED_CAPTION_OPACITY_FLASH && cc->spara.user_options.fg_opacity != CLOSED_CAPTION_OPACITY_FLASH) {
                    /*only bg flashing*/
                    if (cc->flash_stat == FLASH_SHOW) {
                        ac->opacity &= 0xF0;
                        ac->opacity |= VBI_OPAQUE;
                    } else if (cc->flash_stat == FLASH_HIDE) {
                        ac->opacity &= 0xF0;
                        ac->opacity |= VBI_TRANSPARENT_SPACE;
                    }
                } else if (cc->spara.user_options.bg_opacity != CLOSED_CAPTION_OPACITY_FLASH && cc->spara.user_options.fg_opacity == CLOSED_CAPTION_OPACITY_FLASH) {
                    /*only fg flashing*/
                    if (cc->flash_stat == FLASH_SHOW) {
                        ac->opacity &= 0x0F;
                        ac->opacity |= (VBI_OPAQUE<<4);
                    } else if (cc->flash_stat == FLASH_HIDE) {
                        ac->opacity &= 0x0F;
                        ac->opacity |= (ac->opacity<<4);
                        ac->foreground = ac->background;
                    }
                }
                else if (cc->spara.user_options.bg_opacity == CLOSED_CAPTION_OPACITY_FLASH && cc->spara.user_options.fg_opacity == CLOSED_CAPTION_OPACITY_FLASH) {
                    /*bg & fg both flashing*/
                    if (cc->flash_stat == FLASH_SHOW) {
                        ac->opacity = (VBI_OPAQUE<<4) | VBI_OPAQUE;
                    } else if (cc->flash_stat == FLASH_HIDE) {
                        ac->opacity = (VBI_TRANSPARENT_SPACE<<4) | VBI_TRANSPARENT_SPACE;
                    }
                }
            }
        }
    }
}

#define MAX_DETECT_COUNTS 30
//#define KOREAN_DEFAULT_708_CC
//now think if vbi event callback continuous pano more than 3 times, think have data.
static int am_auto_detect_data_pgno(ClosedCaptionDecoder_t *cc, int pgno) {
    if (!cc->auto_detect_play || cc->auto_set_play_flag) {
        return 0;
    }

//for default 708 cc(pgno >= 9), if both have 608 and 708, 708 only, if only have 608, then 608.
#ifdef KOREAN_DEFAULT_708_CC
    if (pgno >= CLOSED_CAPTION_SERVICE1 && pgno <= CLOSED_CAPTION_SERVICE6) {
        cc->cea_708_cc_flag = true;
    }
    if (cc->cea_708_cc_flag && pgno < CLOSED_CAPTION_SERVICE1) {
        return 0;
    }
    if (cc->cea_708_cc_flag) {//708 smallest pgno
        if (cc->auto_detect_last_708_pgno >= pgno) {
            cc->auto_detect_last_708_pgno = pgno;
            cc->vbi_pgno = pgno;
            cc->spara.caption1 = pgno;
        }
    } else {//608 smallest pgno
        if (cc->auto_detect_last_pgno >= pgno) {
            cc->auto_detect_last_pgno = pgno;
            cc->vbi_pgno = pgno;
            cc->spara.caption1 = pgno;
        }
    }
#else
    if (cc->auto_detect_last_pgno >= pgno) {
        cc->auto_detect_last_pgno = pgno;
        cc->vbi_pgno = pgno;
        cc->spara.caption1 = pgno;
    }
#endif
    cc->auto_detect_pgno_count++;

    if (cc->auto_detect_pgno_count >= MAX_DETECT_COUNTS) {
        cc->auto_set_play_flag = TRUE;
        SUBTITLE_LOGI("cc last play pgno:%d", cc->vbi_pgno);
        return true;
    }
    return 0;
}

static int am_q_tone_data_event(ClosedCaptionDecoder_t *cc, uint8_t * data, int left) {
    if (cc->cpara.q_tone_cb && left > 0) {
        SUBTITLE_LOGI("am_filter_q_tone_data: %s, size:%d", data, left);
        cc->cpara.q_tone_cb(cc, data, left);
        return true;
    }
    return 0;

}

static void am_cc_vbi_event_handler(vbi_event *ev, void *user_data) {
    ClosedCaptionDecoder_t *cc = (ClosedCaptionDecoder_t*)user_data;
    int pgno, subno;
    char* json_buffer;
    ClosedCaptionJsonChain_t* node, *json_chain_head;
    static int count = 0;
    json_chain_head = cc->json_chain_head;
    if (ev->type == VBI_EVENT_CAPTION) {
        if (cc->hide) return;

        /*q_tone event*/
        if (cc->decoder.dtvcc.has_q_tone_data) {
            int len = cc->decoder.dtvcc.index_q_tone;
            am_q_tone_data_event(cc, cc->decoder.dtvcc.q_tone, len);
            for (int i = 0; i < len; i++) {
                cc->decoder.dtvcc.index_q_tone = 0;
            }
            cc->decoder.dtvcc.index_q_tone = 0;
            SUBTITLE_LOGI("q_tone_data event, no need show!");
            return;
        }

        SUBTITLE_LOGI("VBI Caption event: pgno %d, cur_pgno %d, cc->auto_detect_play:%d",
            ev->ev.caption.pgno, cc->vbi_pgno, cc->auto_detect_play);

        pgno = ev->ev.caption.pgno;
        if (pgno < CLOSED_CAPTION_MAX) {
            if (cc->process_update_flag == 0) {
                cc->curr_data_mask   |= 1 << pgno;
                cc->curr_switch_mask |= 1 << pgno;
            }
        }

        if (am_auto_detect_data_pgno(cc, pgno)) {
            SUBTITLE_LOGI("cc auto detect play pgno:%d", pgno);
        }

        pthread_mutex_lock(&cc->lock);
        if (cc->vbi_pgno == ev->ev.caption.pgno && cc->flash_stat == FLASH_NONE) {
            json_buffer = (char*)malloc(JSON_STRING_LENGTH);
            if (!json_buffer) {
                SUBTITLE_LOGI("json buffer malloc failed");
                pthread_mutex_unlock(&cc->lock);
                return;
            }
            /* Convert to json attributes */

/*
 * If the limit vbi_pgno < 6 will cause the excess to not be displayed.
 */
/* coverity[overrun-call:SUPPRESS] */
            int ret;
            ret = tvcc_to_json (&cc->decoder, cc->vbi_pgno, json_buffer, JSON_STRING_LENGTH);
            if (ret == -1) {
                SUBTITLE_LOGI("tvcc_to_json failed");
                if (json_buffer) free(json_buffer);
                pthread_mutex_unlock(&cc->lock);
                return;
            }
            //SUBTITLE_LOGE("---------json: %s %d", json_buffer, count);
            //count ++;
            /* Create one json node */
            node = (ClosedCaptionJsonChain_t*)malloc(sizeof(ClosedCaptionJsonChain_t));
            node->buffer = json_buffer;
            node->pts = cc->decoder_cc_pts;

            SUBTITLE_LOGI("vbi_event node_pts %x decoder_pts %x", node->pts, cc->decoder_cc_pts);
            /* TODO: Add node time */
            AmlogicGetTimeSpec(&node->decode_time);

            /* Add to json chain */
            json_chain_head->json_chain_prior->json_chain_next = node;
            json_chain_head->count++;
            node->json_chain_next = json_chain_head;
            node->json_chain_prior = json_chain_head->json_chain_prior;
            json_chain_head->json_chain_prior = node;

            cc->render_flag = true;
            cc->need_clear = 0;
            cc->timeout = CC_CLEAR_TIME;
        }
        pthread_mutex_unlock(&cc->lock);
        pthread_cond_signal(&cc->cond);
    } else if ((ev->type == VBI_EVENT_ASPECT) || (ev->type == VBI_EVENT_PROG_INFO)) {
        cc->curr_data_mask |= 1 << CLOSED_CAPTION_XDS;
        if (cc->cpara.pinfo_cb) cc->cpara.pinfo_cb(cc, ev->ev.prog_info);
    } else if (ev->type == VBI_EVENT_NETWORK) {
        cc->curr_data_mask |= 1 << CLOSED_CAPTION_XDS;
        if (cc->cpara.network_cb) cc->cpara.network_cb(cc, &ev->ev.network);
    } else if (ev->type == VBI_EVENT_RATING) {
        cc->curr_data_mask |= 1 << CLOSED_CAPTION_XDS;
        if (cc->cpara.rating_cb)  cc->cpara.rating_cb(cc, &ev->ev.prog_info->rating);
    }
}

static void dump_cc_data(uint8_t *buff, int size) {
    int i;
    char buf[4096];

    if (size > 1024)
        size = 1024;
    for (i=0; i<size; i++) {
        sprintf(buf+i*3, "%02x ", buff[i]);
    }
    SUBTITLE_LOGI("debug-cc dump_cc_data len: %d data:%s", size, buf);
}

static void am_cc_clear(ClosedCaptionDecoder_t *cc) {
    ClosedCaptionDrawPara_t draw_para;
    ClosedCaptionJsonChain_t *node, *node_to_clear, *json_chain_head;
    struct vbi_page sub_page;

    /* Clear all node */
    json_chain_head = cc->json_chain_head;
    node = json_chain_head->json_chain_next;
    while (node != json_chain_head) {
        node_to_clear = node;
        node = node->json_chain_next;
        free(node_to_clear->buffer);
        free(node_to_clear);
    }
    json_chain_head->json_chain_next = json_chain_head;
    json_chain_head->json_chain_prior = json_chain_head;

    if (cc->cpara.json_buffer) tvcc_empty_json(cc->vbi_pgno, cc->cpara.json_buffer, 8096);
    if (cc->cpara.json_update) cc->cpara.json_update(cc);
    SUBTITLE_LOGI("am_cc_clear");
}

/*
 * If has cc data to render, return 1
 * else return 0
 */
static int am_cc_render(ClosedCaptionDecoder_t *cc) {
    ClosedCaptionDrawPara_t draw_para;
    struct vbi_page sub_pages[8];
    int sub_pg_cnt, i;
    int status = 0;
    static int count = 0;
    ClosedCaptionJsonChain_t* node, *node_reset, *json_chain_head;
    struct timespec now;
    int has_data_to_render;
    int32_t decode_time_gap = 0;
    int ret;

    if (cc->hide)
        return 0;
    has_data_to_render = 0;

    if (am_cc_calc_caption_size(&draw_para.caption_width,
        &draw_para.caption_height) != AM_SUCCESS)
        return 0;
    //if (cc->cpara.draw_begin)
    //  cc->cpara.draw_begin(cc, &draw_para);
    json_chain_head = cc->json_chain_head;
    AmlogicGetTimeSpec(&now);
    /* Fetch one json from chain */
    node = json_chain_head->json_chain_next;

    while (node != json_chain_head)
    {
        //100 indicates analog which does not need to use pts
        if (cc->vfmt != 100)
        {
            decode_time_gap = (int32_t)(node->pts - cc->video_pts);
            //do sync in 5secs time gap
            if (decode_time_gap > 0 && decode_time_gap < 450000)
            {
                SUBTITLE_LOGI("render_thread pts gap large than 0, value %d", decode_time_gap);
                has_data_to_render = 0;
                node = node->json_chain_next;
                break;
            }
            SUBTITLE_LOGI("render_thread pts in range, node->pts %x videopts %x", node->pts, cc->video_pts);
        }

        if (cc->cpara.json_buffer)
        {
            memcpy(cc->cpara.json_buffer, node->buffer, JSON_STRING_LENGTH);
            has_data_to_render = 1;
        }
        SUBTITLE_LOGI("json--> %s", node->buffer);
        if (cc->cpara.json_update && has_data_to_render)
            cc->cpara.json_update(cc);

        //SUBTITLE_LOGE("Render json: %s, %d", node->buffer, count);
        //count++;
CLEAN_NODE:
        node_reset = node;
        node->json_chain_prior->json_chain_next = node->json_chain_next;
        node->json_chain_next->json_chain_prior = node->json_chain_prior;
        node = node->json_chain_next;
        json_chain_head->count--;
        if (node_reset->buffer)
            free(node_reset->buffer);
        free(node_reset);
        //if (has_data_to_render == 1)
        //  break;
    }


    //if (cc->cpara.draw_end)
    //  cc->cpara.draw_end(cc, &draw_para);
    // SUBTITLE_LOGI("render_thread render exit, has_data %d chain_left %d", has_data_to_render,json_chain_head->count);
    return has_data_to_render;
}

static void am_cc_set_tv(const uint8_t *buf, unsigned int n_bytes) {
    int cc_flag;
    int cc_count;
    int i;

    cc_flag = buf[1] & 0x40;
    if (!cc_flag) {
        SUBTITLE_LOGI("cc_flag is invalid, %d", n_bytes);
        return;
    }
    cc_count = buf[1] & 0x1f;
    for (i = 0; i < cc_count; ++i) {
        unsigned int b0;
        unsigned int cc_valid;
        unsigned int cc_type;
        unsigned char cc_data1;
        unsigned char cc_data2;

        b0 = buf[3 + i * 3];
        cc_valid = b0 & 4;
        cc_type = b0 & 3;
        cc_data1 = buf[4 + i * 3];
        cc_data2 = buf[5 + i * 3];

        if (cc_type == 0 || cc_type == 1) { //NTSC pair
            struct vout_CCparam_s cc_param;
            cc_param.type = cc_type;
            cc_param.data1 = cc_data1;
            cc_param.data2 = cc_data2;
            //SUBTITLE_LOGI("cc_type:%#x, write cc data: %#x, %#x", cc_type, cc_data1, cc_data2);
            if (ioctl(vout_fd, VOUT_IOC_CC_DATA, &cc_param)== -1) SUBTITLE_LOGE("ioctl VOUT_IOC_CC_DATA failed, error:%s", strerror(errno));
            if (!cc_valid || i >= 3) break;
        }
    }
}

static void solve_vbi_data (ClosedCaptionDecoder_t *cc, struct vbi_data_s *vbi) {
    int line;
    if (cc == NULL || vbi == NULL) return;
    line = vbi->line_num;
    // if (line != 21) return;
    // if (vbi->field_id == VBI_FIELD_2) line = 284;
    SUBTITLE_LOGI("VBI solve_vbi_data line %d: %02x %02x", line, vbi->b[0], vbi->b[1]);
    vbi_decode_caption(cc->decoder.vbi, line, vbi->b);
}

/** VBI data thread.*/
static void *am_vbi_data_thread(void *arg) {

    ClosedCaptionDecoder_t *cc = (ClosedCaptionDecoder_t*)arg;
    struct vbi_data_s  vbi[50];
    int fd;
    int type = VBI_TYPE_USCC;
    int now, last;
    int last_data_mask = 0;
    int timeout = cc->cpara.data_timeout * 2;

    fd = open(VBI_DEV_FILE, O_RDWR);
    if (fd == -1) {
        SUBTITLE_LOGE("cannot open \"%s\"", VBI_DEV_FILE);
        return NULL;
    }

    if (ioctl(fd, VBI_IOC_SET_TYPE, &type )== -1) SUBTITLE_LOGI("VBI_IOC_SET_TYPE error:%s", strerror(errno));
    if (ioctl(fd, VBI_IOC_START) == -1) SUBTITLE_LOGI("VBI_IOC_START error:%s", strerror(errno));

    AmlogicGetClock(&last);

    while (cc->running) {
        struct pollfd pfd;
        int           ret;

        pfd.fd     = fd;
        pfd.events = POLLIN;

        ret = poll(&pfd, 1, CC_POLL_TIMEOUT);

        if (cc->running && (ret > 0) && (pfd.events & POLLIN)) {
            struct vbi_data_s *pd;

            ret = read(fd, vbi, sizeof(vbi));
            pd  = vbi;
            //SUBTITLE_LOGI("am_vbi_data_thread running read data == %d",ret);

            if (ret >= (int)sizeof(struct vbi_data_s)) {
                while (ret >= (int)sizeof(struct vbi_data_s)) {
                    solve_vbi_data(cc, pd);
                    pd ++;
                    ret -= sizeof(struct vbi_data_s);
                }
            } else {
                cc->process_update_flag = 1;
                vbi_refresh_cc(cc->decoder.vbi);
                cc->process_update_flag = 0;
            }
        } else {
            cc->process_update_flag = 1;
            vbi_refresh_cc(cc->decoder.vbi);
            cc->process_update_flag = 0;
        }

        AmlogicGetClock(&now);

        if (cc->curr_data_mask) {
            cc->curr_data_mask |= (1 << CLOSED_CAPTION_DATA);
        }

        if (cc->curr_data_mask & (1 << CLOSED_CAPTION_DATA)) {
            if (!(last_data_mask & (1 << CLOSED_CAPTION_DATA))) {
                last_data_mask |= (1 << CLOSED_CAPTION_DATA);
                if (cc->cpara.data_cb) {
                    cc->cpara.data_cb(cc, last_data_mask);
                }
            }
        }

        if (now - last >= timeout) {
            if (last_data_mask != cc->curr_data_mask) {
                if (cc->cpara.data_cb) {
                    cc->cpara.data_cb(cc, cc->curr_data_mask);
                }
                //SUBTITLE_LOGI("CC data mask 0x%x -> 0x%x", last_data_mask, cc->curr_data_mask);
                last_data_mask = cc->curr_data_mask;
            }
            cc->curr_data_mask = 0;
            last = now;
            timeout = cc->cpara.data_timeout;
        }
        cc->process_update_flag = 1;
        vbi_refresh_cc(cc->decoder.vbi);
        cc->process_update_flag = 0;
    }

    SUBTITLE_LOGI("am_vbi_data_thread exit");
    ioctl(fd, VBI_IOC_STOP);
    close(fd);
    return NULL;
}

/** CC data thread*/
static void *am_cc_data_thread(void *arg) {
    ClosedCaptionDecoder_t *cc = (ClosedCaptionDecoder_t*)arg;
    uint8_t cc_buffer[5*1024];/*In fact, 99 is enough*/
    uint8_t *cc_data;
    int cnt, left, fd, cc_data_cnt;
    struct pollfd fds;
    int last_pkg_idx = -1, pkg_idx;
    int ud_dev_no = 0;
    UserdataOpenParaType para;
    int now, last, last_switch;
    int last_data_mask = 0;
    int last_switch_mask = 0;
    uint32_t *cc_pts;

    char display_buffer[8192];
    int i;
    int mode = 0;
    SUBTITLE_LOGE("CC data thread start.");
    memset(&para, 0, sizeof(para));
    para.vfmt = cc->vfmt;
    para.playerid = cc->player_id;
    para.mediasyncid = cc->media_sync_id;
    SUBTITLE_LOGI("am_cc_data_thread vfmt:%d playerid:%d mediasyncid:%d", para.vfmt, para.playerid, para.mediasyncid);
    /* Start the cc data */
    if (UserdataOpen(ud_dev_no, &para) != AM_SUCCESS) {
        SUBTITLE_LOGE("Cannot open userdata device %d", ud_dev_no);
        return NULL;
    }

    AmlogicGetClock(&last);
    last_switch = last;
    UserdataGetMode(0, &mode);
    UserdataSetMode(0, mode | USERDATA_MODE_CC);
    int index = 1;
    while (cc->running) {
        cc_data_cnt = UserdataRead(ud_dev_no, cc_buffer, sizeof(cc_buffer), CC_POLL_TIMEOUT);
        /* There is a poll action in userdata read */
        if (!cc->running)
            break;
        cc_data = cc_buffer + 4;
        cc_data_cnt -= 4;
        //dump_cc_data(cc_buffer, cc_data_cnt);
        if (cc_data_cnt > 8 && cc_data[0] == 0x47 && cc_data[1] == 0x41 && cc_data[2] == 0x39 && cc_data[3] == 0x34) {
            cc_pts = cc_buffer;
            cc->decoder_cc_pts = *cc_pts;
            SUBTITLE_LOGI("cc_data_thread mpeg cc_count %d pts %x", cc_data_cnt,*cc_pts);
            //dump_cc_data(cc_data, cc_data_cnt);

            if (cc_data[4] != 0x03 /* 0x03 indicates cc_data */) {
                SUBTITLE_LOGI("Unprocessed user_data_type_code 0x%02x, we only expect 0x03", cc_data[4]);
                continue;
            }

            if (vout_fd != -1) am_cc_set_tv(cc_data+4, cc_data_cnt-4);
            SUBTITLE_LOGI("debug-cc index:%d frame", index);
            dump_cc_data(cc_data, cc_data_cnt);
            /*decode this cc data*/
            tvcc_decode_data(&cc->decoder, 0, cc_data+4, cc_data_cnt-4);
            SUBTITLE_LOGI("debug-cc index:%d frame end !", index);
            index++;
        } else if (cc_data_cnt > 4 && cc_data[0] == 0xb5 && cc_data[1] == 0x00 && cc_data[2] == 0x2f ) {
            //dump_cc_data(cc_data+4, cc_data_cnt-4);
            //direct format
            if (cc_data[3] != 0x03 /* 0x03 indicates cc_data */) {
                SUBTITLE_LOGE("Unprocessed user_data_type_code 0x%02x, we only expect 0x03", cc_data[3]);
                continue;
            }
            cc_data[4] = cc_data[3];// use user_data_type_code in place of user_data_code_length  for extract code
            if (vout_fd != -1) am_cc_set_tv(cc_data+4, cc_data_cnt-4);

            /*decode this cc data*/
            tvcc_decode_data(&cc->decoder, 0, cc_data+4, cc_data_cnt-4);
        }

        cc->process_update_flag = 1;
        update_service_status(&cc->decoder);
        vbi_refresh_cc(cc->decoder.vbi);
        cc->process_update_flag = 0;

        AmlogicGetClock(&now);

        if (cc->curr_data_mask) {
            cc->curr_data_mask |= (1 << CLOSED_CAPTION_DATA);
        }

        if (cc->curr_data_mask & (1 << CLOSED_CAPTION_DATA)) {
            if (!(last_data_mask & (1 << CLOSED_CAPTION_DATA))) {
                last_data_mask |= (1 << CLOSED_CAPTION_DATA);
                if (cc->cpara.data_cb) {
                    cc->cpara.data_cb(cc, last_data_mask);
                }
            }
        }

        if (now - last >= cc->cpara.data_timeout) {
            if (last_data_mask != cc->curr_data_mask) {
                if (cc->cpara.data_cb) {
                    cc->cpara.data_cb(cc, cc->curr_data_mask);
                }
                SUBTITLE_LOGI("CC data mask 0x%x -> 0x%x", last_data_mask, cc->curr_data_mask);
                last_data_mask = cc->curr_data_mask;
            }
            cc->curr_data_mask = 0;
            last = now;
        }

        if (now - last_switch >= cc->cpara.switch_timeout) {
            if (last_switch_mask != cc->curr_switch_mask) {
                if (!(cc->curr_switch_mask & (1 << cc->vbi_pgno))) {
                    if ((cc->vbi_pgno == cc->spara.caption1) && (cc->spara.caption2 != CLOSED_CAPTION_NONE)) {
                        SUBTITLE_LOGI("CC switch %d -> %d", cc->vbi_pgno, cc->spara.caption2);
                        cc->vbi_pgno = cc->spara.caption2;
                    } else if ((cc->vbi_pgno == cc->spara.caption2) && (cc->spara.caption1 != CLOSED_CAPTION_NONE)) {
                        SUBTITLE_LOGI("CC switch %d -> %d", cc->vbi_pgno, cc->spara.caption1);
                        cc->vbi_pgno = cc->spara.caption1;
                    }
                }
                last_switch_mask = cc->curr_switch_mask;
            }
            cc->curr_switch_mask = 0;
            last_switch = now;
        }
    }
    /*Stop the cc data*/
    UserdataClose(ud_dev_no);
    SUBTITLE_LOGE("CC data thread exit now");
    return NULL;
}

/** CC rendering thread*/
static void *am_cc_render_thread(void *arg) {
    ClosedCaptionDecoder_t *cc = (ClosedCaptionDecoder_t*)arg;
    struct timespec ts;
    int cnt, timeout, ret;
    int last, now;
    int nodata = 0;
    char vpts_buf[16];
    uint32_t vpts;
    int32_t pts_gap_cc_video;
    int32_t vpts_gap;
    ClosedCaptionJsonChain_t *node;
    /* Get fps of video */
    int fps;
    AmlogicGetClock(&last);
    pthread_mutex_lock(&cc->lock);
    timeout = 10;
    void* media_sync = NULL;
    if (cc->media_sync_id >= 0) {
        media_sync = MediaSync_create();
        if (NULL != media_sync) {
            MediaSync_bindInstance(media_sync, cc->media_sync_id, MEDIA_SUBTITLE);
        }
    }
    while (cc->running) {
        // Update video pts
        node = cc->json_chain_head->json_chain_next;
        if (node == cc->json_chain_head) node = NULL;
        vpts = am_cc_get_video_pts(media_sync);
        // If has cc data in chain, set gap time to timeout
        if (node) {
            pts_gap_cc_video = (int32_t)(node->pts - vpts);
            if (pts_gap_cc_video <= 0) {
                SUBTITLE_LOGI("pts gap less than 0, node pts %x vpts %x, gap %x", node->pts, vpts, pts_gap_cc_video);
                timeout = 0;
            } else {
                timeout = pts_gap_cc_video*1000/90000;
                SUBTITLE_LOGI("render_thread timeout node_pts %x vpts %x gap %d calculate %d", node->pts, vpts, pts_gap_cc_video, timeout);
                // Set timeout to 1 second If pts gap is more than 1 second.
                // We need to judge if video is continuous
                timeout = (timeout>10)?10:timeout;
            }
        } else {
            // SUBTITLE_LOGI("render_thread no node in chain, timeout set to 1000");
            timeout = 10;
        }

        AmlogicGetTimeSpecTimeout(timeout, &ts);
        if (cc->running)
            pthread_cond_timedwait(&cc->cond, &cc->lock, &ts);
        else
            break;

        vpts = am_cc_get_video_pts(media_sync);
        cc->video_pts = vpts;
        // Judge if video pts gap is in valid range
        if (node) {
            vpts_gap = (int32_t)(node->pts - cc->video_pts);
            SUBTITLE_LOGI("render_thread after timeout node_pts %x vpts %x gap %d calculate %d",
                node->pts, vpts, vpts_gap, timeout);
        }

        //if (!nodata)
        am_cc_render(cc);
        AmlogicGetClock(&now);
        if (cc->curr_data_mask & (1 << cc->vbi_pgno)) {
            nodata = 0;
            last   = now;
        } else if ((now - last) > 10000 && nodata ==0) {
            last = now;
            SUBTITLE_LOGI("cc render thread: No data now.");
            if ((cc->vbi_pgno < CLOSED_CAPTION_TEXT1) || (cc->vbi_pgno > CLOSED_CAPTION_TEXT4)) {
                am_cc_clear(cc);
                if (cc->vbi_pgno < CLOSED_CAPTION_TEXT1)
                    vbi_caption_reset(cc->decoder.vbi);
                nodata = 1;
            }
        }
    }
    am_cc_clear(cc);
    if (media_sync != NULL) {
        MediaSync_destroy(media_sync);
    }
    pthread_mutex_unlock(&cc->lock);
    //close(fd);
    SUBTITLE_LOGE("CC rendering thread exit now");
    return NULL;
}

/** Create CC
 */
int ClosedCaptionCreate(ClosedCaptionCreatePara_t *para, ClosedCaptionHandleType *handle) {
    ClosedCaptionDecoder_t *cc;
    if (para == NULL || handle == NULL) return CLOSED_CAPTION_ERROR_INVALID_PARAM;
    cc = (ClosedCaptionDecoder_t*)malloc(sizeof(ClosedCaptionDecoder_t));
    if (cc == NULL) return CLOSED_CAPTION_ERROR_NO_MEM;
    if (para->bypass_cc_enable) {
        vout_fd= open("/dev/tv", O_RDWR);
        if (vout_fd == -1) {
            SUBTITLE_LOGI("open vdin error");
        }
    }
    memset(cc, 0, sizeof(ClosedCaptionDecoder_t));
    cc->json_chain_head = (ClosedCaptionJsonChain_t*) calloc (sizeof(ClosedCaptionJsonChain_t), 1);
    if (cc->json_chain_head == NULL) {
        free (cc);
        return CLOSED_CAPTION_ERROR_NO_MEM;
    }
    cc->json_chain_head->json_chain_next = cc->json_chain_head;
    cc->json_chain_head->json_chain_prior = cc->json_chain_head;
    cc->json_chain_head->count = 0;
    cc->decoder_param = para->decoder_param;
    strncpy(cc->lang, para->lang, 10);
    /* init the tv cc decoder */
    tvcc_init(&cc->decoder, para->lang, 10, para->decoder_param);
    if (cc->decoder.vbi == NULL) {
        free(cc);
        return CLOSED_CAPTION_ERROR_LIBZVBI;
    }
    vbi_event_handler_register(cc->decoder.vbi, VBI_EVENT_CAPTION|VBI_EVENT_ASPECT |VBI_EVENT_PROG_INFO|VBI_EVENT_NETWORK |VBI_EVENT_RATING, am_cc_vbi_event_handler, cc);
    pthread_mutex_init(&cc->lock, NULL);
    pthread_cond_init(&cc->cond, NULL);
    cc->cpara = *para;
    *handle = cc;
    AM_SigHandlerInit();
    return AM_SUCCESS;
}

/** Destroy CC
 */
int ClosedCaptionDestroy(ClosedCaptionHandleType handle) {
    ClosedCaptionDecoder_t *cc = (ClosedCaptionDecoder_t*)handle;
    if (vout_fd != -1) close(vout_fd);
    if (cc == NULL) return CLOSED_CAPTION_ERROR_INVALID_PARAM;
    ClosedCaptionStop(handle);
    tvcc_destroy(&cc->decoder);
    pthread_mutex_destroy(&cc->lock);
    pthread_cond_destroy(&cc->cond);
    if (cc->json_chain_head) free(cc->json_chain_head);
    free(cc);
    SUBTITLE_LOGI("am_cc_destroy ok");
    return AM_SUCCESS;
}

/** Start CC data reception processing
 */
int ClosedCaptionStart(ClosedCaptionHandleType handle, ClosedCaptionStartPara_t *para) {
    ClosedCaptionDecoder_t *cc = (ClosedCaptionDecoder_t*)handle;
    int rc, ret = AM_SUCCESS;
    if (cc == NULL || para == NULL) return CLOSED_CAPTION_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&cc->lock);
    if (cc->running) {
        ret = CLOSED_CAPTION_ERROR_BUSY;
        goto start_done;
    }

    // Not select a valid cc channel, enable auto detect.
    if (para->caption1 <= CLOSED_CAPTION_DEFAULT || para->caption1 >= CLOSED_CAPTION_MAX) {
        para->auto_detect_play = 1;
    }

    SUBTITLE_LOGE("ClosedCaptionStart vfmt %d para->caption1=%d para->caption2=%d", para->vfmt, para->caption1, para->caption2);

    cc->vfmt = para->vfmt;
    cc->evt = -1;
    cc->spara = *para;
    cc->vbi_pgno = para->caption1;
    cc->auto_detect_play = para->auto_detect_play;
    cc->auto_detect_last_pgno = CLOSED_CAPTION_MAX;
    cc->auto_detect_last_708_pgno = CLOSED_CAPTION_MAX;
    cc->running = true;
    cc->media_sync_id = para->mediaysnc_id;
    cc->player_id = para->player_id;

    cc->curr_switch_mask = 0;
    cc->curr_data_mask   = 0;
    cc->video_pts = 0;

    /* start the rendering thread */
    rc = pthread_create(&cc->render_thread, NULL, am_cc_render_thread, (void*)cc);
    if (rc) {
        cc->running = 0;
        SUBTITLE_LOGI("%s:%s", __func__, strerror(rc));
        ret = CLOSED_CAPTION_ERROR_SYS;
    } else {
        /* start the data source thread */
        if (cc->cpara.input == CLOSED_CAPTION_INPUT_VBI) {
            rc = pthread_create(&cc->data_thread, NULL, am_vbi_data_thread, (void*)cc);
        } else {
            rc = pthread_create(&cc->data_thread, NULL, am_cc_data_thread, (void*)cc);
        }
        if (rc) {
            cc->running = 0;
            pthread_join(cc->render_thread, NULL);
            SUBTITLE_LOGI("%s:%s", __func__, strerror(rc));
            ret = CLOSED_CAPTION_ERROR_SYS;
        }
    }

start_done:
    pthread_mutex_unlock(&cc->lock);
    return ret;
}

/** Stop CC processing
 */
int ClosedCaptionStop(ClosedCaptionHandleType handle) {
    ClosedCaptionDecoder_t *cc = (ClosedCaptionDecoder_t*)handle;
    int ret = AM_SUCCESS;
    bool join = 0;
    if (cc == NULL) return CLOSED_CAPTION_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&cc->lock);
    if (cc->running) {
        cc->running = 0;
        join = true;
    }
    pthread_mutex_unlock(&cc->lock);
    pthread_cond_broadcast(&cc->cond);
    pthread_kill(cc->data_thread, SIGALRM);
    if (join) {
        pthread_join(cc->data_thread, NULL);
        pthread_join(cc->render_thread, NULL);
    }
    SUBTITLE_LOGI("am_cc_stop ok");
    return ret;
}
