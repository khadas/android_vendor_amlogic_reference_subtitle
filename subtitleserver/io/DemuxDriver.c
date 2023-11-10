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
 ***************************************************************************/

#define  LOG_TAG "DemuxDriver"
#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include "SubtitleLog.h"

#include "DemuxDriver.h"
#include "AmlogicUtil.h"

#define DMX_SYNC

#define DMX_BUF_SIZE       (4096)
#define DMX_POLL_TIMEOUT   (200)
#ifdef CHIP_8226H
#define DMX_DEV_COUNT      (16)
#elif defined(CHIP_8226M) || defined(CHIP_8626X)
#define DMX_DEV_COUNT      (16)
#else
#define DMX_DEV_COUNT      (16)
#endif

#define DMX_CHAN_ISSET_FILTER(chan,fid)    ((chan)->filter_mask[(fid)>>3]&(1<<((fid)&3)))
#define DMX_CHAN_SET_FILTER(chan,fid)      ((chan)->filter_mask[(fid)>>3]|=(1<<((fid)&3)))
#define DMX_CHAN_CLR_FILTER(chan,fid)      ((chan)->filter_mask[(fid)>>3]&=~(1<<((fid)&3)))

/****************************************************************************
 * Static data
 ***************************************************************************/

#ifdef EMU_DEMUX
extern const DemuxDriverType emu_dmx_drv;
#define HW_DMX_DRV emu_dmx_drv
#else
extern const DemuxDriverType linux_dvb_dmx_drv;
#define HW_DMX_DRV linux_dvb_dmx_drv
#endif
//extern const DemuxDriverType dvr_dmx_drv;
//#define SW_DMX_DRV dvr_dmx_drv

static DemuxDeviceType dmx_devices[DMX_DEV_COUNT] =
{
#ifdef EMU_DEMUX
{
.drv = &emu_dmx_drv,
.src = DEMUX_SRC_TS0
},
{
.drv = &emu_dmx_drv,
.src = DEMUX_SRC_TS0
}
#else
{
.drv = &linux_dvb_dmx_drv,
.src = DEMUX_SRC_TS0
},
{
.drv = &linux_dvb_dmx_drv,
.src = DEMUX_SRC_TS0
}
//#if defined(CHIP_8226M) || defined(CHIP_8626X)
,
{
.drv = &linux_dvb_dmx_drv,
.src = DEMUX_SRC_TS0
}
//#endif
#endif
};

/****************************************************************************
 * Static functions
 ***************************************************************************/

/** Get the device structure pointer based on the device number*/
static inline int dmx_get_dev(int dev_no, DemuxDeviceType **dev) {
    if ((dev_no < 0) || (dev_no >= DMX_DEV_COUNT)) {
        SUBTITLE_LOGI("invalid demux device number %d, must in(%d~%d)", dev_no, 0, DMX_DEV_COUNT-1);
        return DEMUX_ERROR_INVALID_DEV_NO;
    }
    *dev = &dmx_devices[dev_no];
    return AM_SUCCESS;
}

/** Get the device structure based on the device number and check whether the device is open*/
static inline int dmx_get_opened_dev(int dev_no, DemuxDeviceType **dev) {
    AM_TRY(dmx_get_dev(dev_no, dev));

    if ((*dev)->open_count <= 0) {
        SUBTITLE_LOGI("demux device %d has not been opened", dev_no);
        return DEMUX_ERROR_INVALID_DEV_NO;
    }
    return AM_SUCCESS;
}

/** Get the corresponding filter structure based on the ID and check whether the device is in use*/
static inline int dmx_get_used_filter(DemuxDeviceType *dev, int filter_id, DemuxFilterType **pf) {
    DemuxFilterType *filter;
    if ((filter_id < 0) || (filter_id >= DEMUX_FILTER_COUNT)) {
        SUBTITLE_LOGI("invalid filter id, must in %d~%d", 0, DEMUX_FILTER_COUNT-1);
        return DEMUX_ERROR_INVALID_ID;
    }
    filter = &dev->filters[filter_id];
    if (!filter->used) {
        SUBTITLE_LOGI("filter %d has not been allocated", filter_id);
        return DEMUX_ERROR_NOT_ALLOCATED;
    }
    *pf = filter;
    return AM_SUCCESS;
}

/** Data detection thread*/
static void* dmx_data_thread(void *arg) {
    DemuxDeviceType *dev = (DemuxDeviceType*)arg;
    uint8_t *sec_buf;
    uint8_t *sec;
    int sec_len;
    Demux_FilterMaskType mask;
    int ret;

    #define BUF_SIZE (4096)
    sec_buf = (uint8_t*)malloc(BUF_SIZE);
    //for coverity
    if (sec_buf) {
        memset(sec_buf, 0, BUF_SIZE);
    }
    while (dev->enable_thread) {
        DEMUX_FILTER_MASK_CLEAR(&mask);
        uint32_t id;

        ret = dev->drv->poll(dev, &mask, DMX_POLL_TIMEOUT);
        if (ret == AM_SUCCESS) {
            if (DEMUX_FILTER_MASK_ISEMPTY(&mask))
                continue;

            #if defined(DMX_WAIT_CB) || defined(DMX_SYNC)
            pthread_mutex_lock(&dev->lock);
            dev->flags |= DMX_FL_RUN_CB;
            pthread_mutex_unlock(&dev->lock);
            #endif

            for (id=0; id < DEMUX_FILTER_COUNT; id++) {
                DemuxFilterType *filter=&dev->filters[id];
                DemuxDataCb cb;
                void *data;

                if (!DEMUX_FILTER_MASK_ISSET(&mask, id))
                    continue;

                if (!filter->enable || !filter->used)
                    continue;

                sec_len = BUF_SIZE;

                #ifndef DMX_WAIT_CB
                pthread_mutex_lock(&dev->lock);
                #endif
                if (!filter->enable || !filter->used) {
                    ret = AM_FAILURE;
                } else {
                    cb   = filter->cb;
                    data = filter->user_data;
                    ret  = dev->drv->read(dev, filter, sec_buf, &sec_len);
                }
                #ifndef DMX_WAIT_CB
                pthread_mutex_unlock(&dev->lock);
                #endif
                if (ret == DEMUX_ERROR_TIMEOUT) {
                    sec = NULL;
                    sec_len = 0;
                } else if (ret!=AM_SUCCESS) {
                    continue;
                } else {
                    sec = sec_buf;
                }

                if (cb) {
                    if (id && sec)
                    SUBTITLE_LOGI("filter %d data callback len fd:%ld len:%d, %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                        id, (long)filter->drv_data, sec_len,
                        sec[0], sec[1], sec[2], sec[3], sec[4],
                        sec[5], sec[6], sec[7], sec[8], sec[9]);
                    cb(dev->dev_no, id, sec, sec_len, data);
                    if (id && sec)
                    SUBTITLE_LOGI("filter %d data callback ok", id);
                }
            }
            #if defined(DMX_WAIT_CB) || defined(DMX_SYNC)
            pthread_mutex_lock(&dev->lock);
            dev->flags &= ~DMX_FL_RUN_CB;
            pthread_mutex_unlock(&dev->lock);
            pthread_cond_broadcast(&dev->cond);
            #endif
        } else {
            usleep(10000);
        }
    }

    if (sec_buf) {
        free(sec_buf);
    }
    return NULL;
}

/** Wait for the callback function to stop running*/
static inline int dmx_wait_cb(DemuxDeviceType *dev)
{
#ifdef DMX_WAIT_CB
    if (dev->thread != pthread_self()) {
        while (dev->flags & DMX_FL_RUN_CB)
            pthread_cond_wait(&dev->cond, &dev->lock);
    }
#else
    UNUSED(dev);
#endif
    return AM_SUCCESS;
}

/** Stop Section filter*/
static int dmx_stop_filter(DemuxDeviceType *dev, DemuxFilterType *filter) {
    int ret = AM_SUCCESS;
    if (!filter->used || !filter->enable) return ret;
    if (dev->drv->enable_filter) ret = dev->drv->enable_filter(dev, filter, 0);
    if (ret >= 0) filter->enable = 0;
    return ret;
}

/** release filter*/
static int dmx_free_filter(DemuxDeviceType *dev, DemuxFilterType *filter) {
    int ret = AM_SUCCESS;
    if (!filter->used) return ret;
    ret = dmx_stop_filter(dev, filter);
    if (ret == AM_SUCCESS) {
        if (dev->drv->free_filter) {
            ret = dev->drv->free_filter(dev, filter);
        }
    }
    if (ret == AM_SUCCESS) {
        filter->used=0;
    }
    return ret;
}

/****************************************************************************
 * API functions
 ***************************************************************************/

/** Open the demultiplexing device
  * dev_no demultiplexing device number
  * \param[in] para Demultiplexing device opening parameters

  * - AM_SUCCESS Success
  * - other values error code (see DemuxDriver.h)
  */
int DemuxOpen(int dev_no, const AmlogicDemuxOpenParameterType *para) {
    DemuxDeviceType *dev;
    int ret = AM_SUCCESS;
    assert(para);
    AM_TRY(dmx_get_dev(dev_no, &dev));
    pthread_mutex_lock(&am_gAdpLock);
    if (dev->open_count > 0) {
        SUBTITLE_LOGI("demux device %d has already been opened", dev_no);
        dev->open_count++;
        ret = AM_SUCCESS;
        goto final;
    }

    dev->dev_no = dev_no;

    /*if(para->use_sw_filter){
        dev->drv = &SW_DMX_DRV;
    }else{
        dev->drv = &HW_DMX_DRV;
    }*/
    //default use hw dmx
    dev->drv = &HW_DMX_DRV;
    if (dev->drv->open) {
        ret = dev->drv->open(dev, para);
    }

    if (ret == AM_SUCCESS) {
        pthread_mutex_init(&dev->lock, NULL);
        pthread_cond_init(&dev->cond, NULL);
        dev->enable_thread = true;
        pthread_mutex_lock(&dev->lock);
        dev->flags = 0;
        pthread_mutex_unlock(&dev->lock);

        if (pthread_create(&dev->thread, NULL, dmx_data_thread, dev)) {
            pthread_mutex_destroy(&dev->lock);
            pthread_cond_destroy(&dev->cond);
            ret = DEMUX_ERROR_CANNOT_CREATE_THREAD;
        }
    }

    if (ret == AM_SUCCESS) {
        dev->open_count = 1;
    }
final:
    pthread_mutex_unlock(&am_gAdpLock);
    return ret;
}

/** Close demultiplexing device
  * dev_no demultiplexing device number

  * - AM_SUCCESS Success
  * - other values error code (see DemuxDriver.h)
  */
int DemuxClose(int dev_no) {
    DemuxDeviceType *dev;
    int ret = AM_SUCCESS;
    int i;
    AM_TRY(dmx_get_opened_dev(dev_no, &dev));
    pthread_mutex_lock(&am_gAdpLock);
    if (dev->open_count == 1) {
        dev->enable_thread = 0;
        if (dev->drv->wake) {
            dev->drv->wake(dev);
        }
        pthread_join(dev->thread, NULL);
        for (i = 0; i < DEMUX_FILTER_COUNT; i++) {
            dmx_free_filter(dev, &dev->filters[i]);
        }
        if (dev->drv->close) {
            dev->drv->close(dev);
        }
        pthread_mutex_destroy(&dev->lock);
        pthread_cond_destroy(&dev->cond);
    }
    dev->open_count--;
    pthread_mutex_unlock(&am_gAdpLock);
    return ret;
}

/** assigns a filter
  * dev_no demultiplexing device number
  * \param[out] fhandle returns the filter handle

  * - AM_SUCCESS Success
  * - other values error code (see DemuxDriver.h)
  */
int DemuxAllocateFilter(int dev_no, int *fhandle) {
    DemuxDeviceType *dev;
    int ret = AM_SUCCESS;
    int fid;
    assert(fhandle);
    AM_TRY(dmx_get_opened_dev(dev_no, &dev));
    pthread_mutex_lock(&am_gHwDmxLock);
    pthread_mutex_lock(&dev->lock);
    for (fid = 0; fid < DEMUX_FILTER_COUNT; fid++) {
        if (!dev->filters[fid].used) break;
    }
    if (fid >= DEMUX_FILTER_COUNT) {
        SUBTITLE_LOGI("no free section filter");
        ret = DEMUX_ERROR_NO_FREE_FILTER;
    }
    if (ret == AM_SUCCESS) {
        dmx_wait_cb(dev);
        dev->filters[fid].id   = fid;
        if (dev->drv->alloc_filter) {
            ret = dev->drv->alloc_filter(dev, &dev->filters[fid]);
        }
    }
    if (ret == AM_SUCCESS) {
        dev->filters[fid].used = true;
        *fhandle = fid;
    }
    pthread_mutex_unlock(&dev->lock);
    pthread_mutex_unlock(&am_gHwDmxLock);
    return ret;
}

/** Set Section filter
  * dev_no demultiplexing device number
  * fhandle filter handle
  * \param[in] params Section filter parameters

  * - AM_SUCCESS Success
  * - other values error code (see DemuxDriver.h)
  */
int DemuxSetSecFilter(int dev_no, int fhandle, const struct dmx_sct_filter_params *params) {
    DemuxDeviceType *dev;
    DemuxFilterType *filter;
    int ret = AM_SUCCESS;
    assert(params);
    AM_TRY(dmx_get_opened_dev(dev_no, &dev));
    if (!dev->drv->set_sec_filter) {
        SUBTITLE_LOGI("demux do not support set_sec_filter");
        return DEMUX_ERROR_NOT_SUPPORTED;
    }
    pthread_mutex_lock(&am_gHwDmxLock);
    pthread_mutex_lock(&dev->lock);
    ret = dmx_get_used_filter(dev, fhandle, &filter);
    if (ret == AM_SUCCESS) {
        dmx_wait_cb(dev);
        ret = dmx_stop_filter(dev, filter);
    }
    if (ret == AM_SUCCESS) {
        ret = dev->drv->set_sec_filter(dev, filter, params);
        SUBTITLE_LOGI("set sec filter %d PID: %d filter: %02x:%02x %02x:%02x %02x:%02x %02x:%02x %02x:%02x %02x:%02x %02x:%02x %02x:%02x",
                fhandle, params->pid,
                params->filter.filter[0], params->filter.mask[0],
                params->filter.filter[1], params->filter.mask[1],
                params->filter.filter[2], params->filter.mask[2],
                params->filter.filter[3], params->filter.mask[3],
                params->filter.filter[4], params->filter.mask[4],
                params->filter.filter[5], params->filter.mask[5],
                params->filter.filter[6], params->filter.mask[6],
                params->filter.filter[7], params->filter.mask[7]);
    }
    pthread_mutex_unlock(&dev->lock);
    pthread_mutex_unlock(&am_gHwDmxLock);
    return ret;
}

/** Set PES filter
  * dev_no demultiplexing device number
  * fhandle filter handle
  * \param[in] params PES filter parameters

  * - AM_SUCCESS Success
  * - other values error code (see DemuxDriver.h)
  */
int DemuxSetPesFilter(int dev_no, int fhandle, const struct dmx_pes_filter_params *params) {
    DemuxDeviceType *dev;
    DemuxFilterType *filter;
    int ret = AM_SUCCESS;
    assert(params);
    AM_TRY(dmx_get_opened_dev(dev_no, &dev));
    if (!dev->drv->set_pes_filter) {
        SUBTITLE_LOGI("demux do not support set_pes_filter");
        return DEMUX_ERROR_NOT_SUPPORTED;
    }
    pthread_mutex_lock(&am_gHwDmxLock);
    pthread_mutex_lock(&dev->lock);
    ret = dmx_get_used_filter(dev, fhandle, &filter);
    if (ret == AM_SUCCESS) {
        dmx_wait_cb(dev);
        ret = dmx_stop_filter(dev, filter);
    }
    if (ret == AM_SUCCESS) {
        ret = dev->drv->set_pes_filter(dev, filter, params);
        SUBTITLE_LOGI("set pes filter %d PID %d flags %d", fhandle, params->pid, params->flags);
    }
    pthread_mutex_unlock(&dev->lock);
    pthread_mutex_unlock(&am_gHwDmxLock);
    return ret;
}

/** releases a filter
  * dev_no demultiplexing device number
  * fhandle filter handle

  * - AM_SUCCESS Success
  * - other values error code (see DemuxDriver.h)
  */
int DemuxFreeFilter(int dev_no, int fhandle) {
    DemuxDeviceType *dev;
    DemuxFilterType *filter;
    int ret = AM_SUCCESS;
    AM_TRY(dmx_get_opened_dev(dev_no, &dev));
    pthread_mutex_lock(&am_gHwDmxLock);
    pthread_mutex_lock(&dev->lock);
    ret = dmx_get_used_filter(dev, fhandle, &filter);
    if (ret == AM_SUCCESS) {
        dmx_wait_cb(dev);
        ret = dmx_free_filter(dev, filter);
    }
    pthread_mutex_unlock(&dev->lock);
    pthread_mutex_unlock(&am_gHwDmxLock);
    return ret;
}

/** starts a filter
  * dev_no demultiplexing device number
  * fhandle filter handle

  * - AM_SUCCESS Success
  * - other values error code (see DemuxDriver.h)
  */
int DemuxStartFilter(int dev_no, int fhandle) {
    DemuxDeviceType *dev;
    DemuxFilterType *filter = NULL;
    int ret = AM_SUCCESS;
    AM_TRY(dmx_get_opened_dev(dev_no, &dev));
    pthread_mutex_lock(&am_gHwDmxLock);
    pthread_mutex_lock(&dev->lock);
    ret = dmx_get_used_filter(dev, fhandle, &filter);
    if (!filter->enable) {
        if (ret == AM_SUCCESS) {
            if (dev->drv->enable_filter) {
                ret = dev->drv->enable_filter(dev, filter, true);
            }
        }
        if (ret == AM_SUCCESS) {
            filter->enable = true;
        }
    }
    pthread_mutex_unlock(&dev->lock);
    pthread_mutex_unlock(&am_gHwDmxLock);
    return ret;
}

/** Stop a filter
  * dev_no demultiplexing device number
  * fhandle filter handle

  * - AM_SUCCESS Success
  * - other values error code (see DemuxDriver.h)
  */
int DemuxStopFilter(int dev_no, int fhandle) {
    DemuxDeviceType *dev;
    DemuxFilterType *filter = NULL;
    int ret = AM_SUCCESS;
    AM_TRY(dmx_get_opened_dev(dev_no, &dev));
    pthread_mutex_lock(&am_gHwDmxLock);
    pthread_mutex_lock(&dev->lock);
    ret = dmx_get_used_filter(dev, fhandle, &filter);
    if (ret == AM_SUCCESS) {
        if (filter->enable) {
            dmx_wait_cb(dev);
            ret = dmx_stop_filter(dev, filter);
        }
    }
    pthread_mutex_unlock(&dev->lock);
    pthread_mutex_unlock(&am_gHwDmxLock);
    return ret;
}

/** Set the buffer size of a filter
  * dev_no demultiplexing device number
  * fhandle filter handle
  * size buffer size

  * - AM_SUCCESS Success
  * - other values error code (see DemuxDriver.h)
  */
int DemuxSetBufferSize(int dev_no, int fhandle, int size) {
    DemuxDeviceType *dev;
    DemuxFilterType *filter;
    int ret = AM_SUCCESS;
    AM_TRY(dmx_get_opened_dev(dev_no, &dev));
    pthread_mutex_lock(&am_gHwDmxLock);
    pthread_mutex_lock(&dev->lock);
    if (!dev->drv->set_buf_size) {
        SUBTITLE_LOGI("do not support set_buf_size");
        ret = DEMUX_ERROR_NOT_SUPPORTED;
    }
    if (ret == AM_SUCCESS) ret = dmx_get_used_filter(dev, fhandle, &filter);
    if (ret == AM_SUCCESS) ret = dev->drv->set_buf_size(dev, filter, size);
    pthread_mutex_unlock(&dev->lock);
    pthread_mutex_unlock(&am_gHwDmxLock);
    return ret;
}

/** Get the callback function and user parameters corresponding to a filter
  * dev_no demultiplexing device number
  * fhandle filter handle
  * \param[out] cb returns the callback function corresponding to the filter
  * \param[out] data returns user parameters

  * - AM_SUCCESS Success
  * - other values error code (see DemuxDriver.h)
  */
int DemuxGetCallback(int dev_no, int fhandle, DemuxDataCb *cb, void **data) {
    DemuxDeviceType *dev;
    DemuxFilterType *filter;
    int ret = AM_SUCCESS;
    AM_TRY(dmx_get_opened_dev(dev_no, &dev));
    pthread_mutex_lock(&dev->lock);
    ret = dmx_get_used_filter(dev, fhandle, &filter);
    if (ret == AM_SUCCESS) {
        if (cb) *cb = filter->cb;
        if (data) *data = filter->user_data;
    }
    pthread_mutex_unlock(&dev->lock);
    return ret;
}

/** Set the callback function and user parameters corresponding to a filter
  * dev_no demultiplexing device number
  * fhandle filter handle
  * \param[in] cb callback function
  * \param[in] data user parameters of the callback function

  * - AM_SUCCESS Success
  * - other values error code (see DemuxDriver.h)
  */
int DemuxSetCallback(int dev_no, int fhandle, DemuxDataCb cb, void *data) {
    DemuxDeviceType *dev;
    DemuxFilterType *filter;
    int ret = AM_SUCCESS;
    AM_TRY(dmx_get_opened_dev(dev_no, &dev));
    pthread_mutex_lock(&dev->lock);
    ret = dmx_get_used_filter(dev, fhandle, &filter);
    if (ret == AM_SUCCESS) {
        dmx_wait_cb(dev);

        filter->cb = cb;
        filter->user_data = data;
    }
    pthread_mutex_unlock(&dev->lock);
    return ret;
}

/** Set the input source of the demultiplexing device
  * dev_no demultiplexing device number
  * src input source
  * - AM_SUCCESS Success
  * - other values error code (see DemuxDriver.h)
  */
int DemuxSetSource(int dev_no, AmlogicDemuxSourceType src) {
    DemuxDeviceType *dev;
    int ret = AM_SUCCESS;

    AM_TRY(dmx_get_opened_dev(dev_no, &dev));

    pthread_mutex_lock(&dev->lock);
    if (!dev->drv->set_source) {
        SUBTITLE_LOGI("do not support set_source");
        ret = DEMUX_ERROR_NOT_SUPPORTED;
    }

    if (ret == AM_SUCCESS) {
        ret = dev->drv->set_source(dev, src);
    }

    pthread_mutex_unlock(&dev->lock);

    if (ret == AM_SUCCESS) {
        pthread_mutex_lock(&am_gAdpLock);
        dev->src = src;
        pthread_mutex_unlock(&am_gAdpLock);
    }

    return ret;
}

/** DMX synchronization, which can be used to wait for the callback function to complete execution
  * dev_no demultiplexing device number
  * - AM_SUCCESS Success
  * - other values error code (see DemuxDriver.h)
  */
int DemuxSync(int dev_no) {
    DemuxDeviceType *dev;
    int ret = AM_SUCCESS;

    AM_TRY(dmx_get_opened_dev(dev_no, &dev));

    pthread_mutex_lock(&dev->lock);
    if (dev->thread != pthread_self()) {
        while (dev->flags & DMX_FL_RUN_CB)
            pthread_cond_wait(&dev->cond, &dev->lock);
    }
    pthread_mutex_unlock(&dev->lock);

    return ret;
}

int DemuxGetScrambleStatus(int dev_no, bool dev_status[2]) {
    char buf[32];
    char class_file[64];
    int vflag, aflag;
    int i;
    dev_status[0] = dev_status[1] = 0;
    snprintf(class_file,sizeof(class_file), "/sys/class/dmx/demux%d_scramble", dev_no);
    for (i = 0; i < 5; i++) {
        if (AmlogicFileRead(class_file, buf, sizeof(buf)) == AM_SUCCESS) {
            sscanf(buf,"%d %d", &vflag, &aflag);
            if (!dev_status[0]) dev_status[0] = vflag ? true : 0;
            if (!dev_status[1]) dev_status[1] = aflag ? true : 0;
            //SUBTITLE_LOGI("DemuxGetScrambleStatus video scramble %d, audio scramble %d\n", vflag, aflag);
            if (dev_status[0] && dev_status[1]) {
                return AM_SUCCESS;
            }
            usleep(10*1000);
        } else {
            SUBTITLE_LOGI("DemuxGetScrambleStatus read scramble status failed\n");
            return AM_FAILURE;
        }
    }
    return AM_SUCCESS;
}
