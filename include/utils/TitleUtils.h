#pragma once
#include <cstdint>


namespace TitleUtils {
    inline uint32_t getMiiMakerOwner() {
        return 0x1004a200;
    }

    inline bool getMiiMakerisTitleOnUSB() { return false; }
}; // namespace TitleUtils