#pragma once

#include <cstdint>
#include <ctime>

#include <utils/MiiUtils.h>

typedef int64_t OSTime;
inline OSTime OSGetTime() {

    uint64_t osticks_since_2010 = ((uint64_t) MiiUtils::generate_timestamp(2000,1)) * 0x3b46dda;
    return (osticks_since_2010);
}

typedef int32_t OSTick;
inline OSTick OSGetTick() {
    // does not mimick OSGetTick wii_u function, just something that changes with time is needed 
    return (OSTick) std::time(0);
} 	

