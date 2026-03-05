#pragma once
#include <cstdint>

typedef struct NetConfMACAddr NetConfMACAddr;

struct NetConfMACAddr {
    uint8_t MACAddr[0x6];
};


namespace nsysnet {
    typedef uint16_t NetConfInterfaceType;

    inline int netconf_get_if_macaddr([[maybe_unused]] NetConfInterfaceType interface, NetConfMACAddr *info) {
        info->MACAddr[0] = 1;
        return 0;
    };
} // namespace nsysnet
