#pragma once

#include <string>
#include <utils/Colors.h>
#include <utils/DrawUtils.h>

std::string getNowDate();
void sdWriteDisclaimer(Color bg_color = COLOR_BLACK);

namespace savemng {
    inline bool firstSDWrite = true;
};