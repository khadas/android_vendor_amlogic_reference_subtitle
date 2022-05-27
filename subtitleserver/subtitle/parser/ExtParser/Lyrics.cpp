#define LOG_TAG "Lyrics"

#include "Lyrics.h"

Lyrics::Lyrics(std::shared_ptr<DataSource> source): TextSubtitle(source) {
    //mBuffer = new char[LINE_LEN + 1]();
    mBuffer = (char *)MALLOC(LINE_LEN+1);
    memset(mBuffer, 0, LINE_LEN+1);
    mReuseBuffer = false;
    ALOGD("Lyrics");
}

Lyrics::~Lyrics() {
    ALOGD("~Lyrics--");
    free(mBuffer);
}

std::shared_ptr<ExtSubItem> Lyrics::decodedItem() {
    int64_t a1, a2, a3;
    //char text[LINE_LEN + 1];
    char * text = (char *)MALLOC(LINE_LEN+1);
    char *text1 = (char *)MALLOC(LINE_LEN+1);
    memset(text, 0, LINE_LEN+1);
    memset(text1, 0, LINE_LEN+1);
    int pattenLen;

    while (true) {
        if (!mReuseBuffer) {
            if (mReader->getLine(mBuffer) == nullptr) {
                ALOGD("return null");
                free(text);
                free(text1);
                return nullptr;
            }
        }

        // parse start and text
        //TODO for coverity
        if (sscanf(mBuffer, "[%lld:%lld.%lld]%[^\n\r]", &a1, &a2, &a3, text) < 4) {
            mReuseBuffer = false;
            // fail, check again.
            continue;
        }

        //check the text
        int64_t b1;
        int cnt = 0;
        while (sscanf(text, "[%lld:%lld.%lld]%[^\n\r]", &b1, &b1, &b1, text1) == 4) {
            if (cnt == 0) {
                MEMCPY(mBuffer, text, LINE_LEN+1);//use for next decodeItem
            }
            MEMCPY(text, text1, LINE_LEN+1);
            cnt ++;
        }

        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        item->start = a1 * 6000 + a2 * 100 + a3;
        item->end = item->start + 400;
        item->lines.push_back(std::string(text));
        //ALOGD("item start:%lld,end:%lld",item->start, item->end);
        //maybe such as this: [03:37.00][03:02.00][01:31.00] hello world
        if (cnt > 0) {
            //ALOGD("mBuffer after copy:%s", mBuffer);
            mReuseBuffer = true;
            free(text);
            free(text1);
            return item;
        } else {
        // get time End, maybe has end time, maybe not, handle this case.
            if (mReader->getLine(mBuffer) == nullptr) {
                ALOGD("file end, read null");
                free(text);
                free(text1);
                mReuseBuffer = false;//no this, may can't break while loop
                return item;
            }
            // has end??
            pattenLen = sscanf(mBuffer, "[%lld:%lld.%lld]%[^\n\r]", &a1, &a2, &a3, text);
            if (pattenLen == 4) {
                mReuseBuffer = true;
                //ALOGD("reusebuffer,text:%s",text);
                free(text);
                free(text1);
                return item;
            } else if (pattenLen == 3) {
                item->end = a1 * 6000 + a2 * 100 + a3;
                //ALOGD("item end:%lld",item->end);
            }

            mReuseBuffer = false;
            free(text);
            free(text1);
            return item;
        }
    }
    free(text);
    free(text1);
    return nullptr;
}

