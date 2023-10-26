#define LOG_TAG "Mplayer1"

#include "Mplayer1.h"
#include <utils/Log.h>


Mplayer1::Mplayer1(std::shared_ptr<DataSource> source): TextSubtitle(source) {
    // default rate
    mPtsRate = 15;
}

Mplayer1::~Mplayer1() {
}

std::shared_ptr<ExtSubItem> Mplayer1::decodedItem() {
    char *line = (char *)MALLOC(LINE_LEN+1);
    if (!line) {
        ALOGE("[%s::%d] line malloc error!\n", __FUNCTION__, __LINE__);
        return nullptr;
    }
    char *line2 = (char *)MALLOC(LINE_LEN);
    if (!line2) {
        ALOGE("[%s::%d] line2 malloc error!\n", __FUNCTION__, __LINE__);
        free(line);
        return nullptr;
    }
    memset(line, 0, LINE_LEN+1);
    memset(line2, 0, LINE_LEN);
    while (mReader->getLine(line)) {
        int start =0, end = 0, tmp;
        ALOGD(" read: %s", line);
        if (sscanf(line, "%d,%d,%d,%[^\r\n]", &start, &end, &tmp, line2) < 4) {
                continue;
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


