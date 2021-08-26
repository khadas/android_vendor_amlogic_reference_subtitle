#define LOG_TAG "Mplayer2"

#include "Mplayer2.h"

Mplayer2::Mplayer2(std::shared_ptr<DataSource> source): TextSubtitle(source) {
}

Mplayer2::~Mplayer2() {
}

std::shared_ptr<ExtSubItem> Mplayer2::decodedItem() {
    char line[LINE_LEN+1];
    char line2[LINE_LEN];

    while (mReader->getLine(line)) {
        int start =0, end = 0;
        ALOGD(" read: %s", line);
        if (sscanf(line, "[%d][%d]%[^\r\n]", &start, &end, line2) < 3) {
                continue;
        }


        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        item->start = start * 10;
        item->end = end * 10;
        std::string s(line2);
        item->lines.push_back(s);
        return item;
    }
    return nullptr;
}

