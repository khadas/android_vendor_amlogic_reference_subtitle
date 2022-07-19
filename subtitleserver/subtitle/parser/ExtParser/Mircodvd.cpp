#define LOG_TAG "Mircodvd"

#include "Mircodvd.h"
#include <utils/Log.h>

#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

Mircodvd::Mircodvd(std::shared_ptr<DataSource> source): TextSubtitle(source) {
    // default rate
    mPtsRate = 15;
}

Mircodvd::~Mircodvd() {
}

std::shared_ptr<ExtSubItem> Mircodvd::decodedItem() {
    char *line = (char *)MALLOC(LINE_LEN+1);
    if (!line) {
        LOGE("[%s::%d] line malloc error!\n", __FUNCTION__, __LINE__);
        return nullptr;
    }
    char *line2 = (char *)MALLOC(LINE_LEN);
    if (!line2) {
        LOGE("[%s::%d] line2 malloc error!\n", __FUNCTION__, __LINE__);
        free(line);
        return nullptr;
    }
    memset(line, 0, LINE_LEN+1);
    memset(line2, 0, LINE_LEN);
    while (mReader->getLine(line)) {
        int start =0, end = 0;
        if (sscanf (line, "{%d}{%d}%[^\r\n]", &start, &end, line2) < 3) {
            if (sscanf(line, "{%d}{}%[^\r\n]", &start, line2) < 2) {
                continue;
            }
        }

        if (start == 1) {
            if (atoi(line2) > 0) {
                mPtsRate = atoi(line2);
            }
            continue;
        }

        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        item->start = start*100/mPtsRate;
        item->end = end*100/mPtsRate;
        std::string s(line2);
        item->lines.push_back(s);
        free(line);
        free(line2);
        return item;
    }
    free(line);
    free(line2);
    return nullptr;
}

