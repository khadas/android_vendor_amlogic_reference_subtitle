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

#define LOG_TAG "SubtitleManager-jni"
#include <jni.h>
#include <utils/RefBase.h>
#include <utils/Mutex.h>

#include <utils/Atomic.h>
#include <utils/Log.h>
//#include <utils/RefBase.h>
#include <utils/String8.h>
#include <fcntl.h>
#include <unistd.h>
#include <cutils/properties.h>
#include <dlfcn.h>
#include <algorithm>

#include "SubtitleServerClient.h"

// use the type defines here. not directly use the api.
#include "SubtitleNativeAPI.h"

using android::Mutex;
using amlogic::SubtitleServerClient;
using amlogic::SubtitleListener;

//Must always keep the same as Java final const!
static const int SUBTITLE_TXT =1;
static const int SUBTITLE_IMAGE =2;
static const int SUBTITLE_CC_JASON = 3;
static const int SUBTITLE_IMAGE_CENTER = 4;


class SubtitleDataListenerImpl;


#define TRACE_CALL 1
#define FIND_CLASS(var, className) \
        var = env->FindClass(className); \
        LOG_FATAL_IF(! var, "Unable to find class " className);

#define GET_METHOD_ID(var, clazz, methodName, methodDescriptor) \
        var = env->GetMethodID(clazz, methodName, methodDescriptor); \
        LOG_FATAL_IF(! var, "Unable to find method " methodName);


struct JniContext {
    Mutex mLock;
    JavaVM *mJavaVM = NULL;

    jobject mSubtitleManagerObject;
    int mFallBack = true;

    jmethodID mNotifySubtitleEvent;
    jmethodID mNotifySubtitleUIEvent;
    jmethodID mSubtitleTextOrImage;
    jmethodID mUpdateChannelId;
    jmethodID mNotifySubtitleAvail;
    jmethodID mNotifySubtitleInfo;
    jmethodID mExtSubtitle;

    sp<SubtitleServerClient> mSubContext;

    JNIEnv* getJniEnv(bool *needDetach) {
        int ret = -1;
        JNIEnv *env = NULL;
        ret = mJavaVM->GetEnv((void **) &env, JNI_VERSION_1_4);
        if (ret < 0) {
            ret = mJavaVM->AttachCurrentThread(&env, NULL);
            if (ret < 0) {
                ALOGE("Can't attach thread ret = %d", ret);
                return NULL;
            }
            *needDetach = true;
        }
        return env;
    }

    void DetachJniEnv() {
        int result = mJavaVM->DetachCurrentThread();
        if (result != JNI_OK) {
            ALOGE("thread detach failed: %#x", result);
        }
    }

    jboolean callJava_isExternalSubtitle() {
        bool needDetach = false;
        JNIEnv *env = getJniEnv(&needDetach);
        jboolean  isExtSub = env->CallBooleanMethod(mSubtitleManagerObject, mExtSubtitle);
        if (needDetach) DetachJniEnv();
        return isExtSub;
    }

    jint callJava_subtitleTextOrImage(int parserType) {
        bool needDetach = false;
        JNIEnv *env = getJniEnv(&needDetach);
        if (mSubtitleManagerObject != nullptr && env!= nullptr) {
            jint uiType = env->CallIntMethod(mSubtitleManagerObject, mSubtitleTextOrImage, parserType);
            if (needDetach) DetachJniEnv();
            return uiType;
        } else {
            // Handle the case where mSubtitleManagerObject is null
            ALOGE("mSubtitleManagerObject is Null or env is Null");
            if (needDetach) DetachJniEnv();
            return -1;/* some default value or error code */;
        }
    }
    void callJava_showTextData(const char *data, int type, int cmd, int objectSegmentId) {
        bool needDetach = false;
        //jint i = 0;
        JNIEnv *env = getJniEnv(&needDetach);
        //jstring string = env->NewStringUTF(data);
        jbyteArray byteArray = env->NewByteArray(strlen(data));
        env->SetByteArrayRegion(byteArray, 0, strlen(data),(jbyte *)data);
        // Text data do not care positions, no such info!
        env->CallVoidMethod(mSubtitleManagerObject, mNotifySubtitleEvent, nullptr, byteArray,
                type, /*x,y*/0, 0, /*w,h*/0, 0, /*vw,vh*/0, 0, !(cmd==0), objectSegmentId);
        env->DeleteLocalRef(byteArray);
        if (needDetach) DetachJniEnv();
   }

    void callJava_showBitmapData(const char *data, int size, int uiType,
            int x, int y, int width, int height,
            int videoWidth, int videoHeight, int cmd, int objectSegmentId) {
        bool needDetach = false;

        if (width <= 0 || height <= 0 || size <= 0) {
            ALOGE("invalid parameter width=%d height=%d size=%d", width, height, size);
            return;
        }
        ALOGI("callJava_showBitmapData width=%d height=%d size=%d", width, height, size);
        JNIEnv *env = getJniEnv(&needDetach);
        if (width * height * 4 == size) {
            jintArray array = env->NewIntArray(width*height);
            env->SetIntArrayRegion(array, 0, width*height, (jint *)data);
            env->CallVoidMethod(mSubtitleManagerObject, mNotifySubtitleEvent, array, nullptr,
                uiType, x, y, width, height, videoWidth, videoHeight, !(cmd==0), objectSegmentId);
            env->DeleteLocalRef(array);
        } else {
            ALOGE("invalid RGBA bitmap, width=%d height=%d size=%d", width, height, size);
        }
        if (needDetach) DetachJniEnv();
    }

    void callJava_notifyDataEvent(int event, int id) {
        bool needDetach = false;
        JNIEnv *env = getJniEnv(&needDetach);
        ALOGD("callJava_notifyDataEvent: %d %d", event, id);
        env->CallVoidMethod(mSubtitleManagerObject, mUpdateChannelId, event, id);
        if (needDetach) DetachJniEnv();
    }

    void callJava_notifySubtitleAvail(int avail) {
        bool needDetach = false;
        JNIEnv *env = getJniEnv(&needDetach);
        ALOGD("callJava_notifySubtitleAvail: %d", avail);
        env->CallVoidMethod(mSubtitleManagerObject, mNotifySubtitleAvail, avail);
        if (needDetach) DetachJniEnv();
    }

    void callJava_notifySubtitleInfo(int what, int extra) {
        bool needDetach = false;
        JNIEnv *env = getJniEnv(&needDetach);
        ALOGD("callJava_notifySubtitleInfo: what:%d, extra:%d", what, extra);
        env->CallVoidMethod(mSubtitleManagerObject, mNotifySubtitleInfo, what, extra);
        if (needDetach) DetachJniEnv();
    }

    void callJava_uiCommand(int command, const std::vector<int> &param) {
        bool needDetach = false;
        JNIEnv *env = getJniEnv(&needDetach);
        ALOGD("callJava_uiCommand:%d", command);

        jintArray array = env->NewIntArray(param.size());
        int *jintData = (int *)malloc(param.size() * sizeof(int));

        for (int i=0; i<param.size(); i++) {
            ALOGD("param: %d %d", i, param[i]);
            jintData[i] = param[i];
        }
        env->SetIntArrayRegion(array, 0, param.size(), (jint *)jintData);
        env->CallVoidMethod(mSubtitleManagerObject, mNotifySubtitleUIEvent, command, array);
        env->DeleteLocalRef(array);
        free(jintData);
        if (needDetach) DetachJniEnv();
    }
};

JniContext *gJniContext = nullptr;
static inline JniContext *getJniContext() {
    // TODO: check error
    return gJniContext;
}



class SubtitleDataListenerImpl : public SubtitleListener {
public:
    SubtitleDataListenerImpl() {}
    ~SubtitleDataListenerImpl() {}
    // TODO: maybe, we can split to notify Text and notify Bitmap
    virtual void onSubtitleEvent(const char *data, int size, int parserType,
            int x, int y, int width, int height,
            int videoWidth, int videoHeight, int cmd, int objectSegmentId)
{
        jint uiType = getJniContext()->callJava_subtitleTextOrImage(parserType);

        ALOGD("in onStringSubtitleEvent subtitleType=%d size=%d x=%d, y= %d width=%d height=%d videow=%d, videoh=%d cmd=%d objectSegmentId=%d\n",
                uiType, size, x, y, width, height, videoWidth, videoHeight, cmd, objectSegmentId);

        if (uiType == 1 && width > 0 && height > 0) uiType = 2;

        if (((uiType == 2) || (uiType == 4)) && size > 0) {
            getJniContext()->callJava_showBitmapData(data, size, uiType, x, y, width, height, videoWidth, videoHeight, cmd, objectSegmentId);
        } else {
            getJniContext()->callJava_showTextData(data, uiType, cmd, objectSegmentId);
        }

    }

    // TODO: maybe the name needs to be reconsidered
    virtual void onSubtitleDataEvent(int event, int id) {
        getJniContext()->callJava_notifyDataEvent(event, id);
    }

    void onSubtitleAvail(int avail) {
        getJniContext()->callJava_notifySubtitleAvail(avail);
    }

    void onSubtitleAfdEvent(int afd, int playerid) {
        //TODO:need mw register and cb
    }

    void onSubtitleDimension(int width, int height){}

    void onMixVideoEvent(int val) {
        //TODO:need mw register and cb
    }

    void onSubtitleLanguage(std::string lang) {
        //TODO:need mw register and cb
    }

    void onSubtitleInfo(int what, int extra) {
        ALOGD("onSubtitleInfo:what:%d, extra:%d", what, extra);
        getJniContext()->callJava_notifySubtitleInfo(what, extra);
    }

    // sometime, server may crash, we need clean up in server side.
    virtual void onServerDied() {
        std::vector<int> params;
        getJniContext()->callJava_uiCommand((int)FallthroughUiCmd::CMD_UI_HIDE, params);
    }


    void onSubtitleUIEvent(int uiCmd, const std::vector<int> &params) {
        getJniContext()->callJava_uiCommand(uiCmd, params);
    }
};

static void nativeInit(JNIEnv* env, jobject object, jboolean startFallbackDisplay) {
    if (TRACE_CALL) ALOGD("%s %d", __func__, __LINE__);
    getJniContext()->mSubtitleManagerObject = env->NewGlobalRef(object);
    getJniContext()->mFallBack = startFallbackDisplay;
    if (getJniContext()->mSubContext == nullptr) {
        getJniContext()->mSubContext = new SubtitleServerClient(
            getJniContext()->mFallBack,
            new SubtitleDataListenerImpl(),
            OpenType::TYPE_APPSDK);
    }
}

static void nativeDestroy(JNIEnv* env, jobject object) {
    if (TRACE_CALL) ALOGD("%s %d", __func__, __LINE__);
    getJniContext()->mSubContext = nullptr;
    env->DeleteGlobalRef(getJniContext()->mSubtitleManagerObject);
    getJniContext()->mSubtitleManagerObject = nullptr;

}

static void nativeUpdateVideoPos(JNIEnv* env, jobject object, jint pos) {
    //ALOGD("subtitleShowSub pos:%d\n", pos);
    if (pos > 0 && getJniContext()->mSubContext != nullptr) {
        getJniContext()->mSubContext->updateVideoPos(pos);
    } else {
        ALOGE("why pos is negative? %d", pos);
    }
}


static jboolean nativeOpenSubIdx(JNIEnv* env, jobject object, jstring jpath, jint trackId, jint ioType) {
    if (TRACE_CALL) ALOGD("%s %d", __func__, __LINE__);

    bool res = false;
    int fd = -1;

    if (getJniContext()->mSubContext != nullptr) {
        getJniContext()->mSubContext->userDataOpen();
        bool isExt = getJniContext()->callJava_isExternalSubtitle();
        ALOGD("isExt? %d", isExt);

        if (isExt) {
            const char *cpath = env->GetStringUTFChars(jpath, nullptr);
            std::string path = cpath;
            fd = ::open(path.c_str(), O_RDONLY);
            size_t pos = path.rfind(".");
            std::string ext = path.substr(pos, path.size()-pos);
            ALOGD("file extension is:%s", ext.c_str());
            std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
            ALOGD("file extension is:%s", ext.c_str());
            if (ext.compare(".IDX") == 0) {
                int idxDataFd = -1;
                std::string subFile = path.substr(0, pos);
                ALOGD(">> %s", (subFile+".SUB").c_str());
                if (access((subFile+".SUB").c_str(), F_OK) == 0) {
                    idxDataFd = ::open((subFile+".SUB").c_str(), O_RDONLY);
                    ALOGD("access %s", (subFile+".SUB").c_str());
                } else if(access((subFile+".sub").c_str(), F_OK) == 0) {
                    idxDataFd = ::open((subFile+".sub").c_str(), O_RDONLY);
                    ALOGD("access %s", (subFile+".sub").c_str());
                }

                if (idxDataFd != -1) {
                    res = getJniContext()->mSubContext->open(fd, idxDataFd, trackId, ioType);
                    ::close(idxDataFd);
                } else {
                    res = getJniContext()->mSubContext->open(fd, ioType);
                }
            } else {
                res = getJniContext()->mSubContext->open(fd, ioType);
            }
            env->ReleaseStringUTFChars(jpath, cpath);
        } else {
            res = getJniContext()->mSubContext->open(-1, ioType);
        }
    } else {
       ALOGE("Subtitle Connection not established");
    }
    if (fd >= 0) ::close(fd);
    return true;
}

static jboolean nativeOpen(JNIEnv* env, jobject object, jstring jpath, jint ioType) {
    // negative index is for normal open.
    // idx-sub subtitle may has many track, we support select it
    return nativeOpenSubIdx(env, object, jpath, -1, ioType);
}

static void nativeClose(JNIEnv* env, jobject object) {
    if (TRACE_CALL) ALOGD("%s %d %p", __func__, __LINE__, getJniContext()->mSubContext.get());
    if (getJniContext()->mSubContext != nullptr) {
        getJniContext()->mSubContext->userDataClose();
        getJniContext()->mSubContext->close();
    } else {
       ALOGE("Subtitle Connection not established");
    }
}


static jint nativeTotalSubtitles(JNIEnv* env, jobject object) {
    if (TRACE_CALL) ALOGD("%s %d %p", __func__, __LINE__, getJniContext()->mSubContext.get());
    if (getJniContext()->mSubContext != nullptr) {
        return getJniContext()->mSubContext->totalTracks();
    } else {
        ALOGE("Subtitle Connection not established");
    }

    return -1;
}

static jint nativeInnerSubtitles(JNIEnv* env, jobject object) {
    return nativeTotalSubtitles(env, object);
}

static void nativeSetSubType(JNIEnv* env, jclass clazz, jint type) {
    if (getJniContext()->mSubContext != nullptr) {
        getJniContext()->mSubContext->setSubType(type);
    } else {
        ALOGE("Subtitle Connection not established");
    }
}

static jstring nativeGetSubLanguage(JNIEnv* env, jclass clazz, jint idx) {
    if (getJniContext()->mSubContext != nullptr) {
        std::string val;
        val = getJniContext()->mSubContext->getSubLanguage(idx);
        return env->NewStringUTF(val.c_str());
    } else {
        ALOGE("Subtitle Connection not established");
    }
    return env->NewStringUTF("");
}

static void nativeSetSubLanguage(JNIEnv* env, jclass clazz, jstring jlang) {
    if (getJniContext()->mSubContext != nullptr) {
        const char *lang = env->GetStringUTFChars(jlang, nullptr);
        getJniContext()->mSubContext->setSubLanguage(lang);
        env->ReleaseStringUTFChars(jlang, lang);
    } else {
        ALOGE("Subtitle Connection not established");
    }
}

static void nativeSetStartTimeStamp(JNIEnv* env, jclass clazz, jint startTime) {
    if (getJniContext()->mSubContext != nullptr) {
        getJniContext()->mSubContext->setStartTimeStamp(startTime);
    } else {
        ALOGE("Subtitle Connection not established");
    }
}

static void nativeSetSctePid(JNIEnv* env, jclass clazz, jint pid) {
    if (getJniContext()->mSubContext != nullptr) {
        getJniContext()->mSubContext->setSubPid(pid);
    } else {
        ALOGE("Subtitle Connection not established");
    }
}

static void nativeSetSubPid(JNIEnv* env, jclass clazz, jint pid, jint onid, jint tsid) {
    if (getJniContext()->mSubContext != nullptr) {
        getJniContext()->mSubContext->setSubPid(pid, onid, tsid);
    } else {
        ALOGE("Subtitle Connection not established");
    }
}

static void nativeSetPlayerType(JNIEnv* env, jclass clazz, jint type) {
    if (type == 1) {
        ALOGE("SubtitleServerHidlClient nativeSetPlayerType amu ...");
        property_set("media.ctcmediaplayer.enable", "0");
        property_set("media.ctcmediaplayer.ts-stream", "0");
     } else {
        ALOGE("SubtitleServerHidlClient nativeSetPlayerType  ctc ...");
        property_set("media.ctcmediaplayer.enable", "1");
        property_set("media.ctcmediaplayer.ts-stream", "1");
     }
}

static jint nativeGetSubType(JNIEnv* env, jclass clazz) {
    if (getJniContext()->mSubContext != nullptr) {
        return getJniContext()->mSubContext->getSubType();
    } else {
        ALOGE("Subtitle Connection not established");
    }
    return -1;
}

static void nativeResetForSeek(JNIEnv* env, jclass clazz) {
    if (TRACE_CALL) ALOGD("%s %d", __func__, __LINE__);
    if (getJniContext()->mSubContext != nullptr) {
        getJniContext()->mSubContext->resetForSeek();
    } else {
        ALOGE("Subtitle Connection not established");
    }

}

static void nativeSelectCcChannel(JNIEnv* env, jclass clazz, jint channel) {
    if (TRACE_CALL) ALOGD("%s idx:%d", __func__, __LINE__);
    if ((getJniContext() != nullptr) &&(getJniContext()->mSubContext != nullptr)) {
        getJniContext()->mSubContext->selectCcChannel(channel);
    } else {
        ALOGE("Subtitle Connection not established");
    }
}

static jint nativeTtControl(JNIEnv* env, jclass clazz, jint teletextEvent, jint magazine, jint pageNo, jint regionId) {
    if (getJniContext()->mSubContext != nullptr) {
        getJniContext()->mSubContext->ttControl(teletextEvent, magazine, pageNo, regionId, -1);
    } else {
        ALOGE("Subtitle Connection not established");
    }
    return 0;
}

static jint nativeTtGoHome(JNIEnv* env, jclass clazz) {
    if (getJniContext()->mSubContext != nullptr) {
        getJniContext()->mSubContext->ttControl(TT_EVENT_INDEXPAGE, -1, -1, -1, -1);
    } else {
        ALOGE("Subtitle Connection not established");
    }
    return 0;
}


static jint nativeTtGotoPage(JNIEnv* env, jclass clazz, jint magazine, jint pageNo) {
    if (getJniContext()->mSubContext != nullptr) {
        getJniContext()->mSubContext->ttControl(TT_EVENT_GO_TO_PAGE, magazine, pageNo, -1, -1);
    } else {
        ALOGE("Subtitle Connection not established");
    }
    return 0;
}

static jint nativeTtNextPage(JNIEnv* env, jclass clazz, jint dir) {
    if (getJniContext()->mSubContext != nullptr) {
        getJniContext()->mSubContext->ttControl(dir==1?TT_EVENT_NEXTPAGE:TT_EVENT_PREVIOUSPAGE, -1, -1, -1, -1);
    } else {
        ALOGE("Subtitle Connection not established");
    }
    return 0;
}

static jint nativeTtNextSubPage(JNIEnv* env, jclass clazz, jint dir) {
    if (getJniContext()->mSubContext != nullptr) {
        getJniContext()->mSubContext->ttControl(dir==1?TT_EVENT_NEXTSUBPAGE:TT_EVENT_PREVIOUSSUBPAGE, -1, -1, -1, -1);
    } else {
        ALOGE("Subtitle Connection not established");
    }
    return 0;
}

static void nativeSubtitleTune(JNIEnv* env, jclass clazz, jint type, jint param1, jint param2, jint param3)
{
    ALOGD("nativeSubtitleTune: subtitleTune(type:%d, params:%d,%d,%d)", type, param1, param2, param3);
    if (getJniContext()->mSubContext != nullptr)
    {
        getJniContext()->mSubContext->setPipId(type + 1, param1);
    }
}

static void nativeUnCrypt(JNIEnv *env, jclass clazz, jstring src, jstring dest) {
    const char *FONT_VENDOR_LIB = "/vendor/lib/libvendorfont.so";
    const char *FONT_PRODUCT_LIB = "/product/lib/libvendorfont.so";
    const char *FONT_VENDOR_LIB_64 = "/vendor/lib64/libvendorfont.so";
    const char *FONT_PRODUCT_LIB_64 = "/product/lib64/libvendorfont.so";
    void *handle = NULL;
    char tempbuf[PROPERTY_VALUE_MAX];
    memset(tempbuf, 0, PROPERTY_VALUE_MAX);
    property_get("ro.product.cpu.abilist64",tempbuf, "");

    // TODO: maybe we need some smart method to get the lib.
    if (strlen(tempbuf) > 0) {//64bit
        ALOGD("nativeUnCrypt,is 64bit system,tempbuf:%s", tempbuf);
        handle = dlopen(FONT_PRODUCT_LIB_64, RTLD_NOW);
        if (handle == nullptr) {
            handle = dlopen(FONT_VENDOR_LIB_64, RTLD_NOW);
        }

        if (handle == nullptr) {
            ALOGE(" nativeUnCrypt error! cannot open uncrypto lib");
            return;
        }
    } else {//32bit

        ALOGD("nativeUnCrypt,is 64bit system");
        handle = dlopen(FONT_PRODUCT_LIB, RTLD_NOW);
        if (handle == nullptr) {
            handle = dlopen(FONT_VENDOR_LIB, RTLD_NOW);
        }

        if (handle == nullptr) {
            ALOGE(" nativeUnCrypt error! cannot open uncrypto lib");
            return;
        }
    }

    typedef void (*fnFontRelease)(const char*, const char*);
    fnFontRelease fn = (fnFontRelease)dlsym(handle, "vendor_font_release");
    if (fn == nullptr) {
        ALOGE(" nativeUnCrypt error! cannot locate symbol vendor_font_release in uncrypto lib");
        dlclose(handle);
        return;
    }

    const char *srcstr = (const char *)env->GetStringUTFChars(src, NULL);
    const char *deststr = (const char *)env->GetStringUTFChars( dest, NULL);

    fn(srcstr, deststr);
    dlclose(handle);

    (env)->ReleaseStringUTFChars(src, (const char *)srcstr);
    (env)->ReleaseStringUTFChars(dest, (const char *)deststr);
}

static JNINativeMethod SubtitleManager_Methods[] = {
    {"nativeInit", "(Z)V", (void *)nativeInit},
    {"nativeDestroy", "()V", (void *)nativeDestroy},
    {"nativeUpdateVideoPos", "(I)V", (void *)nativeUpdateVideoPos},
    {"nativeOpen", "(Ljava/lang/String;I)Z", (void *)nativeOpen},
    {"nativeOpenSubIdx", "(Ljava/lang/String;II)Z", (void *)nativeOpenSubIdx},
    {"nativeClose", "()V", (void *)nativeClose},
    {"nativeTotalSubtitles", "()I", (void *)nativeTotalSubtitles},
    {"nativeInnerSubtitles", "()I", (void *)nativeInnerSubtitles},
    {"nativeGetSubType", "()I", (void *)nativeGetSubType},
    {"nativeSetSubType", "(I)V", (void *)nativeSetSubType},
    {"nativeGetSubLanguage", "(I)Ljava/lang/String;", (void *)nativeGetSubLanguage},
    {"nativeSetSubLanguage", "(Ljava/lang/String;)V", (void *)nativeSetSubLanguage},
    {"nativeSetStartTimeStamp", "(I)V", (void *)nativeSetStartTimeStamp},
    {"nativeSetPlayerType", "(I)V", (void *)nativeSetPlayerType},
    {"nativeSetSctePid", "(I)V", (void *)nativeSetSctePid},
    {"nativeSetSubPid", "(III)V", (void *)nativeSetSubPid},
    {"nativeResetForSeek", "()V", (void *)nativeResetForSeek},
    {"nativeTtControl", "(IIII)I", (void *)nativeTtControl},
    {"nativeTtGoHome", "()I", (void *)nativeTtGoHome},
    {"nativeTtGotoPage", "(II)I", (void *)nativeTtGotoPage},
    {"nativeTtNextPage", "(I)I", (void *)nativeTtNextPage},
    {"nativeTtNextSubPage", "(I)I", (void *)nativeTtNextSubPage},
    {"nativeSubtitleTune", "(IIII)V", (void *)nativeSubtitleTune},
    {"nativeSelectCcChannel", "(I)V", (void *)nativeSelectCcChannel},
    {"nativeUnCrypt", "(Ljava/lang/String;Ljava/lang/String;)V", (void *)nativeUnCrypt},

};

int register_com_droidlogic_app_SubtitleManager(JNIEnv *env) {
    static const char *const kClassPathName = "com/droidlogic/app/SubtitleManager";
    jclass clazz;
    int rc;
    FIND_CLASS(clazz, kClassPathName);

    if (clazz == NULL) {
        ALOGE("Native registration unable to find class '%s'\n", kClassPathName);
        return -1;
    }

    rc = (env->RegisterNatives(clazz, SubtitleManager_Methods, NELEM(SubtitleManager_Methods)));
    if (rc < 0) {
        env->DeleteLocalRef(clazz);
        ALOGE("RegisterNatives failed for '%s' %d\n", kClassPathName, rc);
        return -1;
    }

    GET_METHOD_ID(getJniContext()->mNotifySubtitleEvent, clazz, "notifySubtitleEvent", "([I[BIIIIIIIZI)V");
    GET_METHOD_ID(getJniContext()->mSubtitleTextOrImage, clazz, "subtitleTextOrImage", "(I)I");
    GET_METHOD_ID(getJniContext()->mNotifySubtitleUIEvent, clazz, "notifySubtitleUIEvent", "(I[I)V");
    GET_METHOD_ID(getJniContext()->mUpdateChannelId, clazz, "updateChannelId", "(II)V");
    GET_METHOD_ID(getJniContext()->mNotifySubtitleAvail, clazz, "notifyAvailable", "(I)V");
    GET_METHOD_ID(getJniContext()->mExtSubtitle, clazz, "isExtSubtitle", "()Z");
    GET_METHOD_ID(getJniContext()->mNotifySubtitleInfo, clazz, "notifySubtitleInfo", "(II)V");
    return rc;
}


jint JNI_OnLoad(JavaVM *vm, void *reserved __unused) {
    JNIEnv *env = NULL;
    jint result = -1;

    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGI("ERROR: GetEnv failed\n");
        goto bail;
    }
    assert(env != NULL);

    gJniContext = new JniContext();
    getJniContext()->mJavaVM = vm;

    if (register_com_droidlogic_app_SubtitleManager(env) < 0) {
        ALOGE("Can't register SubtitleManager JNI");
        goto bail;
    }

    /* success -- return valid version number */
    result = JNI_VERSION_1_4;

bail:
    return result;
}

