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

#include <jni.h>
#include <utils/RefBase.h>
#include <utils/Mutex.h>

#include <utils/Atomic.h>
#include <utils/Log.h>
//#include <utils/RefBase.h>
#include <utils/String8.h>
//#include <utils/String16.h>
#include <unistd.h>
#include <cutils/properties.h>
#include <dlfcn.h>


#include <android/native_window.h>
//#include <android/native_window_jni.h>

using namespace android;

#define FIND_CLASS(var, className) \
        var = env->FindClass(className); \
        LOG_FATAL_IF(! var, "Unable to find class " className);

#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

void startNativeRender(JNIEnv* env, jobject object, jobject surface) {
    //ANativeWindow *nwin = ANativeWindow_fromSurface(env, surface);

    sp<ANativeWindow> win = android_view_Surface_getNativeWindow(env, surface);
    if (win != NULL) {
    win->incStrong((void*)ANativeWindow_fromSurface);
    }
    return win.get();

}

static JNINativeMethod SubtitleRender_Methods[] = {
    {"nStartNativeRender", "(Landroid/view/Surface;)V", (void *)startNativeRender},

};

int register_com_droidlogic_app_SubtitleViewAdaptor(JNIEnv *env) {
    static const char *const kClassPathName = "com/droidlogic/app/SubtitleViewAdaptor";
    jclass clazz;
    int rc;
    FIND_CLASS(clazz, kClassPathName);

    if (clazz == NULL) {
        ALOGE("Native registration unable to find class '%s'\n", kClassPathName);
        return -1;
    }

    rc = (env->RegisterNatives(clazz, SubtitleRender_Methods, NELEM(SubtitleRender_Methods)));
    if (rc < 0) {
        env->DeleteLocalRef(clazz);
        ALOGE("RegisterNatives failed for '%s' %d\n", kClassPathName, rc);
        return -1;
    }

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

    if (register_com_droidlogic_app_SubtitleViewAdaptor(env) < 0) {
        ALOGE("Can't register SubtitleViewAdaptor JNI");
        goto bail;
    }

    /* success -- return valid version number */
    result = JNI_VERSION_1_4;

bail:
    return result;
}
