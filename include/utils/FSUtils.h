#pragma once

#include <LockingQueue.h>
#include <coreinit/filesystem_fsa.h>
#include <future>
#include <string>
#include <utils/Colors.h>

#define IO_MAX_FILE_BUFFER              (1024 * 1024) // 1 MB
#define __FSAShimSend                   ((FSError (*)(FSAShimBuffer *, uint32_t))(0x101C400 + 0x042d90))

#define GENERATE_FAT32_TRANSLATION_FILE true

struct file_buffer {
    void *buf;
    size_t len;
    size_t buf_size;
};

namespace FSUtils {


    int checkEntry(const char *fPath);
    bool readThread(FILE *srcFile, LockingQueue<file_buffer> *ready, LockingQueue<file_buffer> *done);
    bool writeThread(FILE *dstFile, LockingQueue<file_buffer> *ready, LockingQueue<file_buffer> *done, size_t /*totalSize*/);
    bool copyFileThreaded(FILE *srcFile, FILE *dstFile, size_t totalSize, const std::string &pPath, const std::string &oPath);
    bool copyFile(const std::string &sPath, const std::string &initial_tPath, bool if_generate_FAT32_translation_file = false);
    bool copyDir(const std::string &sPath, const std::string &initial_tPath, bool if_generate_FAT32_translation_file = false);
    bool removeDir(const std::string &pPath);
    bool folderEmpty(const char *fPath);
    bool folderEmptyIgnoreSavemii(const char *fPath);
    bool createFolder(const char *path);
    bool createFolderUnlocked(const std::string &path);
    bool setOwnerAndMode(uint32_t owner, uint32_t group, FSMode mode, std::string path, FSError &fserror);
    bool setOwnerAndModeRec(uint32_t owner, uint32_t group, FSMode mode, std::string path, FSError &fserror);
    void flushVol(const std::string &srcPath);
    int slc_resilient_rename(std::string &src_path, std::string &dst_path);

    void deinit_fs_buffers();

    inline size_t getFilesize(FILE *fp) {
        fseek(fp, 0L, SEEK_END);
        size_t fsize = ftell(fp);
        fseek(fp, 0L, SEEK_SET);
        return fsize;
    }

    inline std::string getUSB() {
        return "storage_usb01:";
    }

} // namespace FSUtils