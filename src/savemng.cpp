#include <savemng.h>
#include <utils/DrawUtils.h>
#include <utils/ConsoleUtils.h>
#include <utils/LanguageUtils.h>
#include <utils/Colors.h>


std::string getNowDate() {
    char time[20];
    sprintf(time,"%02d/%02d/%d %02d:%02d", 01, 10, 2025, 10, 00);
    return std::string(time);
}

void sdWriteDisclaimer(Color bg_color /*= COLOR_BLACK*/) {
    DrawUtils::beginDraw();
    DrawUtils::clear(bg_color);
    Console::consolePrintPosAligned(8, 0, 1, _("Please wait. First write to (some) SDs can take several seconds."));
    DrawUtils::endDraw();
    savemng::firstSDWrite = false;
}

