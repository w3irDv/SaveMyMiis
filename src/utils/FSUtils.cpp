#include <cstdlib>
#include <dirent.h>
#include <malloc.h>
//#include <mocha/mocha.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utils/ConsoleUtils.h>
//#include <utils/EscapeFAT32Utils.h>
#include <utils/FSUtils.h>
#include <utils/InProgress.h>
#include <utils/LanguageUtils.h>
#include <utils/StringUtils.h>

static file_buffer buffers[16];
static char *fileBuf[2];
static bool buffersInitialized = false;

static size_t written = 0;
static int readError;
static int writeError;

void FSUtils::deinit_fs_buffers() {
    if (buffersInitialized) {
        for (auto &buffer : buffers) {
            if (buffer.buf != nullptr)
                free(buffer.buf);
        }
        for (auto &buffer : fileBuf) {
            if (buffer != nullptr)
                free(buffer);
        }
        buffersInitialized = false;
    }
}

int FSUtils::checkEntry(const char *fPath) {
    struct stat st{};
    if (stat(fPath, &st) == -1)
        return 0; // path does not exist

    if (S_ISDIR(st.st_mode))
        return 2; // is a directory

    return 1; // is a file
}


bool FSUtils::readThread(FILE *srcFile, LockingQueue<file_buffer> *ready, LockingQueue<file_buffer> *done) {
    file_buffer currentBuffer{};
    ready->waitAndPop(currentBuffer);
    while ((currentBuffer.len = fread(currentBuffer.buf, 1, currentBuffer.buf_size, srcFile)) > 0) {
        done->push(currentBuffer);
        ready->waitAndPop(currentBuffer);
    }
    done->push(currentBuffer);
    if (ferror(srcFile) != 0) {
        readError = errno;
        return false;
    }
    return true;
}


bool FSUtils::writeThread(FILE *dstFile, LockingQueue<file_buffer> *ready, LockingQueue<file_buffer> *done, size_t /*totalSize*/) {
    uint bytes_written;
    file_buffer currentBuffer{};
    ready->waitAndPop(currentBuffer);
    written = 0;
    while (currentBuffer.len > 0 && (bytes_written = fwrite(currentBuffer.buf, 1, currentBuffer.len, dstFile)) == currentBuffer.len) {
        done->push(currentBuffer);
        ready->waitAndPop(currentBuffer);
        written += bytes_written;
    }
    done->push(currentBuffer);
    if (ferror(dstFile) != 0) {
        writeError = errno;
        return false;
    }
    return true;
}

bool FSUtils::copyFileThreaded(FILE *srcFile, FILE *dstFile, size_t totalSize, const std::string &pPath, const std::string &oPath) {
    LockingQueue<file_buffer> read;
    LockingQueue<file_buffer> write;
    for (auto &buffer : buffers) {
        if (!buffersInitialized) {
            buffer.buf = aligned_alloc(0x40, IO_MAX_FILE_BUFFER);
            buffer.len = 0;
            buffer.buf_size = IO_MAX_FILE_BUFFER;
        }
        read.push(buffer);
    }
    if (!buffersInitialized) {
        fileBuf[0] = static_cast<char *>(aligned_alloc(0x40, IO_MAX_FILE_BUFFER));
        fileBuf[1] = static_cast<char *>(aligned_alloc(0x40, IO_MAX_FILE_BUFFER));
    }
    buffersInitialized = true;
    setvbuf(srcFile, fileBuf[0], _IOFBF, IO_MAX_FILE_BUFFER);
    setvbuf(dstFile, fileBuf[1], _IOFBF, IO_MAX_FILE_BUFFER);

    std::future<bool> readFut = std::async(std::launch::async, readThread, srcFile, &read, &write);
    std::future<bool> writeFut = std::async(std::launch::async, writeThread, dstFile, &write, &read, totalSize);
    uint32_t passedMs = 1;
    do {
        //DrawUtils::beginDraw();
        //DrawUtils::clear(COLOR_BLACK);
        //showFileOperation(basename(pPath.c_str()), pPath, oPath);
        Console::consolePrintPos(-2, 17, "%s > %s", pPath.c_str(), oPath.c_str());
        Console::consolePrintPos(-2, 17, "Bytes Copied: %d of %d (%i kB/s)", written, totalSize, (uint32_t) (((uint64_t) written * 1000) / ((uint64_t) 1024 * passedMs)));
        //DrawUtils::endDraw();
    } while (std::future_status::ready != writeFut.wait_for(std::chrono::milliseconds(2)));
    bool success = readFut.get() && writeFut.get();
    return success;
}


bool FSUtils::copyFile(const std::string &sPath, const std::string &initial_tPath, [[maybe_unused]] bool if_generate_FAT32_translation_file /*= false*/) {

    std::string tPath(initial_tPath);
    if (sPath.find("savemiiMeta.json") != std::string::npos)
        return true;
    if (sPath.find("savemii_saveinfo.xml") != std::string::npos && tPath.find("/meta/saveinfo.xml") == std::string::npos)
        return true;
    FILE *source = fopen(sPath.c_str(), "rb");
    if (source == nullptr) {
        Console::showMessage(ERROR_CONFIRM, _("Cannot open file for read\n\n%s\n\n%s"), sPath.c_str(), strerror(errno));
        return false;
    }

    FILE *dest = fopen(tPath.c_str(), "wb");
    if (dest == nullptr) {
        fclose(source);
        Console::showMessage(ERROR_CONFIRM, _("Cannot open file for write\n\n%s\n\n%s"), tPath.c_str(), strerror(errno));
        return false;
    }

    //copy_file:

    size_t sizef = getFilesize(source);

    readError = 0;
    writeError = 0;
    bool success = copyFileThreaded(source, dest, sizef, sPath, tPath);

    fclose(source);

    std::string writeErrorStr{};

    if (writeError > 0)
        writeErrorStr.assign(strerror(writeError));

    if (fclose(dest) != 0) {
        success = false;
        if (writeError != 0)
            writeErrorStr.append("\n" + std::string(strerror(errno)));
        else {
            writeError = errno;
            writeErrorStr.assign(strerror(errno));
        }
        if (errno == 28) { // let's warn about the "no space left of device" that is generated when copy common savedata without wiping
            if (tPath.starts_with("storage"))
                writeErrorStr.append(_("\n\nbeware: if you haven't done it, try to wipe savedata before restoring"));
        }
    }

    chmod(tPath.c_str(), 0766);

    if (!success) {
        if (readError > 0) {
            Console::showMessage(ERROR_CONFIRM, _("Read error\n\n%s\n\n reading from %s"), strerror(readError), sPath.c_str());
        }
        if (writeError > 0) {
            Console::showMessage(ERROR_CONFIRM, _("Write error\n\n%s\n\n copying to %s"), writeErrorStr.c_str(), sPath.c_str());
        }
    }

    return success;
}


bool FSUtils::copyDir(const std::string &sPath, const std::string &initial_tPath, bool if_generate_FAT32_translation_file /*= false*/) { // Source: ft2sd

    std::string tPath(initial_tPath);
    DIR *dir = opendir(sPath.c_str());
    if (dir == nullptr) {
        InProgress::copyErrorsCounter++;
        Console::showMessage(ERROR_CONFIRM, _("Error opening source dir\n\n%s\n\n%s"), sPath.c_str(), strerror(errno));
        return false;
    }

    if ((mkdir(tPath.c_str(), 0755) != 0) && errno != EEXIST) {
        InProgress::copyErrorsCounter++;
        Console::showMessage(ERROR_CONFIRM, _("Error creating folder\n\n%s\n\n%s"), tPath.c_str(), strerror(errno));
        closedir(dir);
        return false;
    }

    //dir_created:
    chmod(tPath.c_str(), 0755);
    struct dirent *data;

    errno = 0;
    while ((data = readdir(dir)) != nullptr) {

        if (strcmp(data->d_name, "..") == 0 || strcmp(data->d_name, ".") == 0)
            continue;

        std::string targetPath = StringUtils::stringFormat("%s/%s", tPath.c_str(), data->d_name);

        if ((data->d_type & DT_DIR) != 0) {
            if (!copyDir(sPath + StringUtils::stringFormat("/%s", data->d_name), targetPath, if_generate_FAT32_translation_file)) {
                if (InProgress::abortCopy) {
                    closedir(dir);
                    return false;
                }
                std::string errorMessage = StringUtils::stringFormat(_("Error copying directory - Backup/Restore is not reliable\nErrors so far: %d\nDo you want to continue?"), InProgress::copyErrorsCounter);
                if (!Console::promptConfirm((Style) (ST_YES_NO | ST_ERROR), errorMessage.c_str())) {
                    InProgress::abortCopy = true;
                    closedir(dir);
                    return false;
                }
            }
        } else {
            if (!copyFile(sPath + StringUtils::stringFormat("/%s", data->d_name), targetPath, if_generate_FAT32_translation_file)) {
                std::string errorMessage = StringUtils::stringFormat(_("Error copying file - Backup/Restore is not reliable\nErrors so far: %d\nDo you want to continue?"), InProgress::copyErrorsCounter);
                if (!Console::promptConfirm((Style) (ST_YES_NO | ST_ERROR), errorMessage.c_str())) {
                    InProgress::abortCopy = true;
                    closedir(dir);
                    return false;
                }
            }
        }
        errno = 0;
        if (InProgress::totalSteps > 1 || InProgress::immediateAbort) {
            InProgress::input->read();
            if (InProgress::input->get(ButtonState::HOLD, Button::L) && InProgress::input->get(ButtonState::HOLD, Button::MINUS)) {
                InProgress::abortTask = true;
                if (InProgress::immediateAbort) {
                    closedir(dir);
                    return false;
                }
            }
        }
    }

    if (errno != 0) {
        InProgress::copyErrorsCounter++;
        Console::showMessage(ERROR_CONFIRM, _("Error while parsing folder content\n\n%s\n\n%s\n\nCopy may be incomplete"), sPath.c_str(), strerror(errno));
        std::string errorMessage = StringUtils::stringFormat(_("Error copying directory - Backup/Restore is not reliable\nErrors so far: %d\nDo you want to continue?"), InProgress::copyErrorsCounter);
        if (!Console::promptConfirm((Style) (ST_YES_NO | ST_ERROR), errorMessage.c_str())) {
            InProgress::abortCopy = true;
            closedir(dir);
            return false;
        }
    }

    if (closedir(dir) != 0) {
        InProgress::copyErrorsCounter++;
        Console::showMessage(ERROR_CONFIRM, _("Error while closing folder\n\n%s\n\n%s"), sPath.c_str(), strerror(errno));
        std::string errorMessage = StringUtils::stringFormat(_("Error copying directory - Backup/Restore is not reliable\nErrors so far: %d\nDo you want to continue?"), InProgress::copyErrorsCounter);
        if (!Console::promptConfirm((Style) (ST_YES_NO | ST_ERROR), errorMessage.c_str())) {
            InProgress::abortCopy = true;
            return false;
        }
    }

    return (InProgress::copyErrorsCounter == 0);
}

bool FSUtils::removeDir(const std::string &pPath) {
    DIR *dir = opendir(pPath.c_str());
    if (dir == nullptr) {
        return false;
    }

    struct dirent *data;

    while ((data = readdir(dir)) != nullptr) {

        if (strcmp(data->d_name, "..") == 0 || strcmp(data->d_name, ".") == 0) continue;

        std::string tempPath = pPath + "/" + data->d_name;

        if (data->d_type & DT_DIR) {
            const std::string &origPath = tempPath;
            removeDir(tempPath);
            if (rmdir(origPath.c_str()) == -1) {
                Console::showMessage(ERROR_CONFIRM, _("Failed to delete folder\n\n%s\n\n%s"), origPath.c_str(), strerror(errno));
            }
        } else {
            if (unlink(tempPath.c_str()) == -1) {
                Console::showMessage(ERROR_CONFIRM, _("Failed to delete file\n\n%s\n\n%s"), tempPath.c_str(), strerror(errno));
            }
            if (InProgress::totalSteps > 1) {
                InProgress::input->read();
                if (InProgress::input->get(ButtonState::HOLD, Button::L) && InProgress::input->get(ButtonState::HOLD, Button::MINUS))
                    InProgress::abortTask = true;
            }
        }
    }

    if (closedir(dir) != 0) {
        Console::showMessage(ERROR_CONFIRM, _("Error while reading folder content\n\n%s\n\n%s"), pPath.c_str(), strerror(errno));
        return false;
    }
    return true;
}


bool FSUtils::folderEmpty(const char *fPath) {
    DIR *dir = opendir(fPath);
    if (dir == nullptr)
        return true; // if empty or non-existant, return true.

    bool empty = true;
    struct dirent *data;
    while ((data = readdir(dir)) != nullptr) {
        // rewritten to work wether ./.. are returned or not
        if (strcmp(data->d_name, ".") == 0 || strcmp(data->d_name, "..") == 0)
            continue;
        empty = false;
        break;
    }

    closedir(dir);
    return empty;
}


bool FSUtils::folderEmptyIgnoreSavemii(const char *fPath) {
    DIR *dir = opendir(fPath);
    if (dir == nullptr)
        return true; // if empty or non-existant, return true.

    bool empty = true;
    struct dirent *data;
    while ((data = readdir(dir)) != nullptr) {
        // rewritten to work wether ./.. are returned or not
        if (strcmp(data->d_name, ".") == 0 || strcmp(data->d_name, "..") == 0 || strcmp(data->d_name, "savemiiMeta.json") == 0)
            continue;
        empty = false;
        break;
    }

    closedir(dir);
    return empty;
}

bool FSUtils::createFolder(const char *path) {
    std::string strPath(path);
    size_t pos = 0;
    std::string vol_prefix("fs:/vol/");
    if (strPath.starts_with(vol_prefix))
        pos = strPath.find('/', vol_prefix.length());
    std::string dir;
    while ((pos = strPath.find('/', pos + 1)) != std::string::npos) {
        dir = strPath.substr(0, pos);
        if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
            Console::showMessage(ERROR_CONFIRM, _("Error while creating folder:\n\n%s\n\n%s"), dir.c_str(), strerror(errno));
            return false;
        }
        //FSAChangeMode(handle, FSUtils::newlibtoFSA(dir).c_str(), (FSMode) 0x666);
        //chmod(dir.c_str(),0666);

        //printf("chmod(%s,0666)\n",dir.c_str());
    }
    if (mkdir(strPath.c_str(), 0755) != 0 && errno != EEXIST) {
        Console::showMessage(ERROR_CONFIRM, _("Error while creating folder:\n\n%s\n\n%s"), dir.c_str(), strerror(errno));
        return false;
    }
    //FSAChangeMode(handle, FSUtils::newlibtoFSA(strPath).c_str(), (FSMode) 0x666);
    //chmod(strPath.c_str(),0666);
    //printf("chmod(%s,0666)\n",strPath.c_str());
    return true;
}

bool FSUtils::createFolderUnlocked(const std::string &path) {
    //std::string _path = FSUtils::newlibtoFSA(path);
    std::string _path = path;
    size_t pos = 0;
    std::string dir;
    while ((pos = _path.find('/', pos + 1)) != std::string::npos) {
        dir = _path.substr(0, pos);
        //if ((dir == "/vol") || (dir == "/vol/storage_mlc01" || (dir == FSUtils::newlibtoFSA(getUSB()))))
        //continue;
        //FSError createdDirStatus = FSAMakeDir(handle, dir.c_str(), (FSMode) 0x666);
        int createdDirStatus = mkdir(dir.c_str(), 0755);
        //printf("%s\n",dir.c_str());
        //if ((createdDirStatus != FS_ERROR_ALREADY_EXISTS) && (createdDirStatus != FS_ERROR_OK)) {
        if ((createdDirStatus != 0) && (errno != EEXIST)) {
            //Console::showMessage(ERROR_CONFIRM, _("Error while creating folder:\n\n%s\n\n%s"), dir.c_str(), FSAGetStatusStr(createdDirStatus));
            Console::showMessage(ERROR_CONFIRM, _("Error while creating folder:\n\n%s\n\n%s"), dir.c_str(), strerror(errno));
            return false;
        }
    }
    //FSError createdDirStatus = FSAMakeDir(handle, _path.c_str(), (FSMode) 0x666);
    int createdDirStatus = mkdir(_path.c_str(), 0755);
    //printf("%s\n",_path.c_str());
    //if ((createdDirStatus != FS_ERROR_ALREADY_EXISTS) && (createdDirStatus != FS_ERROR_OK)) {
    if ((createdDirStatus != 0) && (errno != EEXIST)) {
        //Console::showMessage(ERROR_CONFIRM, _("Error while creating final folder:\n\n%s\n\n%s"), dir.c_str(), FSAGetStatusStr(createdDirStatus));
        Console::showMessage(ERROR_CONFIRM, _("Error while creating final folder:\n\n%s\n\n%s"), dir.c_str(), strerror(errno));
        return false;
    }
    return true;
}


bool FSUtils::setOwnerAndMode([[maybe_unused]] uint32_t owner, [[maybe_unused]] uint32_t group, [[maybe_unused]] FSMode mode, [[maybe_unused]] std::string path, [[maybe_unused]] FSError &fserror) {

    printf("\n\nsetting %s perms to %08x:%08x %03x\n\n", path.c_str(), owner, group, mode);
    /*
    fserror = FSAChangeMode(handle, FSUtils::newlibtoFSA(path).c_str(), mode);
    if (fserror != FS_ERROR_OK) {
        Console::showMessage(ERROR_CONFIRM, _("Error\n%s\nsetting permissions for\n%s"), FSAGetStatusStr(fserror), path.c_str());
        return false;
    }

    fserror = FS_ERROR_OK;
    FSAShimBuffer *shim = (FSAShimBuffer *) memalign(0x40, sizeof(FSAShimBuffer));
    if (!shim) {
        Console::showMessage(ERROR_SHOW, _("Error creating shim for change perms\n\n%s"), path.c_str());
        return false;
    }

    shim->clientHandle = handle;
    shim->ipcReqType = FSA_IPC_REQUEST_IOCTL;
    strcpy(shim->request.changeOwner.path, FSUtils::newlibtoFSA(path).c_str());
    shim->request.changeOwner.owner = owner;
    shim->request.changeOwner.group = group;
    shim->command = FSA_COMMAND_CHANGE_OWNER;
    fserror = __FSAShimSend(shim, 0);
    free(shim);

    if (fserror != FS_ERROR_OK) {
        Console::showMessage(ERROR_CONFIRM, _("Error\n%s\nsetting owner/group for\n%s"), FSAGetStatusStr(fserror), path.c_str());
        return false;
    }
*/

    return true;
}


void FSUtils::flushVol([[maybe_unused]] const std::string &srcPath) {
    return;
}


bool FSUtils::setOwnerAndModeRec(uint32_t owner, uint32_t group, FSMode mode, std::string path, FSError &fserror) {

    setOwnerAndMode(owner, group, mode, path, fserror);

    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) {
        InProgress::copyErrorsCounter++;
        Console::showMessage(ERROR_CONFIRM, _("Error opening source dir\n\n%s\n\n%s"), path.c_str(), strerror(errno));
        return false;
    }

    struct dirent *data;

    errno = 0;
    while ((data = readdir(dir)) != nullptr) {

        if (strcmp(data->d_name, "..") == 0 || strcmp(data->d_name, ".") == 0)
            continue;
        std::string tPath = path + StringUtils::stringFormat("/%s", data->d_name);

        if ((data->d_type & DT_DIR) != 0) {

            if (!setOwnerAndMode(owner, group, mode, tPath, fserror)) {
                if (InProgress::abortCopy) {
                    closedir(dir);
                    return false;
                }
            }
        } else {
            if (!setOwnerAndMode(owner, group, mode, tPath, fserror)) {
                std::string errorMessage = StringUtils::stringFormat(_("Error setting permissions - Restore is not reliable\nErrors so far: %d\nDo you want to continue?"), InProgress::copyErrorsCounter);
                if (!Console::promptConfirm((Style) (ST_YES_NO | ST_ERROR), errorMessage.c_str())) {
                    InProgress::abortCopy = true;
                    closedir(dir);
                    return false;
                }
            }
        }
        errno = 0;
        if (InProgress::totalSteps > 1 || InProgress::immediateAbort) {
            InProgress::input->read();
            if (InProgress::input->get(ButtonState::HOLD, Button::L) && InProgress::input->get(ButtonState::HOLD, Button::MINUS)) {
                InProgress::abortTask = true;
                if (InProgress::immediateAbort)
                    return false;
            }
        }
    }

    if (errno != 0) {
        InProgress::copyErrorsCounter++;
        Console::showMessage(ERROR_CONFIRM, _("Error while parsing folder content\n\n%s\n\n%s\n\nCopy may be incomplete"), path.c_str(), strerror(errno));
        std::string errorMessage = StringUtils::stringFormat(_("Error setting permissions - Restore is not reliable\nErrors so far: %d\nDo you want to continue?"), InProgress::copyErrorsCounter);
        if (!Console::promptConfirm((Style) (ST_YES_NO | ST_ERROR), errorMessage.c_str())) {
            InProgress::abortCopy = true;
            closedir(dir);
            return false;
        }
    }

    if (closedir(dir) != 0) {
        InProgress::copyErrorsCounter++;
        Console::showMessage(ERROR_CONFIRM, _("Error while closing folder\n\n%s\n\n%s"), path.c_str(), strerror(errno));
        std::string errorMessage = StringUtils::stringFormat(_("Error setting permissions - Restore is not reliable\nErrors so far: %d\nDo you want to continue?"), InProgress::copyErrorsCounter);
        if (!Console::promptConfirm((Style) (ST_YES_NO | ST_ERROR), errorMessage.c_str())) {
            InProgress::abortCopy = true;
            return false;
        }
    }

    return (InProgress::copyErrorsCounter == 0);
}


int FSUtils::slc_resilient_rename(std::string &src_path, std::string &dst_path) {
    int res;

    if (src_path.rfind("storage_slcc01:", 0) == 0) {
        res = FSUtils::copyFile(src_path.c_str(), dst_path.c_str()) ? 0 : 1;
        if (res == 0)
            unlink(src_path.c_str());
    } else {
        res = rename(src_path.c_str(), dst_path.c_str());
    }

    return res;
}