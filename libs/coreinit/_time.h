#pragma once

#include <cstdint>
#include <ctime>

typedef int64_t OSTime;
inline OSTime OSGetTime() {
    //return 0xb5aa99f4c46967;
    return 0x6769c4f499aab500;
}

typedef int32_t OSTick;
inline OSTick OSGetTick() {
    return (OSTick) std::time(0);
} 	

