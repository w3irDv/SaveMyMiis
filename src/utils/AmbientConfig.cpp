#include <utils/AmbientConfig.h>
#include <utils/ConsoleUtils.h>
#include <utils/LanguageUtils.h>

bool AmbientConfig::get_device_hash() {  
    return true;
}

bool AmbientConfig::get_mac_address() {

        //Console::showMessage(ERROR_CONFIRM, "ret: %d  mac:%02x:%02x:%02x:%02x:%02x:%02x",
         //                    ret, mac_address.MACAddr[0], mac_address.MACAddr[1], mac_address.MACAddr[2], mac_address.MACAddr[3], mac_address.MACAddr[4], mac_address.MACAddr[5]);

    return true;
}


void AmbientConfig::getWiiUSerialId() {
    return;
}

bool AmbientConfig::get_author_id() {
    return true;
}

