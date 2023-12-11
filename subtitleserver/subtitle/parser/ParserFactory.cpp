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

#include "ParserFactory.h"

#include "AribB24Parser.h"
#include "TtmlParser.h"
#include "AssParser.h"
#include "DvbParser.h"
#include "DvdParser.h"
#include "PgsParser.h"
#include "TeletextParser.h"
#include "ClosedCaptionParser.h"
#include "Scte27Parser.h"
#include "ExtParser.h"
#include "SmpteTtmlParser.h"

std::shared_ptr<Parser> ParserFactory::create(
            std::shared_ptr<SubtitleParamType> subParam,
            std::shared_ptr<DataSource> source) {
    int type = subParam->subType;


    // TODO: unless we can determine CC type, or default start CC parser
    if (type == TYPE_SUBTITLE_INVALID) {
        type = TYPE_SUBTITLE_CLOSED_CAPTION;
    }

    switch (type) {
        case TYPE_SUBTITLE_VOB:
            return std::shared_ptr<Parser>(new DvdParser(source));

        case TYPE_SUBTITLE_PGS:
            return std::shared_ptr<Parser>(new PgsParser(source));

        case TYPE_SUBTITLE_MKV_STR:
            return std::shared_ptr<Parser>(new AssParser(source));

        case TYPE_SUBTITLE_MKV_VOB:
            return std::shared_ptr<Parser>(new AssParser(source));

        case TYPE_SUBTITLE_SSA:
            return std::shared_ptr<Parser>(new AssParser(source));

        case TYPE_SUBTITLE_TMD_TXT:
            return std::shared_ptr<Parser>(new AssParser(source));

        case TYPE_SUBTITLE_IDX_SUB:
            return std::shared_ptr<Parser>(new AssParser(source));

        case TYPE_SUBTITLE_DVB:
            return std::shared_ptr<Parser>(new DvbParser(source));

        case TYPE_SUBTITLE_DVB_TELETEXT:
            return std::shared_ptr<Parser>(new TeletextParser(source));

        case TYPE_SUBTITLE_DVB_TTML:
            return std::shared_ptr<Parser>(new TtmlParser(source));

        case TYPE_SUBTITLE_ARIB_B24:
            return std::shared_ptr<Parser>(new AribB24Parser(source));

        case TYPE_SUBTITLE_SMPTE_TTML:
            return std::shared_ptr<Parser>(new SmpteTtmlParser(source));

        case TYPE_SUBTITLE_CLOSED_CAPTION: {
            std::shared_ptr<Parser> p = std::shared_ptr<Parser>(new ClosedCaptionParser(source));
            if (p != nullptr) {
                p->updateParameter(type, &subParam->closedCaptionParam);
                p->setPipId(PIP_PLAYER_ID, subParam->playerId);
                p->setPipId(PIP_MEDIASYNC_ID, subParam->mediaId);
            }
            return p;
        }

        case TYPE_SUBTITLE_SCTE27:
            return std::shared_ptr<Parser>(new Scte27Parser(source));

        case TYPE_SUBTITLE_EXTERNAL:
            return std::shared_ptr<Parser>(new ExtParser(source, subParam->idxSubTrackId));

    }

    // TODO: change to stubParser
    return std::shared_ptr<Parser>(new AssParser(source));
}

DisplayType ParserFactory::getDisplayType(int type)
{
    switch (type) {
        case TYPE_SUBTITLE_VOB:
        case TYPE_SUBTITLE_PGS:
        case TYPE_SUBTITLE_MKV_VOB:
        case TYPE_SUBTITLE_DVB:
        case TYPE_SUBTITLE_DVB_TELETEXT:
        case TYPE_SUBTITLE_SCTE27:
        case TYPE_SUBTITLE_SMPTE_TTML:
        case TYPE_SUBTITLE_IDX_SUB:
          return SUBTITLE_IMAGE_DISPLAY;
        case TYPE_SUBTITLE_MKV_STR:
        case TYPE_SUBTITLE_TMD_TXT:
        case TYPE_SUBTITLE_ARIB_B24:
        case TYPE_SUBTITLE_DVB_TTML:
        case  TYPE_SUBTITLE_SSA:
            return SUBTITLE_TEXT_DISPLAY;

        default:
            return SUBTITLE_TEXT_DISPLAY;
    }

}
