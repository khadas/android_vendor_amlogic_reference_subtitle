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

#define LOG_TAG "SubStationAlpha"

#include <vector>

#include<iostream>
#include <sstream>
#include "SubStationAlpha.h"

#include <utils/Log.h>

SubStationAlpha::SubStationAlpha(std::shared_ptr<DataSource> source) : TextSubtitle(source) {
    ALOGD("SubStationAlpha");

    mSubData.format = "Sub Station Alpha";

}

SubStationAlpha::~SubStationAlpha() {
}


// TODO: extends to a fancy script section parser.

/*
    retrieve the subtitle content from the decoded buffer.
    If is the origin ass event buffer, also fill the time stamp info.

    ASS type:
    Format: Layer, Start, End, Style, Actor, MarginL, MarginR, MarginV, Effect, Text
    Dialogue: 0,0:00:00.26,0:00:01.89,Default,,0000,0000,0000,,Don't move

    ssa type and example:
    Format: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
    Dialogue: Marked=0,0:00:00.26,0:00:01.89,Default,NTP,0000,0000,0000,!Effect,Don't move
*/
std::shared_ptr<ExtSubItem> SubStationAlpha::decodedItem() {
    /*
     * Sub Station Alpha v4 (and v2?) scripts have 9 commas before subtitle
     * other Sub Station Alpha scripts have only 8 commas before subtitle
     * Reading the "ScriptType:" field is not reliable since many scripts appear
     * w/o it
     *
     * http://www.scriptclub.org is a good place to find more examples
     * http://www.eswat.demon.co.uk is where the SSA specs can be found
     */
    const int ASS_EVENT_SECTIONS = 9;

    char line[LINE_LEN + 1];
    ExtSubItem *e = NULL; //for coverity
    while (mReader->getLine(line)) {
        std::stringstream ss;
        std::string str;
        std::vector<std::string> items; // store the event sections in vector
        ss << line;
        int count = 0;
        ALOGD("%s", line);
        for (count=0; count<ASS_EVENT_SECTIONS; count++) {
            if (!getline(ss, str, ',')) break;
            items.push_back(str);
        }

        if (count < ASS_EVENT_SECTIONS) continue;

         // get the subtitle content. here we do not need other effect and margin data.
        getline(ss, str);

//====================================================
         // currently not support style control code rendering
         // discard the unsupported {} Style Override control codes

        // TODO: parser styles!
        std::size_t start, end;
        while ((start = str.find("{\\")) != std::string::npos) {
            end = str.find('}', start);
            if (end != std::string::npos) {
                str.erase(start, end-start+1);
            } else {
                break;
            }
        }
//===================================================

        // replace "\n"
        while ((start = str.find("\\n")) != std::string::npos
                || (start = str.find("\\N")) != std::string::npos) {
            std::string newline = "\n";
            str.replace(start, 2, newline);
        }

        uint32_t hour, min, sec, ms;
        // 1 is start time
        ExtSubItem *item = new ExtSubItem();
        e = item;
        if ( sscanf(items[1].c_str(), "%d:%d:%d.%d", &hour, &min, &sec, &ms) != 4) continue;
        item->start = (hour * 60 * 60 + min * 60 + sec) * 100 + ms;

        // 2 is end time.
        if ( sscanf(items[2].c_str(), "%d:%d:%d.%d", &hour, &min, &sec, &ms) != 4) continue;
        item->end = (hour * 60 * 60 + min * 60 + sec) * 100 + ms;

        item->lines.push_back(str);
        return std::shared_ptr<ExtSubItem>(item);
    }
    delete (e);
    return nullptr;
}



