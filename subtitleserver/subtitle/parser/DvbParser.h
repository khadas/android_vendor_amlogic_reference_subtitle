#ifndef __SUBTITLE_DVB_PARSER_H__
#define __SUBTITLE_DVB_PARSER_H__
#include "Parser.h"
#include "DataSource.h"
#include "sub_types.h"

#include "dvbCommon.h"

struct DVBSubContext;
struct DVBSubObjectDisplay;

class DvbParser: public Parser {
public:
    DvbParser(std::shared_ptr<DataSource> source);
    virtual ~DvbParser();
    virtual int parse();
    virtual bool updateParameter(int type, void *data);
    virtual void dump(int fd, const char *prefix);
    void notifyCallerAvail(int avail);

private:
    int getSpu();
    int getInterSpu();
    int getDvbSpu();
    int decodeSubtitle(std::shared_ptr<AML_SPUVAR> spu, char *pSrc, const int size);

    int softDemuxParse();
    int hwDemuxParse();

    void parsePageSegment(const uint8_t *buf, int bufSize);
    void parseRegionSegment(const uint8_t *buf, int bufSize);
    void parseClutSegment(const uint8_t *buf, int bufSize);
    int parseObjectSegment(const uint8_t *buf, int bufSize, int cntObject, int totalObject);
    void parseDisplayDefinitionSegment(const uint8_t *buf, int bufSize);
    int parsePixelDataBlock(DVBSubObjectDisplay *display,
            const uint8_t *buf, int bufSize, int top_bottom, int non_mod);


    int displayEndSegment(std::shared_ptr<AML_SPUVAR> spu);

    void saveResult2Spu(std::shared_ptr<AML_SPUVAR> spu);
    void notifySubtitleDimension(int width, int height);
    void notifySubtitleErrorInfo(int error);

    void checkDebug();

    int init();

    void callbackProcess();
    DVBSubContext *mContext;

    int mDumpSub;

    //CV for notification.
    std::mutex mMutex;
    std::condition_variable mCv;
    int mPendingAction;
    std::shared_ptr<std::thread> mTimeoutThread;
};


#endif

