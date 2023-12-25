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
 * Linux DVB demux driver
 ***************************************************************************/

#define  LOG_TAG "Demux"

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <linux/dvb/dmx.h>
#include <sys/eventfd.h>

#include "SubtitleLog.h"

#include "AmlogicUtil.h"
#include "DemuxDriver.h"

#define open(a...)\
    ({\
     int ret, times=3;\
     do {\
        ret = open(a);\
        if (ret == -1)\
        {\
           usleep(100*1000);\
        }\
     }while(ret==-1 && times--);\
     ret;\
     })

typedef struct
{
    char   dev_name[32];
    int    fd[DEMUX_FILTER_COUNT];
    int    event_fd;
} DVBDmx_t;


static int dvb_open(DemuxDeviceType *dev, const AmlogicDemuxOpenParameterType *para);
static int dvb_close(DemuxDeviceType *dev);
static int dvb_wake(DemuxDeviceType *dev);
static int dvb_alloc_filter(DemuxDeviceType *dev, DemuxFilterType *filter);
static int dvb_free_filter(DemuxDeviceType *dev, DemuxFilterType *filter);
static int dvb_set_sec_filter(DemuxDeviceType *dev, DemuxFilterType *filter, const struct dmx_sct_filter_params *params);
static int dvb_set_pes_filter(DemuxDeviceType *dev, DemuxFilterType *filter, const struct dmx_pes_filter_params *params);
static int dvb_enable_filter(DemuxDeviceType *dev, DemuxFilterType *filter, bool enable);
static int dvb_set_buf_size(DemuxDeviceType *dev, DemuxFilterType *filter, int size);
static int dvb_poll(DemuxDeviceType *dev, Demux_FilterMaskType *mask, int timeout);
static int dvb_read(DemuxDeviceType *dev, DemuxFilterType *filter, uint8_t *buf, int *size);
static int dvb_set_source(DemuxDeviceType *dev, AmlogicDemuxSourceType src);

const DemuxDriverType linux_dvb_dmx_drv = {
.open  = dvb_open,
.close = dvb_close,
.alloc_filter = dvb_alloc_filter,
.free_filter  = dvb_free_filter,
.set_sec_filter = dvb_set_sec_filter,
.set_pes_filter = dvb_set_pes_filter,
.enable_filter  = dvb_enable_filter,
.set_buf_size   = dvb_set_buf_size,
.poll           = dvb_poll,
.read           = dvb_read,
.wake           = dvb_wake,
.set_source     = dvb_set_source
};


/****************************************************************************
 * Static functions
 ***************************************************************************/

static int dvb_open(DemuxDeviceType *dev, const AmlogicDemuxOpenParameterType *para) {
    DVBDmx_t *dmx;
    int i;
    UNUSED(para);
    dmx = (DVBDmx_t*)malloc(sizeof(DVBDmx_t));
    if (!dmx) {
        SUBTITLE_LOGE("not enough memory");
        return DEMUX_ERROR_NO_MEM;
    }
    snprintf(dmx->dev_name, sizeof(dmx->dev_name), "/dev/dvb0.demux%d", dev->dev_no);
    for (i=0; i<DEMUX_FILTER_COUNT; i++)
        dmx->fd[i] = -1;
    dev->drv_data = dmx;
    return AM_SUCCESS;
}

static int dvb_close(DemuxDeviceType *dev) {
    DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
    if (dmx->event_fd >= 0) {
        close(dmx->event_fd);
        dmx->event_fd = -1;
    }
    free(dmx);
    return AM_SUCCESS;
}

static int dvb_alloc_filter(DemuxDeviceType *dev, DemuxFilterType *filter) {
    DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
    int fd;
    int event_fd;
    fd = open(dmx->dev_name, O_RDWR);
    if (fd == -1) {
        SUBTITLE_LOGE("cannot open \"%s\" (%s)", dmx->dev_name, strerror(errno));
        return DEMUX_ERROR_CANNOT_OPEN_DEV;
    }
    dmx->fd[filter->id] = fd;
    event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (event_fd >= 0) {
        dmx->event_fd = event_fd;
    }
    filter->drv_data = (void*)(long)fd;
    return AM_SUCCESS;
}


static int dvb_free_filter(DemuxDeviceType *dev, DemuxFilterType *filter) {
    DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
    int fd = (long)filter->drv_data;
    close(fd);
    dmx->fd[filter->id] = -1;
    return AM_SUCCESS;
}

static int dvb_set_sec_filter(DemuxDeviceType *dev, DemuxFilterType *filter, const struct dmx_sct_filter_params *params) {
    struct dmx_sct_filter_params p;
    int fd = (long)filter->drv_data;
    int ret;
    UNUSED(dev);
    p = *params;
    /*
    if (p.filter.mask[0] == 0) {
        p.filter.filter[0] = 0x00;
        p.filter.mask[0]   = 0x80;
    }
    */
    ret = ioctl(fd, DMX_SET_FILTER, &p);
    if (ret == -1) {
        SUBTITLE_LOGE("set section filter failed (%s)", strerror(errno));
        return DEMUX_ERROR_SYS;
    }
    return AM_SUCCESS;
}

static int dvb_set_pes_filter(DemuxDeviceType *dev, DemuxFilterType *filter, const struct dmx_pes_filter_params *params) {
    int fd = (long)filter->drv_data;
    int ret;
    UNUSED(dev);
    //TODO for coverity check_return
    ret = fcntl(fd,F_SETFL,O_NONBLOCK);
    if (ret == -1) {
        SUBTITLE_LOGE("fcntl failed (%s)", strerror(errno));
        return DEMUX_ERROR_SYS;
    }
    ret = ioctl(fd, DMX_SET_PES_FILTER, params);
    if (ret == -1) {
        SUBTITLE_LOGE("set section filter failed (%s)", strerror(errno));
        return DEMUX_ERROR_SYS;
    }

    return AM_SUCCESS;
}

static int dvb_enable_filter(DemuxDeviceType *dev, DemuxFilterType *filter, bool enable) {
    int fd = (long)filter->drv_data;
    int ret;
    UNUSED(dev);
    if (enable) {
        ret = ioctl(fd, DMX_START, 0);
    } else {
        ret = ioctl(fd, DMX_STOP, 0);
    }
    if (ret == -1) {
        SUBTITLE_LOGE("start filter failed (%s)", strerror(errno));
        return DEMUX_ERROR_SYS;
    }
    return AM_SUCCESS;
}

static int dvb_set_buf_size(DemuxDeviceType *dev, DemuxFilterType *filter, int size) {
    int fd = (long)filter->drv_data;
    int ret;
    UNUSED(dev);
    ret = ioctl(fd, DMX_SET_BUFFER_SIZE, size);
    if (ret == -1) {
        SUBTITLE_LOGE("set buffer size failed (%s)", strerror(errno));
        return DEMUX_ERROR_SYS;
    }
    return AM_SUCCESS;
}

static int dvb_poll(DemuxDeviceType *dev, Demux_FilterMaskType *mask, int timeout) {
    DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
    struct pollfd fds[DEMUX_FILTER_COUNT_EXT];
    int fids[DEMUX_FILTER_COUNT_EXT] = {0};
    int i, cnt = 0, ret;

    for (i = 0; i < DEMUX_FILTER_COUNT; i++) {
        if (dmx->fd[i] != -1) {
            fds[cnt].events = POLLIN|POLLERR;
            fds[cnt].fd     = dmx->fd[i];
            fids[cnt] = i;
            cnt++;
        }
    }

    //add notify event fd
    if (dmx->event_fd != -1) {
        fds[cnt].events = POLLIN|POLLERR;
        fds[cnt].fd     = dmx->event_fd;
        cnt++;
    }
    if (!cnt) return DEMUX_ERROR_TIMEOUT;
    ret = poll(fds, cnt, timeout);
    if (ret <= 0) {
        return DEMUX_ERROR_TIMEOUT;
    }
    for (i = 0; i < cnt; i++) {
        if (fds[i].revents&(POLLIN|POLLERR)) {
            DEMUX_FILTER_MASK_SET(mask, fids[i]);
        }
    }
    return AM_SUCCESS;
}

/*
 *function:@dvb_poll timeout too long, this function can wake up poll advance.
 */
static int dvb_wake(DemuxDeviceType *dev) {
    DVBDmx_t *dmx = (DVBDmx_t*)dev->drv_data;
    uint64_t wdata  = 0;
    int event_fd = dmx->event_fd;
    int ret = write(event_fd, &wdata, 8);
    if (ret != 8) {
        SUBTITLE_LOGE("dvb_wake write %d bytes instead of 8!", ret);
    }
    return AM_SUCCESS;
}

static int dvb_read(DemuxDeviceType *dev, DemuxFilterType *filter, uint8_t *buf, int *size) {
    int fd = (long)filter->drv_data;
    int len = *size;
    int ret;
    struct pollfd pfd;
    UNUSED(dev);
    if (fd == -1) return DEMUX_ERROR_NOT_ALLOCATED;
    pfd.events = POLLIN|POLLERR;
    pfd.fd     = fd;
    ret = poll(&pfd, 1, 0);
    if (ret <= 0) return DEMUX_ERROR_NO_DATA;
    ret = read(fd, buf, len);
    if (ret <= 0) {
        if (errno == ETIMEDOUT) return DEMUX_ERROR_TIMEOUT;
        SUBTITLE_LOGE("read demux failed (%s) %d", strerror(errno), errno);
        return DEMUX_ERROR_SYS;
    }
    *size = ret;
    return AM_SUCCESS;
}

static int dvb_set_source(DemuxDeviceType *dev, AmlogicDemuxSourceType src) {
    char buf[32];
    char *cmd;
    snprintf(buf, sizeof(buf), "/sys/class/stb/demux%d_source", dev->dev_no);
    switch (src) {
        case DEMUX_SRC_TS0:
            cmd = "ts0";
        break;
        case DEMUX_SRC_TS1:
            cmd = "ts1";
        break;
#if defined(CHIP_8226M) || defined(CHIP_8626X)
        case DEMUX_SRC_TS2:
            cmd = "ts2";
        break;
#endif
        case DEMUX_SRC_TS3:
            cmd = "ts3";
        break;
        case DEMUX_SRC_HIU:
            cmd = "hiu";
        break;
        case DEMUX_SRC_HIU1:
            cmd = "hiu1";
        break;
        default:
            SUBTITLE_LOGE("do not support demux source %d", src);
        return DEMUX_ERROR_NOT_SUPPORTED;
    }
    return AM_SUCCESS;
}

