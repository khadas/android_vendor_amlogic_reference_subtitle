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

#pragma once

#include <unistd.h>
#include <fcntl.h>
#include <malloc.h>
#include <string>
#include <regex>
#include "list.h"
#include "DataSource.h"

#define  MALLOC(s)      malloc(s)
#define  FREE(d)        free(d)
#define  MEMCPY(d,s,l)  memcpy(d,s,l)
#define  MEMSET(d,s,l)  memset(d,s,l)
#define  MIN(x,y)       ((x)<(y)?(x):(y))
#define  UTF8           unsigned char
#define  UTF16          unsigned short
#define  UTF32          unsigned int
#define ERR ((void *) -1)
#define LINE_LEN                    1024*512

enum {
    AML_ENCODING_NONE = 0,
    AML_ENCODING_UTF8,
    AML_ENCODING_UTF16,
    AML_ENCODING_UTF16BE,
    AML_ENCODING_UTF32,
    AML_ENCODING_UTF32BE
};

inline char * trim(char *str, const char *whitespace) {
    char *p = str + strlen(str);

    while (p > str && strchr(whitespace, *(p-1)) != NULL)
        *--p = 0;
    return str;
}

inline char * triml(char *str, const char *whitespace) {
    char *p = str;

    while (*p && (strchr(whitespace, *p) != NULL))
        p++;
    return p;
}

inline bool isEmptyLine(char *line) {
    if (line == nullptr) return false;

    char *p = trim(line, "\t ");
    p = triml(p, "\t ");
    int len = strlen(p);

    if (len == 0) {
        return true;
    } else if (len == 1) {
        if ((p[0] == '\r') || (p[0] == '\n'))
            return true;
    }
    return false;
}

static inline void removeHtmlToken(std::string &s) {
    std::regex r("(<[^>]*>)");
    bool replaced;
    do {
        replaced = false;
        std::sregex_iterator next(s.begin(), s.end(), r);
        std::sregex_iterator end;
        if (next != end) {
            std::smatch match = *next;
            //ALOGD("%s html", match.str().c_str());
            s.erase(s.find(match.str()), match.str().length());
            replaced = true;
        }
    } while (replaced);
}



class ExtSubStreamReader {

public:
    ExtSubStreamReader(int charset, std::shared_ptr<DataSource> source);
    ~ExtSubStreamReader();

    bool convertToUtf8(int charset, char *s, int inLen);
    int ExtSubtitleEol(char p);
    char *getLineFromString(char *source, char **dest);
    char *strdup(char *src);
    char *strIStr(const char *haystack, const char *needle);
    void trimSpace(char *s) ;
    char *getLine(char *s/*, int fd*/);
    void backtoLastLine();
    bool rewindStream();
    size_t totalStreamSize();

private:
    void detectEncoding();
    int _convertToUtf8(int charset, const UTF16 *in, int inLen, UTF8 *out, int outMax);

    char *mBuffer;
    int mBufferSize;
    unsigned mFileRead;
    unsigned mLastLineLen;
    std::shared_ptr<DataSource> mDataSource;
    int mEncoding;
    size_t mStreamSize;
};


