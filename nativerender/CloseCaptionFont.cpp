#define LOG_TAG "CloseCaptionFont"

#include "CloseCaptionFont.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>


#include <SkColor.h>
#include <SkPaint.h>
#include <SkPath.h>

#include "SubtitleLog.h"
#include "ZipFileRO.h"

namespace Amlogic {
namespace NativeRender {
namespace CloseCaption {


static const char* kResourcePath[] = {
        "/system_ext/etc/font.bin",
        "/system/etc/font.bin",
        "/vendor/etc/font.bin.vendor"
};

static const char*kTmpPath = "/tmp";
static const char*kFontPath = "/font";

#define ENTRY_NAME_MAX 256



FontManager& FontManager::Instance() {
    return _instance;
}

FontManager FontManager::_instance;


FontManager::FontManager() {

    initFontResource();

    std::string fontPath = getApplicationPath() +"/font/";
    //std::string fontPath("/data/data/com.droidlogic.videoplayer/font/");
    errno = 0;
    mMonoSerifTf    = SkTypeface::MakeFromFile((fontPath+"cinecavD_mono.ttf").c_str());
    if (mMonoSerifTf == nullptr) {
        mMonoSerifTf = getFallbackFonts("mono_sans", false);
    }
    SUBTITLE_LOGI("FontManager mMonoSerifTf:%p %d", mMonoSerifTf.get(), errno);

    mMonoSerifItTf  = SkTypeface::MakeFromFile((fontPath+"cinecavD_mono_it.ttf").c_str());
    if (mMonoSerifItTf == nullptr) {
        mMonoSerifItTf = getFallbackFonts("mono_sans", true);
    }

    mCasualTf       = SkTypeface::MakeFromFile((fontPath+"cinecavD_casual.ttf").c_str());
    if (mCasualTf == nullptr) {
        mCasualTf = getFallbackFonts("casual", false);
    }

    mCasualItTf     = SkTypeface::MakeFromFile((fontPath+"cinecavD_casual_it.ttf").c_str());
    if (mCasualTf == nullptr) {
        mCasualTf = getFallbackFonts("casual", true);
    }

    mPropSansTf     = SkTypeface::MakeFromFile((fontPath+"cinecavD_serif.ttf").c_str());
    mPropSansItTf   = SkTypeface::MakeFromFile((fontPath+"cinecavD_serif_it.ttf").c_str());
    mSmallCapitalTf = SkTypeface::MakeFromFile((fontPath+"cinecavD_sc.ttf").c_str());
    mSmallCapitalItTf = SkTypeface::MakeFromFile((fontPath+"cinecavD_sc_it.ttf").c_str());
    mCursiveTf      = SkTypeface::MakeFromFile((fontPath+"cinecavD_script.ttf").c_str());
    mCursiveItTf    = SkTypeface::MakeFromFile((fontPath+"cinecavD_script_it.ttf").c_str());


    errno = 0;
    mFallbackCJK = SkTypeface::MakeFromFile("/system/fonts/NotoSansCJK-Regular.ttc");

    SUBTITLE_LOGI("mFallbackCJK%s =%p %d", "/system/fonts/NotoSansCJK-Regular.ttc", mFallbackCJK.get(), errno);

    SUBTITLE_LOGI("FontManager %d", access((fontPath+"cinecavD_mono.ttf").c_str(), R_OK));

    FILE *fp = fopen((fontPath+"cinecavD_mono.ttf").c_str(), "r");
    SUBTITLE_LOGI("open File: %s fp=%p",(fontPath+"cinecavD_mono.ttf").c_str(), fp);
}


bool FontManager::initFontResource() {
    errno = 0;
    std::string workpath = getApplicationPath();
    SUBTITLE_LOGI("workpath:%s", workpath.c_str());
    std::string fPath = getApplicationPath() +"/font/";

    if (::access(fPath.c_str(), R_OK) != 0) {
        if (::mkdir(fPath.c_str(), 0775) != 0) {
            SUBTITLE_LOGE("Error, cannot create path!:%s %d", fPath.c_str(), errno);
            return false;
        }
    } else {
        return true;
    }
    for (const auto p : kResourcePath) {
        if (uncryptFontTo(p, fPath.c_str())) return true;
    }

    return false;
}
extern "C" int upzipBuffer2File(const char*inBuf, size_t size, const char*destPath);
bool FontManager::uncryptFontTo(const char*srcPath, const char*destPath) {
    bool result = false;
    android::ZipFileRO *zip = android::ZipFileRO::open(srcPath);
    if (zip == nullptr) {
      SUBTITLE_LOGE("Failed to open file \"%s\": %s", srcPath, strerror(errno));
      return false;
    } else {
        SUBTITLE_LOGI("open Zip file:%p", zip);
    }

    void *cookie = nullptr;
    if (!zip->startIteration(&cookie)) {
        delete zip;
        return false;
    }

    android::ZipEntryRO entry;
    char name[ENTRY_NAME_MAX] = {0};
    while ((entry = zip->nextEntry(cookie)) != nullptr) {
        const int foundEntryName = zip->getEntryFileName(entry, name, ENTRY_NAME_MAX);
        if (foundEntryName > ENTRY_NAME_MAX || foundEntryName == -1) {
            SUBTITLE_LOGE("Error fetching entry file name");
            continue;
        }

        if (strlen(name) > 0) {
            SUBTITLE_LOGI("Got Item :%s", name);
        }

        uint16_t method;
        uint32_t uncompressedLen;
        if (!zip->getEntryInfo(entry, &method, &uncompressedLen, NULL, NULL, NULL, NULL)) {
            SUBTITLE_LOGE("Error getEntryInfo failed for %s", name);
            continue;
        }

        android::FileMap* dataMap = zip->createEntryFileMap(entry);
        if (dataMap == NULL) {
            SUBTITLE_LOGE("create map from entry failed\n");
            continue;
        }

        if (method == android::ZipFileRO::kCompressDeflated) {
            SUBTITLE_LOGI("ZipCompressed!");
        } else if (method == android::ZipFileRO::kCompressStored) {
            SUBTITLE_LOGI("store!");
            std::string destFile(destPath);
            destFile +="/";
            destFile += name;
            result = upzipBuffer2File((const char *)dataMap->getDataPtr(),
                dataMap->getDataLength(), destFile.c_str()) > 0;
        }

        delete dataMap;

    }


    return result;
}

sk_sp<SkTypeface> FontManager::getFallbackFonts(std::string typeName, bool isItalic) {
    SUBTITLE_LOGE("getFallback font:%s", typeName.c_str());
    return nullptr;
}

sk_sp<SkTypeface> FontManager::typeFaceFromName(std::string typeName, bool isItalic, bool isMultiByte) {
    SUBTITLE_LOGE("typeFaceFromName font:%s isMultiByte=%d", typeName.c_str(), isMultiByte);

    if (isMultiByte) {
        return mFallbackCJK;
    }

    if (ignoreCaseCompare(typeName, "default")) {
        return isItalic ? mMonoSerifItTf : mMonoSerifTf;
    } else if (ignoreCaseCompare(typeName, "mono_serif")) {
        return isItalic ? mMonoSerifItTf : mMonoSerifTf;
    } else if (ignoreCaseCompare(typeName, "prop_serif")) {
        return isItalic ? mPropSansItTf : mPropSansTf;
    } else if (ignoreCaseCompare(typeName, "mono_sans")) {
        return isItalic ? mMonoSerifItTf : mMonoSerifTf;
    } else if (ignoreCaseCompare(typeName, "prop_sans")) {
        return isItalic ? mPropSansItTf : mPropSansTf;
    } else if (ignoreCaseCompare(typeName, "casual")) {
        return isItalic ? mCasualItTf : mCasualTf;
    } else if (ignoreCaseCompare(typeName, "cursive")) {
        return isItalic ? mCursiveItTf : mCursiveTf;
    } else if (ignoreCaseCompare(typeName, "small_caps")) {
        return isItalic ? mSmallCapitalItTf : mSmallCapitalTf;
    }
    /* For caption manager convert */
    else if (ignoreCaseCompare(typeName, "sans-serif")) {
        return mPropSansTf;
    } else if (ignoreCaseCompare(typeName, "sans-serif-condensed")) {
        return mPropSansTf;
    } else if (ignoreCaseCompare(typeName, "sans-serif-monospace")) {
        return mMonoSerifTf;
    } else if (ignoreCaseCompare(typeName, "serif")) {
        return mPropSansTf;
    } else if (ignoreCaseCompare(typeName, "serif-monospace")) {
        return mMonoSerifTf;
    //} else if (ignoreCaseCompare(typeName, "casual")) {
    //    return mCasualTf;
    //} else if (ignoreCaseCompare(typeName, "cursive")) {
    //    return  mMonoSerifTf;
    } else if (ignoreCaseCompare(typeName, "small-capitals")) {
        return mSmallCapitalTf;
    //} else if (ignoreCaseCompare(typeName, "fallback")) {
    }

    SUBTITLE_LOGE("font match exception, no match font for %s! ", typeName.c_str());
    return isItalic ? mMonoSerifItTf : mMonoSerifTf;
}


bool FontManager::isMonoFont(std::string font) {
     if (ignoreCaseCompare(font, "default") ||
        ignoreCaseCompare(font, "mono_serif") ||
        ignoreCaseCompare(font, "sans-serif-monospace") ||
        ignoreCaseCompare(font, "prop_serif") ||
        ignoreCaseCompare(font, "serif-monospace") ||
        ignoreCaseCompare(font, "cursive") ||
        ignoreCaseCompare(font, "mono_sans")) {
        return true;
    }
    return false;
}



} // namespace CloseCaption
} // namespace NativeRender
} //namespace Amlogic

