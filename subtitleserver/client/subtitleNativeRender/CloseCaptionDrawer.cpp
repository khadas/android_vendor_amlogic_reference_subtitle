
#include "CloseCaptionDrawer.h"
#include "CloseCaption.h"
#include "SubtitleLog.h"

using Amlogic::NativeRender::CloseCaption::Configure;

CloseCaptionDrawer& CloseCaptionDrawer::GetInstance() {
    return _instance;
}
CloseCaptionDrawer CloseCaptionDrawer::_instance;

CloseCaptionDrawer::CloseCaptionDrawer() {
    mConfig = std::shared_ptr<Configure>(new Configure());
}

CloseCaptionDrawer::~CloseCaptionDrawer() {

}


void CloseCaptionDrawer::draw(SkCanvas &canvas, const char *str) {
    SUBTITLE_LOGI("draw %s", str);
    Amlogic::NativeRender::CloseCaption::CloseCaption cc(mConfig);
    cc.parserJson(str);
    cc.draw(canvas);
}
