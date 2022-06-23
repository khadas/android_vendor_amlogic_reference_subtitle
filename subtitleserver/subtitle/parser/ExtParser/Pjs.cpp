#define LOG_TAG "Pjs"

#include "Pjs.h"
#include <utils/Log.h>

#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

Pjs::Pjs(std::shared_ptr<DataSource> source): TextSubtitle(source) {
    mPtsRate = 15;
}
Pjs::~Pjs() {
}

/*
 * PJS subtitles reader.
 * That's the "Phoenix japanimation Society" format.
 * I found some of them in http://www.scriptsclub.org/ (used for anime).
 * The time is in tenths of second.
 * almost useless now!
 */
// TODO: C++ style
std::shared_ptr<ExtSubItem> Pjs::decodedItem() {
    char *line = (char *)MALLOC(LINE_LEN+1);
    if (!line) {
        LOGE("[%s::%d] line malloc error!\n", __FUNCTION__, __LINE__);
        return nullptr;
    }
    char *text = (char *)MALLOC(LINE_LEN+1);
    if (!text) {
        LOGE("[%s::%d] text malloc error!\n", __FUNCTION__, __LINE__);
        return nullptr;
    }
    memset(line, 0, LINE_LEN+1);
    memset(text, 0, LINE_LEN+1);
    while (mReader->getLine(line)) {
        ALOGD(" read: %s", line);
        int start, end;
        if (sscanf(line, "%d,%d,%[^\n\r]", &start, &end, text) != 3) {
            continue;
        }
        char *p = trim(text, "\t ");
        p = triml(p, "\t ");
        int len = strlen(p);
        if (p[0] == '"' && p[len-1] == '"') {
            p[len-1] = 0;
            p = p + 1;
        }

        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        item->start = start * 100 / mPtsRate;
        item->end = end * 100 / mPtsRate;;
        item->lines.push_back(std::string(p));
        free(line);
        free(text);
        return item;
    }
    free(line);
    free(text);
    return nullptr;
}

