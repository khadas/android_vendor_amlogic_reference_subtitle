#pragma once

#include <type_traits>  // for common_type.
#include <utils/RefBase.h>
#include <utils/Mutex.h>
#include <utils/Atomic.h>
#include <utils/String8.h>

#include "SubtitleLog.h"

#include <hidlmemory/mapping.h>
#include <android/hidl/memory/1.0/IMemory.h>

#include <vendor/amlogic/hardware/subtitleserver/1.0/ISubtitleServer.h>
#include <vendor/amlogic/hardware/subtitleserver/1.0/ISubtitleCallback.h>


using android::hardware::hidl_vec;
using android::sp;
using android::wp;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_handle;
using ::android::hardware::hidl_memory;

using ::android::hardware::mapMemory;
using ::android::hidl::memory::V1_0::IMemory;

using ::vendor::amlogic::hardware::subtitleserver::V1_0::ISubtitleCallback;
using ::vendor::amlogic::hardware::subtitleserver::V1_0::ISubtitleServer;
using ::vendor::amlogic::hardware::subtitleserver::V1_0::Result;
using ::vendor::amlogic::hardware::subtitleserver::V1_0::ConnectType;
using ::vendor::amlogic::hardware::subtitleserver::V1_0::OpenType;
using ::vendor::amlogic::hardware::subtitleserver::V1_0::FallthroughUiCmd;
using ::vendor::amlogic::hardware::subtitleserver::V1_0::SubtitleHidlParcel;


// must keep sync with SubtitleServer.
typedef enum {
    DTV_SUB_INVALID         = -1,
    DTV_SUB_CC              = 2,
    DTV_SUB_SCTE27          = 3,
    DTV_SUB_DVB             = 4,
    DTV_SUB_DVB_TELETEXT    = 5,
    DTV_SUB_DVB_TTML        = 6,
    DTV_SUB_ARIB24          = 7,
    DTV_SUB_SMPTE_TTML      = 8,
} DtvSubtitleType;

typedef struct {
    const char *extSubPath;
    int ioSource;
    int subtitleType = 0;
    int mode = 0;
    int pid = 0;
    int subPid = 0;
    int onid = 0;
    int tsid = 0;
    int videoFormat = 0;             //cc
    int channelId = 0;               //cc
    int ancillaryPageId = 0;         //dvb
    int compositionPageId = 0;       //dvb
    int dmxId;
    int fd = 0;
    int flag = 0;
    const char *lang;
} AmlSubtitleParam2;



namespace amlogic {

class SubtitleListener : public android::RefBase {
public:
    virtual ~SubtitleListener() {}

    virtual void onSubtitleEvent(const char *data, int size, int parserType,
                int x, int y, int width, int height,
                int videoWidth, int videoHeight, int cmd, int objectSegmentId) = 0;

    virtual void onSubtitleDataEvent(int event, int id) = 0;

    virtual void onSubtitleAvail(int avail) = 0;
    virtual void onSubtitleAfdEvent(int avail, int playerid) = 0;
    virtual void onSubtitleDimension(int width, int height) = 0;
    virtual void onMixVideoEvent(int val) = 0;
    virtual void onSubtitleLanguage(std::string lang) = 0;
    virtual void onSubtitleInfo(int what, int extra) = 0;


    // Middleware API do not need, this transfer UI command to fallback displayer
    virtual void onSubtitleUIEvent(int uiCmd, const std::vector<int> &params) = 0;



    // sometime, server may crash, we need clean up in server side.
    virtual void onServerDied() = 0;
};


class SubtitleServerClient : public android::RefBase {

public:
    SubtitleServerClient() = delete;
    SubtitleServerClient(bool isFallback, sp<SubtitleListener> listener, OpenType openType);
    ~SubtitleServerClient();

    bool open(const char*path, int ioType);
    bool open(int fd, int ioType);
    bool open(int fd, int fdData, int trackId, int ioType);
    bool close();

    /* for external subtitle update PTS */
    bool updateVideoPos(int pos);

    int totalTracks();

    bool setSubType(int type);
    int getSubType();
    std::string getSubLanguage(int idx);
    bool setSubLanguage(std::string lang);
    bool setStartTimeStamp(int startTime);
    bool resetForSeek();

    bool setPipId(int mode, int id);
    // most, for scte.
    bool setSubPid(int pid);
    bool setSubPid(int pid, int onid, int tsid);

    bool setSecureLevel(int flag);


    // for select CC index
    bool selectCcChannel(int idx);
    bool selectCcChannel(int idx, const char *lang);
    bool setClosedCaptionId(int id);
    bool setClosedCaptionVfmt(int fmt);
    bool setClosedCaptionLang(const char *lang);
    bool setCompositionPageId(int pageId);
    bool setAncillaryPageId(int ancPageId);


    bool ttControl(int cmd, int magazine, int page, int regionId, int param);


    bool userDataOpen();
    bool userDataClose();

    // ui related.
    // Below api only used for control standalone UI.
    // The UI is not recommended, only for some native app/middleware
    // that cannot Android (Java) UI hierarchy.
    bool uiShow();
    bool uiHide();
    bool uiSetTextColor(int color);
    bool uiSetTextSize(int size);
    bool uiSetGravity(int gravity);
    bool uiSetTextStyle(int style);
    bool uiSetYOffset(int yOffset);
    bool uiSetImageRatio(float ratioW, float ratioH, int32_t maxW, int32_t maxH);
    bool uiGetSubDimension(int *pWidth, int *pHeight);
    bool uiSetSurfaceViewRect(int x, int y, int width, int height);

private:
    struct SubtitleDeathRecipient : public android::hardware::hidl_death_recipient {
    public:
        SubtitleDeathRecipient(wp<SubtitleServerClient> sc) : mOwner(sc) {}
        virtual void serviceDied(uint64_t cookie,
                const ::android::wp<::android::hidl::base::V1_0::IBase>& who) override;
    private:
        wp<SubtitleServerClient> mOwner;
    };

    class SubtitleCallback : public ISubtitleCallback {
    public:
        SubtitleCallback(sp<SubtitleListener> sl) : mListener(sl) {}
        ~SubtitleCallback() {mListener = nullptr;}
        virtual Return<void> notifyDataCallback(const SubtitleHidlParcel &parcel) override;
        virtual Return<void> uiCommandCallback(const SubtitleHidlParcel &parcel)  override ;
        virtual Return<void> eventNotify(const SubtitleHidlParcel& parcel) override;

    private:
        sp<SubtitleListener> mListener;
    };


    template<typename T>
    void checkRemoteResultLocked(Return<T> &r);

    void initRemoteLocked();

    sp<ISubtitleServer> mRemote;
    sp<SubtitleListener> mListener;

    sp<SubtitleCallback> mCallback;
    sp<SubtitleDeathRecipient> mDeathRecipient;

    int mSessionId;
    mutable android::Mutex mLock;

    // standalone fallback impl
    static inline bool mIsFallback;

    // As hidl. check if from middleware or APP.
    OpenType mOpenType;
    bool hasInit;
    bool hasDied;
    AmlSubtitleParam2 subtitleParamHistory;
};

}
