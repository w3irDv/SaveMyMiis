#pragma once

#define _(STRING) LanguageUtils::gettext(STRING)

namespace LanguageUtils {

    const char *gettext(const char *msg) __attribute__((__hot__));

};