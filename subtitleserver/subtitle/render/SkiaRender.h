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

#ifndef __SUBTITLE_SKIA_RENDER_H__
#define __SUBTITLE_SKIA_RENDER_H__

#include <memory>
#include <list>
#include <mutex>
#include "Parser.h"

#include "Render.h"
#include "Display.h"

class SkiaRender : public Render {
public:
    // TODO: we ask display from render, or upper layer?
    SkiaRender(std::shared_ptr<Display> display);
    //SkiaRender() = delete;
    virtual ~SkiaRender() {};

    // TODO: the subtitle may has some params, config how to render
    //       Need impl later.
    virtual bool showSubtitleItem(std::shared_ptr<AML_SPUVAR> spu,int type);
    virtual bool hideSubtitleItem(std::shared_ptr<AML_SPUVAR> spu);
    virtual void resetSubtitleItem();
private:
    bool drawAndPost();

    std::shared_ptr<Display> mDisplay;

    std::list<std::shared_ptr<AML_SPUVAR>> mShowingSubs;

    // currently, all the render is in present thread. no need lock
    // however, later may changed, add one here.
    std::mutex mLock;
};

#endif

