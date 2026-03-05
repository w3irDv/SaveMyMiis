#pragma once

#include <coreinit/mcp.h>
#include <nn/act/client_cpp.h>
#include <nsysnet/netconfig.h>
#include <string>

namespace AmbientConfig {

    bool get_device_hash();
    bool get_mac_address();
    bool get_author_id();
    void getWiiUSerialId();

    inline char device_hash[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    inline NetConfMACAddr mac_address = {.MACAddr = {0, 0x17, 0xab, 0xe1, 0x82, 0x29}};
    //inline uint64_t author_id = 0x12345789abcdef01;
    inline uint64_t author_id = 0x034000044416efbc;
    inline std::string unknownSerialId{"_WIIU_"};
    inline std::string thisConsoleSerialId = unknownSerialId;
    inline MCPRegion thisConsoleRegion;

}; // namespace AmbientConfig
