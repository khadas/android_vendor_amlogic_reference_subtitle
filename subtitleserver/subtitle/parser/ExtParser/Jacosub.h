/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */

#pragma once

#include "TextSubtitle.h"

class Jacosub: public TextSubtitle {

public:
    Jacosub(std::shared_ptr<DataSource> source);
    ~Jacosub();

protected:
    virtual std::shared_ptr<ExtSubItem> decodedItem();

private:
    void parseHeaderInfo();
    int mTimerRes;
    int mShift;
};



