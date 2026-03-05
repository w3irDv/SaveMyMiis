#pragma once

#include "miisavemng.h"
//#include <BackupSetList.h>
#include <jansson.h>
#include <mii/Mii.h>
#include <savemng.h>
#include <string>
#include <utils/AmbientConfig.h>

class Metadata {
public:
    Metadata(MiiRepo *mii_repo, uint8_t slot) : slot(slot),
                                                path(MiiSaveMng::getMiiRepoBackupPath(mii_repo, slot).append("/savemiiMeta.json")),
                                                serialId(AmbientConfig::thisConsoleSerialId), repo_name(mii_repo->repo_name) {};

    bool read();
    bool write();
    std::string simpleFormat();
    std::string get();
    bool set(const std::string &date, bool isUSB);
    std::string getDate() { return Date; };
    std::string getTag() { return tag; };
    void setTag(const std::string &tag_) { this->tag = tag_; };
    std::string getSerialId() { return serialId; };
    uint32_t getVWiiHighID() { return this->vWiiHighID; };

    bool read_mii();
    bool write_mii();
    void setDate(const std::string &Date) { this->Date = Date; };
    void setStorage(const std::string &storage) { this->storage = storage; };

private:
    uint32_t highID;
    uint32_t lowID;
    uint32_t vWiiHighID;
    uint32_t vWiiLowID;
    char shortName[256];
    uint8_t slot;
    std::string path;

    std::string Date;
    std::string storage;
    std::string serialId;
    std::string tag;

    std::string repo_name;
};
