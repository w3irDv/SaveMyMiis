#include <ApplicationState.h>
#include <memory>
#include <menu/MiiProcessSharedState.h>
#include <menu/MiiRepoSelectState.h>
#include <menu/MiiSelectState.h>
#include <menu/MiiTasksState.h>
#include <mii/Mii.h>
#include <mii/MiiStadioSav.h>

#include <mii/WiiUMii.h>
#include <utils/ConsoleUtils.h>
#include <utils/FSUtils.h>
#include <utils/InProgress.h>

#include <map>

#include <miisavemng.h>
#include <utils/MiiUtils.h>

#include <menu/MiiDBOptionsState.h>
#include <menu/MiiTransformTasksState.h>
#include <sys/stat.h>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <mii/MiiAccountRepo.h>
#include <mii/MiiFileRepo.h>
#include <mii/MiiFolderRepo.h>
#include <mii/WiiMii.h>
#include <mii/WiiUMii.h>

#include <utils/StringUtils.h>

#include <Metadata.h>

#include <coreinit/_time.h>

#define TEST_FFL
//#define TEST_RFL
//#define TEST_ACCOUNT

typedef std::map<std::string, std::string> kvConfig;
kvConfig pathConfig;
kvConfig ambientConfig;

void readPathConfig() {

    std::string line;
    std::ifstream infile("config.in");
    while (std::getline(infile, line)) {
        std::istringstream is_line(line);
        std::string key;
        if (std::getline(is_line, key, '=')) {
            std::string value;
            if (key[0] == '#')
                continue;
            if (std::getline(is_line, value)) {
                pathConfig[key] = value;
            }
        }
    }
}

bool readAmbientConfig() {

    std::string line;
    std::ifstream infile("ambient.in");
    while (std::getline(infile, line)) {
        std::istringstream is_line(line);
        std::string key;
        if (std::getline(is_line, key, '=')) {
            std::string value;
            if (key[0] == '#')
                continue;
            if (std::getline(is_line, value)) {
                ambientConfig[key] = value;
            }
        }
    }

    return true;
}

void view_miidata(MiiData *miidata, size_t off1, size_t off2) {
    std::string hex_ascii{};
    for (size_t i = 0; i < miidata->mii_data_size; i++) {
        if (i == off1 || i == off2)
            hex_ascii.append(" ");
        char hexhex[3];
        snprintf(hexhex, 3, "%02x", miidata->mii_data[i]);
        hex_ascii.append(hexhex);
    }
    printf("%s\n", hex_ascii.c_str());
}

void view_all_miidata(MiiRepo *mii_repo, size_t off1, size_t off2) {

    for (size_t j = 0; j < mii_repo->miis.size(); j++) {
        if (!mii_repo->miis.at(j)->is_valid) {
            printf("INVALID - %s\n", mii_repo->miis.at(j)->mii_name.c_str());
            continue;
        }
        std::string hex_ascii{};
        MiiData *miidata = mii_repo->extract_mii_data(j);
        for (size_t i = 0; i < miidata->mii_data_size; i++) {
            if (i == off1 || i == off2)
                hex_ascii.append(" ");
            char hexhex[3];
            snprintf(hexhex, 3, "%02x", miidata->mii_data[i]);
            hex_ascii.append(hexhex);
        }
        printf("%s\n", mii_repo->miis[j]->mii_name.c_str());
        printf("%s\n", hex_ascii.c_str());
        delete miidata;
    }
}

void view_crc(MiiRepo &miirepo) {
    if (miirepo.db_kind == MiiRepo::FILE)
        printf("crc is %04x\n", __builtin_bswap16(miirepo.get_crc()));
}

#define folder 1
#define file   2

const uint8_t repo_type = folder;

MiiRepoSelectState *_menu;
MiiProcessSharedState *_mii_process_shared_state;

MiiRepoSelectState *initRepoSelectMenu(MiiProcessSharedState *mii_process_shared_state) {
    std::vector<bool> mii_repos_candidates;

    for (size_t i = 0; i < MiiUtils::mii_repos.size(); i++)
        mii_repos_candidates.push_back(true);

    // start menu: respo select
    auto menu = new MiiRepoSelectState(mii_repos_candidates, MiiProcess::SELECT_SOURCE_REPO, mii_process_shared_state);
    return menu;
}

bool test_initMiiRepos() {

    if (readAmbientConfig()) {
        AmbientConfig::author_id = std::stoul(ambientConfig["author_id"].c_str(), nullptr, 16);

        uint64_t mac_address = __builtin_bswap64(std::stoul(ambientConfig["mac_address"].c_str(), nullptr, 16));
        memcpy(&AmbientConfig::mac_address.MACAddr, (uint8_t *) &mac_address + 2, 6);

        uint64_t device_hash = __builtin_bswap64(std::stoul(ambientConfig["device_hash"].c_str(), nullptr, 16));
        memcpy(&AmbientConfig::device_hash, (char *) &device_hash, 8);

        AmbientConfig::thisConsoleSerialId = ambientConfig["serial_id"];
    }


    readPathConfig();

    std::string path_to_ffl = pathConfig["path_ffl"].substr(0,pathConfig["path_ffl"].find_last_of("/")); 

    if (FSUtils::checkEntry(path_to_ffl.c_str()) == 0)
        FSUtils::createFolder(path_to_ffl.c_str());
    
    if (FSUtils::checkEntry(pathConfig["path_ffl_Stage"].c_str()) == 0)
        FSUtils::createFolder(pathConfig["path_ffl_Stage"].c_str());

    std::string path_to_rfl = pathConfig["path_rfl"].substr(0,pathConfig["path_rfl"].find_last_of("/"));
    FSUtils::createFolder(path_to_rfl.c_str());

    if (FSUtils::checkEntry(pathConfig["path_rfl_Stage"].c_str()) == 0)
        FSUtils::createFolder(pathConfig["path_rfl_Stage"].c_str());

    if (FSUtils::checkEntry(pathConfig["path_account"].c_str()) == 0)
        FSUtils::createFolder(pathConfig["path_account"].c_str());

    if (FSUtils::checkEntry(pathConfig["path_account_Stage"].c_str()) == 0)
        FSUtils::createFolder(pathConfig["path_account_Stage"].c_str());

    MiiUtils::MiiRepos["FFL"] = new MiiFileRepo<WiiUMii, WiiUMiiData>("FFL", pathConfig["path_ffl"], "mii_bckp_ffl", "Wii U Mii Database", MiiRepo::INTERNAL);
    MiiUtils::MiiRepos["FFL_STAGE"] = new MiiFolderRepo<WiiUMii, WiiUMiiData>("FFL_STAGE", pathConfig["path_ffl_Stage"], "mii_bckp_ffl_Stage", "Stage Folder for Wii U Miis", MiiRepo::SD);
    MiiUtils::MiiRepos["RFL"] = new MiiFileRepo<WiiMii, WiiMiiData>("RFL", pathConfig["path_rfl"], "mii_bckp_rfl", "vWii Mii Database", MiiRepo::INTERNAL);
    MiiUtils::MiiRepos["RFL_STAGE"] = new MiiFolderRepo<WiiMii, WiiMiiData>("RFL_STAGE", pathConfig["path_rfl_Stage"], "mii_bckp_rfl_Stage", "Stage Folder for vWii Miis", MiiRepo::SD);
    MiiUtils::MiiRepos["ACCOUNT"] = new MiiAccountRepo<WiiUMii, WiiUMiiData>("ACCOUNT", pathConfig["path_account"], "mii_bckp_account", "Miis from Account DB", MiiRepo::INTERNAL);
    MiiUtils::MiiRepos["ACCOUNT_STAGE"] = new MiiFolderRepo<WiiUMii, WiiUMiiData>("ACCOUNT_STAGE", pathConfig["path_account_Stage"], "mii_bckp_account_Stage", "Stage folder for Account Miis", MiiRepo::SD);

    // STADIO
    MiiUtils::MiiStadios["FFL_ACCOUNT"] = new MiiStadioSav("FFL_ACCOUNT", pathConfig["path_stadio"], "mii_bckp_ffl", "Internal Stadio save DB");

    MiiUtils::MiiRepos["FFL"]->setStageRepo(MiiUtils::MiiRepos["FFL_STAGE"]);
    //MiiUtils::MiiRepos["FFL_STAGE"]->setStageRepo(MiiUtils::MiiRepos["FFL"]);

    MiiUtils::MiiRepos["RFL"]->setStageRepo(MiiUtils::MiiRepos["RFL_STAGE"]);
    //MiiUtils::MiiRepos["RFL_STAGE"]->setStageRepo(MiiUtils::MiiRepos["RFL"]);

    MiiUtils::MiiRepos["ACCOUNT"]->setStageRepo(MiiUtils::MiiRepos["ACCOUNT_STAGE"]);
    //MiiUtils::MiiRepos["ACCOUNT_STAGE"]->setStageRepo(MiiUtils::MiiRepos["ACCOUNT"]);

    // STADIO
    MiiUtils::MiiRepos["FFL"]->setStadioSav(MiiUtils::MiiStadios["FFL_ACCOUNT"]);
    MiiUtils::MiiRepos["ACCOUNT"]->setStadioSav(MiiUtils::MiiStadios["FFL_ACCOUNT"]);
    MiiUtils::MiiStadios["FFL_ACCOUNT"]->setAccountRepo(MiiUtils::MiiRepos["ACCOUNT"]);

#ifdef TEST_RFL
    MiiUtils::MiiRepos["REPO1"] = MiiUtils::MiiRepos["RFL"];
    MiiUtils::MiiRepos["REPO1_STAGE"] = MiiUtils::MiiRepos["RFL_STAGE"];
#endif
#ifdef TEST_ACCOUNT
    MiiUtils::MiiRepos["REPO1"] = MiiUtils::MiiRepos["ACCOUNT"];
    MiiUtils::MiiRepos["REPO1_STAGE"] = MiiUtils::MiiRepos["ACCOUNT_STAGE"];
#endif
#ifdef TEST_FFL
    MiiUtils::MiiRepos["REPO1"] = MiiUtils::MiiRepos["FFL"];
    MiiUtils::MiiRepos["REPO1_STAGE"] = MiiUtils::MiiRepos["FFL_STAGE"];
#endif

    MiiUtils::mii_repos = {MiiUtils::MiiRepos["REPO1"], MiiUtils::MiiRepos["REPO1_STAGE"]};

    MiiUtils::mii_repos = {
            MiiUtils::MiiRepos["FFL"],
            MiiUtils::MiiRepos["FFL_STAGE"],
            MiiUtils::MiiRepos["RFL"],
            MiiUtils::MiiRepos["RFL_STAGE"],
            MiiUtils::MiiRepos["ACCOUNT"],
            MiiUtils::MiiRepos["ACCOUNT_STAGE"]
    };

    InProgress::input = new Input();

    _mii_process_shared_state = new MiiProcessSharedState();
    _menu = initRepoSelectMenu(_mii_process_shared_state);


    if (FSUtils::checkEntry(MiiUtils::MiiRepos["RFL"]->path_to_repo.c_str()) != 1)
        MiiUtils::ask_if_to_initialize_db(MiiUtils::MiiRepos["RFL"], DB_NOT_FOUND);
    if (FSUtils::checkEntry(MiiUtils::MiiRepos["FFL"]->path_to_repo.c_str()) != 1)
        MiiUtils::ask_if_to_initialize_db(MiiUtils::MiiRepos["FFL"], DB_NOT_FOUND);

    return true;
}

void test_deinitMiiRepos() {

    delete _menu;
    delete _mii_process_shared_state;

    MiiUtils::deinitMiiRepos();

    delete InProgress::input;
    FSUtils::deinit_fs_buffers();
}

void test_test_populate_and_free_repo() {


    auto wiiu_folder_repo = MiiUtils::MiiRepos["REPO1_STAGE"];

    wiiu_folder_repo->open_and_load_repo();
    wiiu_folder_repo->populate_repo();

}


void test_test_list_repo() {


    auto wiiu_folder_repo = MiiUtils::MiiRepos["REPO1"];

    wiiu_folder_repo->open_and_load_repo();
    wiiu_folder_repo->populate_repo();

    printf("repo\n");
    wiiu_folder_repo->test_list_repo();
}


void test_list_mii_repo() {


    auto wii_folder_repo = MiiUtils::MiiRepos["RFL"];

    wii_folder_repo->populate_repo();

    printf("repo\n");
    wii_folder_repo->test_list_repo();
}


void test_extract_and_import_mii() {

    auto wiiu_folder_repo2 = MiiUtils::MiiRepos["REPO1"];      //importem
    auto wiiu_folder_repo = MiiUtils::MiiRepos["REPO1_STAGE"]; //extratc

    wiiu_folder_repo->open_and_load_repo();
    wiiu_folder_repo2->open_and_load_repo();

    wiiu_folder_repo->populate_repo();
    wiiu_folder_repo2->populate_repo();

    printf("%s\n", wiiu_folder_repo->path_to_repo.c_str());
    wiiu_folder_repo->test_list_repo();

    printf("%s\n", wiiu_folder_repo2->path_to_repo.c_str());
    wiiu_folder_repo2->test_list_repo();

    printf("extract one mi from %s\n", wiiu_folder_repo->path_to_repo.c_str());
    MiiData *miidata = wiiu_folder_repo->extract_mii_data(0);

    if (miidata == nullptr) {
        printf("errror extracting data");
        return;
    }

    printf("view\n");
    view_miidata(miidata, 0, 0);

    printf("\n\nlist repo before we importt\n\n");
    wiiu_folder_repo2->test_list_repo();

    printf("\nimpors mii\n");
    wiiu_folder_repo2->import_miidata(miidata, ADD_MII, IN_EMPTY_LOCATION);

    wiiu_folder_repo2->persist_repo();

    delete miidata;

    printf("\n\nlist repo where we importt\n\n");
    wiiu_folder_repo2->populate_repo();
    wiiu_folder_repo2->test_list_repo();

    wiiu_folder_repo->empty_repo();
    wiiu_folder_repo2->empty_repo();
}


void test_persist_first_repo() {

    auto repo = MiiUtils::mii_repos.at(0);

    repo->open_and_load_repo();
    repo->populate_repo();
    repo->persist_repo();
}


void test_persist_FFL_repo() {

    auto repo = MiiUtils::MiiRepos["FFL"];

    repo->open_and_load_repo();
    repo->populate_repo();
    repo->persist_repo();
}


void test_persist_RFL_repo() {

    auto repo = MiiUtils::MiiRepos["RFL"];

    repo->open_and_load_repo();
    repo->populate_repo();
    repo->persist_repo();
}


void test_get_backup_path() {


    printf("backup path from local map using getMii - %s\n", MiiSaveMng::getMiiRepoBackupPath(MiiUtils::MiiRepos["REPO1"], 4).c_str());
}

void test_miiutils_create_repo_get() {

    printf("backup path from Miitils map using getMii %s\n", MiiSaveMng::getMiiRepoBackupPath(MiiUtils::MiiRepos["REPO1"], 1).c_str());
    printf("backup path from Miitils map using getMii %s\n", MiiSaveMng::getMiiRepoBackupPath(MiiUtils::MiiRepos["RFL"], 1).c_str());
    printf("direct pathrepo field from global : %s\n", MiiUtils::MiiRepos["RFL"]->path_to_repo.c_str());
    printf("direct backuppath field from global : %s\n", MiiUtils::MiiRepos["RFL"]->backup_base_path.c_str());
}

void test_mii_db_options_menu() {


    auto mii_repo = MiiUtils::MiiRepos["REPO1"];


    MiiProcessSharedState mii_process_shared_state;
    mii_process_shared_state.primary_mii_repo = mii_repo;

    auto miidboptions = std::make_unique<MiiDBOptionsState>(mii_repo, MiiProcess::BACKUP_DB, &mii_process_shared_state);
    miidboptions->render();
}


void test_backup() {


    printf("backup path - %s\n", MiiSaveMng::getMiiRepoBackupPath(MiiUtils::MiiRepos["REPO1"], 1).c_str());

    MiiUtils::MiiRepos["REPO1"]->backup(1);

    printf("backup path tagged - %s\n", MiiSaveMng::getMiiRepoBackupPath(MiiUtils::MiiRepos["REPO1"], 0).c_str());

    MiiUtils::MiiRepos["REPO1"]->backup(0, "tag me up");

    auto mii_repo = MiiUtils::MiiRepos["REPO1"];

    MiiProcessSharedState mii_process_shared_state;
    mii_process_shared_state.primary_mii_repo = mii_repo;

    auto miidboptions = std::make_unique<MiiDBOptionsState>(mii_repo, MiiProcess::BACKUP_DB, &mii_process_shared_state);
    miidboptions->render();
}


void test_wipe() {


    auto mii_repo = MiiUtils::MiiRepos["REPO1"];

    printf("\n\nbefor wipe\n\n");

    MiiProcessSharedState mii_process_shared_state;
    mii_process_shared_state.primary_mii_repo = mii_repo;

    auto miidboptions = std::make_unique<MiiDBOptionsState>(mii_repo, MiiProcess::BACKUP_DB, &mii_process_shared_state);
    miidboptions->render();

    mii_repo->wipe();

    printf("\n\nafter wipe\n\n");

    //    auto miidboptions = std::make_unique<MiiDBOptionsState>(mii_repo, MiiProcess::BACKUP_DB);
    miidboptions = std::make_unique<MiiDBOptionsState>(mii_repo, MiiProcess::BACKUP_DB, &mii_process_shared_state);
    miidboptions->render();
}


void test_restore() {

    auto mii_repo = MiiUtils::MiiRepos["REPO1"];
    MiiProcessSharedState mii_process_shared_state;

    auto miidboptions = std::make_unique<MiiDBOptionsState>(mii_repo, MiiProcess::RESTORE_DB, &mii_process_shared_state);
    printf("\n\nbefore restore\n\n");
    miidboptions->render();


    mii_repo->restore(0);

    mii_process_shared_state.primary_mii_repo = mii_repo;

    printf("\n\nafter restore\n\n");
    miidboptions = std::make_unique<MiiDBOptionsState>(mii_repo, MiiProcess::RESTORE_DB, &mii_process_shared_state);
    miidboptions->render();
}

void test_is_slot_empty() {


    printf("backup path: - %s\n", MiiSaveMng::getMiiRepoBackupPath(MiiUtils::MiiRepos["REPO1"], 0).c_str());
    printf("is empty slot 0: %s\n", MiiSaveMng::isSlotEmpty(MiiUtils::MiiRepos["REPO1"], 0) ? "si" : "no");

    printf("is empty slot 1: %s\n", MiiSaveMng::isSlotEmpty(MiiUtils::MiiRepos["REPO1"], 1) ? "si" : "no");
    printf("is empty  slot 2: %s\n", MiiSaveMng::isSlotEmpty(MiiUtils::MiiRepos["REPO1"], 2) ? "si" : "no");
    printf("backup path: - %s\n", MiiSaveMng::getMiiRepoBackupPath(MiiUtils::MiiRepos["REPO1"], 5).c_str());
    printf("is empty slot 5: %s\n", MiiSaveMng::isSlotEmpty(MiiUtils::MiiRepos["REPO1"], 5) ? "si" : "no");
}

void test_get_empty_slot() {


    uint8_t slot = MiiSaveMng::getEmptySlot(MiiUtils::MiiRepos["REPO1"]);
    printf("free slot: %d\n", slot);
    printf("backup path: - %s\n", MiiSaveMng::getMiiRepoBackupPath(MiiUtils::MiiRepos["REPO1"], slot).c_str());
    printf("is esmpty %s\n", MiiSaveMng::isSlotEmpty(MiiUtils::MiiRepos["REPO1"], slot) ? "si" : "no");
}


void test_mii_repo_menu() {


    MiiProcessSharedState mii_process_shared_state;
    std::vector<bool> mii_repos_candidates;

    for (size_t i = 0; i < MiiUtils::mii_repos.size(); i++)
        mii_repos_candidates.push_back(true);

    auto repo_select = std::make_unique<MiiRepoSelectState>(mii_repos_candidates, MiiProcess::SELECT_SOURCE_REPO, &mii_process_shared_state);
    repo_select->render();
}

void test_mii_tasks_menu() {


    MiiProcessSharedState mii_process_shared_state;
    std::vector<bool> mii_repos_candidates;

    for (size_t i = 0; i < MiiUtils::mii_repos.size(); i++)
        mii_repos_candidates.push_back(true);

    mii_process_shared_state.primary_mii_repo = MiiUtils::MiiRepos["REPO1"];

    auto mii_repo = MiiUtils::MiiRepos["REPO1"];

    auto task_select = std::make_unique<MiiTasksState>(mii_repo, MiiProcess::SELECT_TASK, &mii_process_shared_state);
    task_select->render();

    mii_process_shared_state.primary_mii_repo = MiiUtils::MiiRepos["ACCOUNT"];

    task_select = std::make_unique<MiiTasksState>(MiiUtils::MiiRepos["ACCOUNT"], MiiProcess::SELECT_TASK, &mii_process_shared_state);
    task_select->render();
}


void test_mii_transform_tasks_menu() {


    MiiProcessSharedState mii_process_shared_state;
    std::vector<bool> mii_repos_candidates;

    for (size_t i = 0; i < MiiUtils::mii_repos.size(); i++)
        mii_repos_candidates.push_back(true);

    mii_process_shared_state.primary_mii_repo = MiiUtils::MiiRepos["REPO1"];

    auto task_select = std::make_unique<MiiTransformTasksState>(MiiUtils::MiiRepos["REPO1"], MiiProcess::SELECT_TRANSFORM_TASK, &mii_process_shared_state);
    task_select->render();

    mii_process_shared_state.primary_mii_repo = MiiUtils::MiiRepos["ACCOUNT"];

    task_select = std::make_unique<MiiTransformTasksState>(MiiUtils::MiiRepos["ACCOUNT"], MiiProcess::SELECT_TRANSFORM_TASK, &mii_process_shared_state);
    task_select->render();
}

void test_populate_repo() {


    printf("\n\nbefore populate\n\n");
    MiiUtils::MiiRepos["REPO1"]->test_list_repo();
    MiiUtils::MiiRepos["REPO1"]->populate_repo();
    printf("\n\nafter populate\n\n");
    MiiUtils::MiiRepos["REPO1"]->test_list_repo();
    MiiUtils::MiiRepos["REPO1"]->empty_repo();
}

void test_get_crc() {

    auto mii_repo = MiiUtils::MiiRepos["REPO1"];
    mii_repo->open_and_load_repo();
    printf("repo_name: %s\n", mii_repo->repo_name.c_str());
    printf("repo_path: %s\n", mii_repo->path_to_repo.c_str());
    printf("crc is %04x\n", __builtin_bswap16(mii_repo->get_crc()));
}

void test_get_crc_after_write() {

    auto mii_repo = MiiUtils::MiiRepos["REPO1"];
    mii_repo->open_and_load_repo();
    printf("repo_name: %s\n", mii_repo->repo_name.c_str());
    printf("repo_path: %s\n", mii_repo->path_to_repo.c_str());
    printf("original crc is %04x\n", mii_repo->get_crc());

    mii_repo->persist_repo();
    mii_repo->open_and_load_repo();
    printf("after persist crc is %04x\n", mii_repo->get_crc());
}


///////////////////////////////////////////////////////////////////////////////////////
///// REPO SELECT
void test_menu_selectSameRepo_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {

    Input input;

    menu->render();

    input.press('a');
    menu->update(&input);
}


void test_menu_selectRepoAccount_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    menu->render();

    Input input;

    // select Account repo

    //for (int i = 0; i < 6; i++) { // to be sure we are on te fist re`p
    //    input.press('u');
    //    menu->update(&input);
    //}

    // now we go down
    for (int i = 0; i < 4; i++) {
        input.press('d');
        menu->update(&input);
    }

    // to task menu
    input.press('a');
    menu->update(&input);
}

void test_menu_selectRepoAccountAux_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    menu->render();

    Input input;

    // select Account repo

    //for (int i = 0; i < 6; i++) { // to be sure we are on te fist re`p
    //    input.press('u');
    //    menu->update(&input);
    //}

    // now we go down to select FFL_S
    menu->update(&input);
    input.press('d');
    menu->update(&input);


    // to task menu
    input.press('a');
    menu->update(&input);
}


void test_menu_selectRepoAccountStage_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {

    menu->render();

    Input input;

    // select Account Stage  repo

    for (int i = 0; i < 5; i++) {
        input.press('d');
        menu->update(&input);
    }

    // to task menu
    input.press('a');
    menu->update(&input);
}

void test_menu_selectRepoRFL_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {

    menu->render();

    Input input;

    // select RFL repo
    input.press('d');
    menu->update(&input);
    input.press('d');
    menu->update(&input);
    // to task menu
    input.press('a');
    menu->update(&input);
}

void test_menu_selectRepoRFLStage_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {

    menu->render();

    Input input;

    // select RFL Stage repo
    for (int i = 0; i < 3; i++) {
        input.press('d');
        menu->update(&input);
    }

    // to task menu
    input.press('a');
    menu->update(&input);
}

void test_menu_selectRepoRFLStage_from_RFL_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {

    menu->render();

    Input input;

    // select RFL Stage repo when previously we have selected RFL
    input.press('d');
    menu->update(&input);

    // to task menu
    input.press('a');
    menu->update(&input);
}

void test_menu_selectRepoFFL_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {

    menu->render();

    Input input;

    // select FFL repo
    // to task menu
    input.press('a');
    menu->update(&input);
}

void test_menu_selectRepoFFLStage_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {

    menu->render();

    Input input;

    // select FFL Stage repo
    input.press('d');
    menu->update(&input);
    // to task menu
    input.press('a');
    menu->update(&input);
}

void test_menu_selectPreviousRepo_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {

    menu->render();

    Input input;

    // up one repo
    input.press('u');
    menu->update(&input);
    // to task menu
    input.press('a');
    menu->update(&input);
}


void test_menu_selectRepoFirstCompatible_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {

    menu->render();

    Input input;

    // to next task
    input.press('a');
    menu->update(&input);
}

/// TASK SELECT

void test_menu_selectListTask_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();

    Input input;

    // select list task
    // to list
    menu->render();

    input.press('d');
    menu->update(&input);
    input.press('d');
    menu->update(&input);
    input.press('d');
    menu->update(&input);
    if (mii_process_shared_state->primary_mii_repo->db_kind != MiiRepo::eDBKind::ACCOUNT) {
        input.press('d');
        menu->update(&input);
    }

    input.press('a');
    menu->update(&input);
    menu->render();
}


void test_menu_selectListTask_Views_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();

    Input input;

    // select list task
    // to list
    menu->render();
    input.press('d');
    menu->update(&input);
    input.press('d');
    menu->update(&input);
    input.press('d');
    menu->update(&input);
    if (mii_process_shared_state->primary_mii_repo->db_kind != MiiRepo::eDBKind::ACCOUNT) {
        input.press('d');
        menu->update(&input);
    }
    input.press('a');
    menu->update(&input);
    menu->render();

    printf("\n SYSTEM_DEVICE \n");
    // set SYSTEM_DEVICE view
    input.press('x');
    menu->update(&input);
    menu->render();

    printf("\n mii id \n");
    // set LOCATION view
    input.press('x');
    menu->update(&input);
    menu->render();


    printf("\n location \n");
    // set LOCATION view
    input.press('x');
    menu->update(&input);
    menu->render();

    printf("\n creators \n");
    // set LOCATION view
    input.press('x');
    menu->update(&input);
    menu->render();

    printf("\n timestamp \n");
    // set LOCATION view
    input.press('x');
    menu->update(&input);
    menu->render();
}


void test_menu_selectExportTask_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();
    Input input;

    // select Export task
    for (int i=0;i<4;i++) {
        input.press('d');
        menu->update(&input);
    }
    
    if (mii_process_shared_state->primary_mii_repo->db_kind != MiiRepo::eDBKind::ACCOUNT) {
        printf("moving more...\n");
        input.press('d');
        menu->update(&input);

        // to  Export menu
    }

    input.press('a');
    menu->update(&input);
}

void test_menu_selectTransformTasks_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();

    Input input;

    // select transform task
    for (int i=0;i<6;i++) {
        input.press('d');
        menu->update(&input);
    }

    if (mii_process_shared_state->primary_mii_repo->db_kind != MiiRepo::eDBKind::ACCOUNT) {
        input.press('d');
        menu->update(&input);
        input.press('d');
        menu->update(&input);
    }
    // to  select miis to trasnform list
    input.press('a');
    menu->update(&input);
}

void test_menu_selectImportTask_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();

    Input input;

    // select import task
    for (int i=0;i<5;i++) {
        input.press('d');
        menu->update(&input);
    }
    if (mii_process_shared_state->primary_mii_repo->db_kind != MiiRepo::eDBKind::ACCOUNT) {
        input.press('d');
        menu->update(&input);
    }
    // to  select miis to export list
    input.press('a');
    menu->update(&input);
}

void test_menu_selectWipeMiiTask_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();

    Input input;

    // select wipe task
    for (int i=0;i<5;i++) {
        input.press('d');
        menu->update(&input);
    }
    menu->update(&input);
    if (mii_process_shared_state->primary_mii_repo->db_kind != MiiRepo::eDBKind::ACCOUNT) {
        input.press('d');
        menu->update(&input);
        input.press('d');
        menu->update(&input);
    }
    // to  select miis to wipe list
    input.press('a');
    menu->update(&input);
}

void test_menu_selectBackupTask_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();

    Input input;

    // select backup task
    // to  select miis to trasnform list
    input.press('a');
    menu->update(&input);

    // select slot 2
    //input.press('r');
    //menu->update(&input);
    //input.press('r');
    //menu->update(&input);

    // slot 0 is used

    input.press('a');
    menu->update(&input);
    menu->render();
}

void test_menu_selectRestoreTask_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();

    Input input;

    // select restore task
    input.press('d');
    menu->update(&input);

    // to  select slot
    input.press('a');
    menu->update(&input);

    menu->render();

    // select slot 2
    //input.press('r');
    //menu->update(&input);
    //input.press('r');
    //menu->update(&input);

    //slot 0 is used

    menu->render();

    input.press('a');
    menu->update(&input);
}

void test_menu_selectWipeTask_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();

    Input input;

    // select wipe task
    input.press('d');
    menu->update(&input);

    input.press('d');
    menu->update(&input);

    // to  wipe task ; then select slot
    input.press('a');
    menu->update(&input);

    menu->render();

    // select slot
    input.press('r');
    menu->update(&input);

    input.press('r');
    menu->update(&input);

    menu->render();

    input.press('a');
    menu->update(&input);
}


void test_menu_selectXRestoreTask_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();

    Input input;

    // select xrestore task
    input.press('d');
    menu->update(&input);

    input.press('d');
    menu->update(&input);

    // to  select ; then select source mii
    input.press('a');
    menu->update(&input);
}


void test_menu_selectSlotTask_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();

    Input input;

    // select slot
    input.press('r');
    menu->update(&input);

    input.press('r');
    menu->update(&input);

    menu->render();

    input.press('a');
    menu->update(&input);
}


void test_menu_selectInitializeTask_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();

    Input input;

    // to restore
    input.press('d');
    menu->update(&input);

    // to  wipe task
    input.press('d');
    menu->update(&input);

    // to  initialize task
    input.press('d');
    menu->update(&input);
    input.press('a');
    menu->update(&input);

    menu->render();

    // Initialize
    input.press('a');
    menu->update(&input);

    menu->render();
}

// TOGGLE TASKS IN TRANSFORM

void test_menu_toggleCopyFlag_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();
    Input input;

    size_t cursorPos = 9;
    for (size_t i = 0; i < cursorPos; i++) {
        input.press('d');
        menu->update(&input);
    }

    input.press('l');
    menu->update(&input);
    menu->render();

    // do it
    input.press('a');
    menu->update(&input);
}

void test_menu_toggleFavoriteFlag_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();
    Input input;

    size_t cursorPos = 4;
    for (size_t i = 0; i < cursorPos; i++) {
        input.press('d');
        menu->update(&input);
    }

    input.press('l');
    menu->update(&input);
    menu->render();

    // do it
    input.press('a');
    menu->update(&input);
}

void test_menu_toggleShareFlag_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();
    Input input;

    size_t cursorPos = 5;
    for (size_t i = 0; i < cursorPos; i++) {
        input.press('d');
        menu->update(&input);
    }

    input.press('l');
    menu->update(&input);
    menu->render();

    // do it
    input.press('a');
    menu->update(&input);
}

void test_menu_toggleNormalSpecialFlag_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();
    Input input;

    size_t cursorPos = 6;
    for (size_t i = 0; i < cursorPos; i++) {
        input.press('d');
        menu->update(&input);
    }

    input.press('l');
    menu->update(&input);
    menu->render();

    // do it
    input.press('a');
    menu->update(&input);
}

void test_menu_toggleForeignFlag_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();
    Input input;

    size_t cursorPos = 7;
    for (size_t i = 0; i < cursorPos; i++) {
        input.press('d');
        menu->update(&input);
    }

    input.press('l');
    menu->update(&input);
    menu->render();

    // do it
    input.press('a');
    menu->update(&input);
}

void test_menu_toggleTempFlag_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();
    Input input;

    size_t cursorPos = 10;
    for (size_t i = 0; i < cursorPos; i++) {
        input.press('d');
        menu->update(&input);
    }

    input.press('l');
    menu->update(&input);
    menu->render();

    // do it
    input.press('a');
    menu->update(&input);
}

void test_menu_updateCRC_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();
    Input input;

    size_t cursorPos = 8;
    for (size_t i = 0; i < cursorPos; i++) {
        input.press('d');
        menu->update(&input);
    }

    input.press('l');
    menu->update(&input);
    menu->render();

    // do it
    input.press('a');
    menu->update(&input);
}

void test_menu_UpdateTimestamp_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();
    Input input;

    size_t cursorPos = 3;
    for (size_t i = 0; i < cursorPos; i++) {
        input.press('d');
        menu->update(&input);
    }

    input.press('l');
    menu->update(&input);
    menu->render();

    // do it
    input.press('a');
    menu->update(&input);
}


void test_menu_transferPhysicalAppearance_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();
    Input input;

    input.press('l');
    menu->update(&input);
    menu->render();

    // do it
    input.press('a');
    menu->update(&input);
}

void test_menu_transferOwnership_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();
    Input input;

    input.press('d');
    menu->update(&input);
    input.press('l');
    menu->update(&input);
    menu->render();

    // do it
    input.press('a');
    menu->update(&input);
}

void test_menu_MakeItLocal_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();
    Input input;

    size_t cursorPos = 2;
    for (size_t i = 0; i < cursorPos; i++) {
        input.press('d');
        menu->update(&input);
    }

    input.press('l');
    menu->update(&input);
    menu->render();

    // do it
    input.press('a');
    menu->update(&input);
}


void test_menu_selectUp_Top_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {
    menu->render();
    Input input;

    for (size_t i = 0; i < 12; i++) {
        input.press('u');
        menu->update(&input);
    }
}

void test_menu_selectBack_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {
    menu->render();
    Input input;
    input.press('b');
    menu->update(&input);
}


//// MII SELECT

int mii_select_count = 0;

void test_menu_selectOnlyOneMii_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {
    menu->render();
    Input input;

    //deselect all miis
    input.press('-');
    menu->update(&input);
    menu->render();

    //select 1 miis

    for (int i = 0; i < mii_select_count % 4; i++) {
        input.press('d');
        menu->update(&input);
    }
    //mii_select_count++;
    input.press('l');
    menu->update(&input);
    menu->render();

    // to next menu
    input.press('a');
    menu->update(&input);
}

void test_menu_selectAllMiis_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {
    menu->render();
    Input input;
    input.press('+');
    menu->update(&input);
    menu->render();
    // to next menu
    input.press('a');
    menu->update(&input);
}


void test_menu_selectOnlyOneMii2_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {
    menu->render();
    Input input;

    //deselect all miis
    input.press('-');
    menu->update(&input);
    menu->render();

    //select 1 miis
    input.press('d');
    menu->update(&input);
    /*
    input.press('d');
    menu->update(&input);
    */
    input.press('l');
    menu->update(&input);
    menu->render();
    // to next menu
    input.press('a');
    menu->update(&input);
}


///// NAVIGATIONS POST-REPO SELECT

void nav_ListTask([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    //check share/min
    test_menu_selectListTask_do(mii_process_shared_state, menu);

    /*
    if (mii_process_shared_state->primary_mii_repo->miis.size() > 0) {
        size_t idflags_offset = (mii_process_shared_state->primary_mii_repo->miis.at(0)->mii_type == Mii::WIIU) ? 0x31 : 0x21;
        view_all_miidata(mii_process_shared_state->primary_mii_repo, idflags_offset, idflags_offset + 1);
    }
*/


    if (mii_process_shared_state->primary_mii_repo->miis.size() > 0) {
        size_t authorid_offset = (mii_process_shared_state->primary_mii_repo->miis.at(0)->mii_type == Mii::WIIU) ? 0x04 : 0x04; //wii does not have
        view_all_miidata(mii_process_shared_state->primary_mii_repo, authorid_offset, authorid_offset + 8);
    }
}

void nav_ListTask_Views([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    //check share/min
    test_menu_selectListTask_Views_do(mii_process_shared_state, menu);

    /*
    if (mii_process_shared_state->primary_mii_repo->miis.size() > 0) {
        size_t idflags_offset = (mii_process_shared_state->primary_mii_repo->miis.at(0)->mii_type == Mii::WIIU) ? 0x31 : 0x21;
        view_all_miidata(mii_process_shared_state->primary_mii_repo, idflags_offset, idflags_offset + 1);
    }
*/

    if (mii_process_shared_state->primary_mii_repo->miis.size() > 0) {
        size_t authorid_offset = (mii_process_shared_state->primary_mii_repo->miis.at(0)->mii_type == Mii::WIIU) ? 0x04 : 0x04; //wii does not have
        view_all_miidata(mii_process_shared_state->primary_mii_repo, authorid_offset, authorid_offset + 8);
    }
}

void nav_ExportTaskOneMii([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("\n\n\nto select Export\n\n\n");
    test_menu_selectExportTask_do(mii_process_shared_state, menu);
    printf("\n\n\nselect one mii\n\n\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_ExportTaskOneMii_with_targetRepoSel([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("\n\n\nto select Export\n\n\n");
    test_menu_selectExportTask_do(mii_process_shared_state, menu);
    printf("\n\n\nselect targetRepo\n\n\n");
    test_menu_selectRepoFirstCompatible_do(mii_process_shared_state, menu);
    printf("\n\n\nselect one mii\n\n\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\ndone\n\n\n");
    menu->render();
}

void nav_ExportTaskAllMiis([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("\n\n\nto select Export\n\n\n");
    test_menu_selectExportTask_do(mii_process_shared_state, menu);
    printf("\n\n\nselect all miis\n\n\n");
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("\n\ndone - print first screen\n\n\n");
    menu->render();
}

void nav_ExportTaskAllMiis_with_targetRepoSel([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("\n\n\nto select Export\n\n\n");
    test_menu_selectExportTask_do(mii_process_shared_state, menu);
    printf("\n\n\nselect targetRepo\n\n\n");
    test_menu_selectRepoFirstCompatible_do(mii_process_shared_state, menu);
    printf("\n\n\nselect all miis\n\n\n");
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_ImportTaskAllMiis([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("\n\n\nto select Import\n\n\n");
    test_menu_selectImportTask_do(mii_process_shared_state, menu);
    printf("\n\n\nselect all miis\n\n\n");
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_ImportTaskOneMii([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("\n\n\nto select Import\n\n\n");
    test_menu_selectImportTask_do(mii_process_shared_state, menu);
    printf("\n\n\nselect targetRepo\n\n\n");
    test_menu_selectRepoFirstCompatible_do(mii_process_shared_state, menu);
    printf("\n\n\nselect one mii\n\n\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_ImportTaskOneMii_with_targetRepoSel([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("\n\n\nto select Import\n\n\n");
    test_menu_selectImportTask_do(mii_process_shared_state, menu);
    printf("\n\n\nselect source Repo\n\n\n");
    test_menu_selectRepoFirstCompatible_do(mii_process_shared_state, menu);
    printf("\n\n\nselect mii\n\n\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_ImportTaskAllMiis_with_targetRepoSel([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("\n\n\nto select Import\n\n\n");
    test_menu_selectImportTask_do(mii_process_shared_state, menu);
    printf("\n\n\nselect source Repo\n\n\n");
    test_menu_selectRepoFirstCompatible_do(mii_process_shared_state, menu);
    printf("\n\n\nselect all miis\n\n\n");
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_WipeTaskOneMii([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("\n\n\nto select Wipe mii\n\n\n");
    test_menu_selectWipeMiiTask_do(mii_process_shared_state, menu);
    printf("\n\n\nselect one mii\n\n\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_WipeTaskAllMiis([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("\n\n\nto select Wipe mii\n\n\n");
    test_menu_selectWipeMiiTask_do(mii_process_shared_state, menu);
    printf("\n\n\nselect all miis\n\n\n");
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_ImportOneMiiAccount([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("\n\n\nto select Import\n\n\n");
    test_menu_selectImportTask_do(mii_process_shared_state, menu);
    printf("\n\n\nselect mii\n\n\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\n\nselect repo with miis to import\n\n\n");
    test_menu_selectRepoAccountStage_do(mii_process_shared_state, menu);
    printf("\n\n\nselect mii to be imported\n\n\n");
    mii_select_count = 0;
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_ToggleCopyFlag([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    //printf("select One Mii\n");
    //test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("select All Mii\n");
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("to toggle copyFlag\n");
    test_menu_toggleCopyFlag_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_updateCRC([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("to updateCRC\n");
    test_menu_updateCRC_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_updateCRC_all([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select all Miis\n");
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("to updateCRC\n");
    test_menu_updateCRC_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_ToggleShareFlag([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    //test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("to toggle shareFlag\n");
    test_menu_toggleShareFlag_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_MakeItLocal([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    mii_select_count = 0;
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    //test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("to make it local\n");
    test_menu_MakeItLocal_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_MakeItAllLocal([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select all Miis\n");
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("to make it local\n");
    test_menu_MakeItLocal_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_ToggleNormalSpecialFlag([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    //test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("to toggle NormalSpecialFlag\n");
    test_menu_toggleNormalSpecialFlag_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_ToggleForeignFlag([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select all Miis\n");
    //test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("to toggle Foreign Flag\n");
    test_menu_toggleForeignFlag_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_ToggleFavoriteFlag([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select all miisMii\n");
    //test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("to toggle  FavoriteFlag\n");
    test_menu_toggleFavoriteFlag_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}


void nav_ToggleFavoriteFlagOneMii([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select one Mii\n");
    //test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("to toggle Favorite Flag\n");
    test_menu_toggleFavoriteFlag_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_ToggleTempFlag([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    //test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("to toggle Temp Flag\n");
    test_menu_toggleTempFlag_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_UpdateTimestamp([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("to update timestamp\n");
    test_menu_UpdateTimestamp_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_transferPhysicalAppearanceRFL([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("to trasfer phys appearance\n");
    test_menu_transferPhysicalAppearance_do(mii_process_shared_state, menu);
    printf("select repo RFL STage\n");
    test_menu_selectRepoRFLStage_from_RFL_do(mii_process_shared_state, menu);
    printf("select mii template\n");
    mii_select_count = 0;
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}


void nav_transferPhysicalAppearanceFFL([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("set transfer physical appearance\n");
    test_menu_transferPhysicalAppearance_do(mii_process_shared_state, menu);
    printf("select repo FFLStage\n");
    test_menu_selectRepoFFLStage_do(mii_process_shared_state, menu);
    printf("select mii template\n");
    mii_select_count = 0;
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_transferPhysicalAppearanceFFLStage([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("set transfer physical appearance\n");
    test_menu_transferPhysicalAppearance_do(mii_process_shared_state, menu);
    printf("select repo FFL\n");
    test_menu_selectRepoFFL_do(mii_process_shared_state, menu);
    printf("select mii template\n");
    mii_select_count = 0;
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_transferOwnershipAccount([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    mii_select_count = 0;
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("to transfer ownership\n");
    test_menu_transferOwnership_do(mii_process_shared_state, menu);
    printf("select repo\n");
    test_menu_selectRepoAccountAux_do(mii_process_shared_state, menu);
    printf("select mii template\n");
    mii_select_count = 0;
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_transferPhysicalAppearanceFFL_same_mii([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    mii_select_count = 0;
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("to transfer physical appearance\n");
    test_menu_transferPhysicalAppearance_do(mii_process_shared_state, menu);
    printf("select repo\n");
    test_menu_selectRepoFFL_do(mii_process_shared_state, menu);
    printf("select mii template\n");
    mii_select_count = 0;
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}


void nav_transferOwnershipRFL([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("to transferOwnership\n");
    test_menu_transferOwnership_do(mii_process_shared_state, menu);
    printf("select repo\n");
    test_menu_selectRepoRFLStage_from_RFL_do(mii_process_shared_state, menu);
    printf("select mii template\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_transferOwnershipFFL([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("to transferOwnership\n");
    test_menu_transferOwnership_do(mii_process_shared_state, menu);
    printf("select repo\n");
    test_menu_selectRepoFFLStage_do(mii_process_shared_state, menu);
    ;
    printf("select mii template\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

void nav_transferOwnershipFFLStage([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    test_menu_selectAllMiis_do(mii_process_shared_state, menu);
    printf("to transferOwnership\n");
    test_menu_transferOwnership_do(mii_process_shared_state, menu);
    printf("select repo\n");
    test_menu_selectRepoFFL_do(mii_process_shared_state, menu);
    printf("select mii template\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}


void nav_BackupTask([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to backup task\n");
    test_menu_selectBackupTask_do(mii_process_shared_state, menu);
}

void nav_RestoreTask([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to restore task\n");
    test_menu_selectRestoreTask_do(mii_process_shared_state, menu);
}

void nav_AccountRestoreTask([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to restore task\n");
    test_menu_selectRestoreTask_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    menu->render();
}


void nav_AccountXRestoreTask([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to xrestore task\n");
    test_menu_selectXRestoreTask_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("select slot\n");
    test_menu_selectSlotTask_do(mii_process_shared_state, menu);
    printf("select a different mii\n");
    test_menu_selectOnlyOneMii2_do(mii_process_shared_state, menu);
    menu->render();
}


void nav_WipeTask([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to wipe task\n");
    test_menu_selectWipeTask_do(mii_process_shared_state, menu);
}

void nav_InitializeTask([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("\n\nto initialize task\n\n");
    test_menu_selectInitializeTask_do(mii_process_shared_state, menu);
}

void nav_Back_To_RepoSelect([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    test_menu_selectBack_do(mii_process_shared_state, menu);
    test_menu_selectBack_do(mii_process_shared_state, menu);
    test_menu_selectBack_do(mii_process_shared_state, menu);
}


void nav_WipeTask_double([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    Input input;
    printf("\n\nto wipe task\n\n");
    test_menu_selectWipeTask_do(mii_process_shared_state, menu);
    menu->render();
    input.press('a');
    menu->update(&input);
    menu->render();
}

//////

TEST_SUITE("PROGRAM") {
    TEST_CASE("HELP") {
        std::string message = R"END(
    savemymiis -tc=REPO_task

    where

        REPO can be FFL, FFL_S, RFL, RFL_S, ACCOUNT, ACCOUNT_S

            (FFL = Wii U DB, RFL = Wii DB, ACCOUNT = Wii U Profiles, and *_S are stage folders)

            PATH to the different repos are configured in config.in file.  If they do not exist,
                they will be created the first time savemymiis is executed.

        task can be any of

            list            import_one  export_one  wipe_one    backup          update_crc
            list_views      import_all  export_all  wipe_all    restore
                                                                wipe
                                                                initialize

            copy_flag                   update_timestamp        transfer_ownership
            share_flag                                          transfer_appearance
            normal_special_flag         make_local_all
            foreign_flag                make_local_one
            temp_flag

    Ex/

        savemymiis -tc=FFL_list
        savemymiis -tc=RFL_share_flag       # Toggle share flag for all miis in repo RFL
        savemymiis -tc=FFL_import_all       # Import all miis in FFL_S into repo FFL
        savemymiis -tc=RFL_export_all       # export all miis in RFL to repo RFL_S

    Most tasks operate over ALL miis in the selected repository.

    Export / Import copy miis between a repo and its _stage counterpart.

    make_local uses information from file ambient.in, where the desired values
       for different parameters must be manually configured
        )END";
        std::cout << message << std::endl;
        exit(0);
    }
}

TEST_SUITE("BASICS") {

    TEST_CASE("slot is empty") {
        test_initMiiRepos();
        test_is_slot_empty();
        test_deinitMiiRepos();
    }

    TEST_CASE("get an empty slot") {
        test_initMiiRepos();
        test_get_empty_slot();
        test_deinitMiiRepos();
    }


    TEST_CASE("get backup path") {
        test_initMiiRepos();
        test_get_backup_path();
        test_deinitMiiRepos();
    }

    TEST_CASE("mii utils create repo get") {
        test_initMiiRepos();
        test_miiutils_create_repo_get();
        test_deinitMiiRepos();
    }
}

TEST_SUITE("POPULATE") {
    TEST_CASE("populate repo") {
        test_initMiiRepos();
        test_populate_repo();
        test_deinitMiiRepos();
    }
}

TEST_SUITE("SELECT AND LIST") {

    TEST_CASE("list a repo") {
        test_initMiiRepos();
        test_test_list_repo();
        test_deinitMiiRepos();
    }
}

TEST_SUITE("MENUS") {
    TEST_CASE("show db options menu") {
        test_initMiiRepos();
        test_mii_db_options_menu();
        test_deinitMiiRepos();
    }

    TEST_CASE("show repo menu") {
        test_initMiiRepos();
        test_mii_repo_menu();
        test_deinitMiiRepos();
    }

    TEST_CASE("show tasks menu") {
        test_initMiiRepos();
        test_mii_tasks_menu();
        test_deinitMiiRepos();
    }

    TEST_CASE("show transform tasks menu") {
        test_initMiiRepos();
        test_mii_transform_tasks_menu();
        test_deinitMiiRepos();
    }
}

TEST_SUITE("BRW") {
    TEST_CASE("backup") {
        test_initMiiRepos();
        test_backup();
        test_deinitMiiRepos();
        FSUtils::deinit_fs_buffers();
    }

    TEST_CASE("wipe") {
        test_initMiiRepos();
        test_wipe();
        test_deinitMiiRepos();
        FSUtils::deinit_fs_buffers();
    }

    TEST_CASE("restore") {
        test_initMiiRepos();
        test_restore();
        test_deinitMiiRepos();
        FSUtils::deinit_fs_buffers();
    }
}

TEST_SUITE("SOME REPO TASKS") {

    TEST_CASE("extract and import mii") {
        test_initMiiRepos();
        test_extract_and_import_mii();
        test_deinitMiiRepos();
    }

    TEST_CASE("populate_and_free") {
        test_initMiiRepos();
        test_test_populate_and_free_repo();
        test_deinitMiiRepos();
    }

    TEST_CASE("get_crc") {
        test_initMiiRepos();
        test_get_crc();
        test_deinitMiiRepos();
    }

    TEST_CASE("get_crc_after_write") {
        test_initMiiRepos();
        test_get_crc_after_write();
        test_deinitMiiRepos();
    }
}

TEST_SUITE("CRC_FIX") {
    TEST_CASE("test_persist_first_repo") {
        test_initMiiRepos();
        test_persist_first_repo();
        test_deinitMiiRepos();
    }

    TEST_CASE("update_FFL_crc") {
        test_initMiiRepos();
        test_persist_FFL_repo();
        test_deinitMiiRepos();
    }

    TEST_CASE("update_RFL_crc") {
        test_initMiiRepos();
        test_persist_RFL_repo();
        test_deinitMiiRepos();
    }

    TEST_CASE("menu_selectRepoAccount_updateCRC_do") {
        test_initMiiRepos();
        test_menu_selectRepoAccount_do(_mii_process_shared_state, _menu);
        nav_updateCRC(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }
}

TEST_SUITE("ACCOUNT") {
    TEST_CASE("ACCOUNT_list") {
        test_initMiiRepos();
        test_menu_selectRepoAccount_do(_mii_process_shared_state, _menu);
        nav_ListTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_list_views") {
        test_initMiiRepos();
        test_menu_selectRepoAccount_do(_mii_process_shared_state, _menu);
        nav_ListTask_Views(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_export_one") {

        test_initMiiRepos();
        test_menu_selectRepoAccount_do(_mii_process_shared_state, _menu);
        nav_ExportTaskOneMii(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_import_one") {
        test_initMiiRepos();
        test_menu_selectRepoAccount_do(_mii_process_shared_state, _menu);
        nav_ImportOneMiiAccount(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_export_all") {

        test_initMiiRepos();
        test_menu_selectRepoAccount_do(_mii_process_shared_state, _menu);
        nav_ExportTaskAllMiis(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_copy_flag") {
        test_initMiiRepos();
        test_menu_selectRepoAccount_do(_mii_process_shared_state, _menu);
        nav_ToggleCopyFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_share_flag") {
        test_initMiiRepos();
        test_menu_selectRepoAccount_do(_mii_process_shared_state, _menu);
        nav_ToggleShareFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_transfer_appearance") {
        test_initMiiRepos();
        test_menu_selectRepoAccount_do(_mii_process_shared_state, _menu);
        nav_transferPhysicalAppearanceFFL(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_transfer_ownership") {
        test_initMiiRepos();
        test_menu_selectRepoAccount_do(_mii_process_shared_state, _menu);
        nav_transferOwnershipAccount(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_backup") {
        test_initMiiRepos();
        test_menu_selectRepoAccount_do(_mii_process_shared_state, _menu);
        nav_BackupTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }


    TEST_CASE("ACCOUNT_restore") {
        test_initMiiRepos();
        test_menu_selectRepoAccount_do(_mii_process_shared_state, _menu);
        nav_AccountRestoreTask(_mii_process_shared_state, _menu);
        test_menu_selectBack_do(_mii_process_shared_state, _menu);
        test_menu_selectUp_Top_do(_mii_process_shared_state, _menu);
        nav_ListTask_Views(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_xrestore") {
        test_initMiiRepos();
        test_menu_selectRepoAccount_do(_mii_process_shared_state, _menu);
        nav_AccountXRestoreTask(_mii_process_shared_state, _menu);
        test_menu_selectBack_do(_mii_process_shared_state, _menu);
        test_menu_selectUp_Top_do(_mii_process_shared_state, _menu);
        nav_ListTask_Views(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNTupdate_timestamp") {
        test_initMiiRepos();
        test_menu_selectRepoAccount_do(_mii_process_shared_state, _menu);
        nav_UpdateTimestamp(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }
}

TEST_SUITE("ACCOUNTStage") {

    TEST_CASE("ACCOUNT_S_list") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_ListTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_list_views") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_ListTask_Views(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_export_one") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_ExportTaskOneMii_with_targetRepoSel(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_export_all") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_ExportTaskAllMiis_with_targetRepoSel(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_import_one") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_ImportTaskOneMii_with_targetRepoSel(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_import_all") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_ImportTaskAllMiis_with_targetRepoSel(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_wipe_one") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_WipeTaskOneMii(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_wipe_all") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_WipeTaskAllMiis(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }


    TEST_CASE("ACCOUNT_S_share_flag") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_ToggleShareFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_normal_special_flag") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_ToggleNormalSpecialFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_copy_flag") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_ToggleCopyFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_Supdate_timestamp") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_UpdateTimestamp(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_backup") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_BackupTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_restore") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_RestoreTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_wipe") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_WipeTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_initialize") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_InitializeTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_update_crc") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_updateCRC_all(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("ACCOUNT_S_temp_flag") {
        test_initMiiRepos();
        test_menu_selectRepoAccountStage_do(_mii_process_shared_state, _menu);
        nav_ToggleTempFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }
}

TEST_SUITE("RFL") {

    TEST_CASE("RFL_list") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_ListTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_list_views") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_ListTask_Views(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_normal_special_flag") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_ToggleNormalSpecialFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFLupdate_timestamp") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_UpdateTimestamp(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_backup") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_BackupTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_restore") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_RestoreTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_wipe") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_WipeTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_initialize") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_InitializeTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }


    TEST_CASE("RFL_export_all") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_ExportTaskAllMiis(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_import_all") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_ImportTaskAllMiis(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_import_one") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_ImportTaskOneMii(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_export_one") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_ExportTaskOneMii(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_share_flag") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_ToggleShareFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }


    TEST_CASE("RFL_favorite_flag") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_ToggleFavoriteFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_foreign_flag") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_ToggleForeignFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_transfer_ownership") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_transferOwnershipRFL(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_transfer_appearance") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_transferPhysicalAppearanceRFL(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }


    TEST_CASE("RFL_update_crc") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_updateCRC_all(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_make_local_one") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_MakeItLocal(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }


    TEST_CASE("RFL_make_local_all") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_MakeItAllLocal(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_wipe_one") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_WipeTaskOneMii(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_wipe_all") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_WipeTaskAllMiis(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }
}

TEST_SUITE("RFLStage") {

    TEST_CASE("RFL_S_list") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_ListTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_list_views") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_ListTask_Views(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_normal_special_flag") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_ToggleNormalSpecialFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_export_one") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_ExportTaskOneMii_with_targetRepoSel(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_export_all") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_ExportTaskAllMiis_with_targetRepoSel(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_import_all") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_ImportTaskAllMiis(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_import_one") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_ImportTaskOneMii(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_backup") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_BackupTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_wipe") {
        test_initMiiRepos();
        test_menu_selectRepoRFL_do(_mii_process_shared_state, _menu);
        nav_WipeTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_initialize") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_InitializeTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_restore") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_RestoreTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_share_flag") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_ToggleShareFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }


    TEST_CASE("RFL_S_update_crc") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_updateCRC(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_Supdate_timestamp") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_UpdateTimestamp(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_favorite_flag") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_ToggleFavoriteFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_foreign_flag") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_ToggleForeignFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_transfer_ownership") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_transferOwnershipRFL(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_transfer_appearance") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_transferPhysicalAppearanceRFL(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_wipe_all") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_WipeTaskAllMiis(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_make_local_one") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_MakeItLocal(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }


    TEST_CASE("RFL_S_make_local_all") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_MakeItAllLocal(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_wipe_one") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_WipeTaskOneMii(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("RFL_S_wipe_all") {
        test_initMiiRepos();
        test_menu_selectRepoRFLStage_do(_mii_process_shared_state, _menu);
        nav_WipeTaskAllMiis(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }
}

TEST_SUITE("FFL") {

    TEST_CASE("FFL_list") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_ListTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_list_views") {

        /*
        uint16_t prova = 0xabcd;
        printf("%04x\n",prova); 
        printf("%04x\n",__builtin_bswap16(prova)); 
        */

        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_ListTask_Views(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_export_one") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_ExportTaskOneMii(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_export_all") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_ExportTaskAllMiis(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_import_one") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_ImportTaskOneMii(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_import_all") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_ImportTaskAllMiis(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_wipe_one") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_WipeTaskOneMii(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_wipe_all") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_WipeTaskAllMiis(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_make_local_one") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_MakeItLocal(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_make_local_all") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_MakeItAllLocal(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_share_flag") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_ToggleShareFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_copy_flag") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_ToggleCopyFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_normal_special_flag") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_ToggleNormalSpecialFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_temp_flag") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_ToggleTempFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_favorite_flag") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_ToggleFavoriteFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_favorite_flag_one") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_ToggleFavoriteFlagOneMii(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_foreign_flag") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_ToggleForeignFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_transfer_appearance") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_transferPhysicalAppearanceFFL(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }


    TEST_CASE("FFL_update_crc") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_updateCRC(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFLupdate_timestamp") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_UpdateTimestamp(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_backup") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_BackupTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_restore") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_RestoreTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_wipe") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_WipeTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }


    TEST_CASE("FFL_initialize") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_InitializeTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_transfer_ownership") {
        test_initMiiRepos();
        test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
        nav_transferOwnershipFFL(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }
}

TEST_SUITE("FFLStage") {


    TEST_CASE("FFL_S_list") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_ListTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_list_views") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_ListTask_Views(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_export_one") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_ExportTaskOneMii_with_targetRepoSel(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_export_all") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_ExportTaskAllMiis_with_targetRepoSel(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_import_one") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_ImportTaskOneMii_with_targetRepoSel(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_import_all") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_ImportTaskAllMiis_with_targetRepoSel(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_wipe_one") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_WipeTaskOneMii(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_share_flag") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_ToggleShareFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_normal_special_flag") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_ToggleNormalSpecialFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_copy_flag") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_ToggleCopyFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_update_crc") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_updateCRC_all(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_Supdate_timestamp") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_UpdateTimestamp(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_backup") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_BackupTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_restore") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_RestoreTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_wipe") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_WipeTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_initialize") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_InitializeTask(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_wipe_all") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_WipeTaskAllMiis(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_temp_flag") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_ToggleTempFlag(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_make_local_one") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_MakeItLocal(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_make_local_all") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_MakeItAllLocal(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_transfer_ownership") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_transferOwnershipFFLStage(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }

    TEST_CASE("FFL_S_transfer_appearance") {
        test_initMiiRepos();
        test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
        nav_transferPhysicalAppearanceFFLStage(_mii_process_shared_state, _menu);
        test_deinitMiiRepos();
    }
}


TEST_CASE("menu_wipeFFLStage_ExportFFL_do") {
    test_initMiiRepos();
    test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
    nav_WipeTask(_mii_process_shared_state, _menu);

    test_menu_selectPreviousRepo_do(_mii_process_shared_state, _menu);
    nav_ExportTaskAllMiis(_mii_process_shared_state, _menu);

    test_deinitMiiRepos();
}


TEST_CASE("menu_selectRepoFFL_transferPhysicalAppearance_same_mii_do") {
    test_initMiiRepos();
    test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
    nav_transferPhysicalAppearanceFFL_same_mii(_mii_process_shared_state, _menu);
    test_deinitMiiRepos();
}

void test_menu_error([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {

    menu->render();
    Input input;

    input.press('l');
    menu->update(&input);
    menu->render();

    // back
    input.press('b');
    menu->update(&input);

    menu->render();
}

void nav_error([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    printf("to toggle copyFlag\n");
    test_menu_error(mii_process_shared_state, menu);
}

TEST_CASE("error") {
    test_initMiiRepos();
    test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
    nav_error(_mii_process_shared_state, _menu);
    test_deinitMiiRepos();
}

TEST_CASE("double_wipe") {
    test_initMiiRepos();
    test_menu_selectRepoFFLStage_do(_mii_process_shared_state, _menu);
    nav_WipeTask_double(_mii_process_shared_state, _menu);
    test_deinitMiiRepos();
}


void nav_tag([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {

    menu->render();

    Input input;

    // select backup task
    // to  select miis to trasnform list
    input.press('a');
    menu->update(&input);

    // select slot 2
    input.press('r');
    menu->update(&input);
    input.press('r');
    menu->update(&input);

    input.press('a');
    menu->update(&input);
    menu->render();

    input.press('+');
    menu->update(&input);
    menu->render();
}
TEST_CASE("notag_do") {
    test_initMiiRepos();
    test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
    nav_tag(_mii_process_shared_state, _menu);
    test_deinitMiiRepos();
}


TEST_CASE("wipe_and_list") {
    test_initMiiRepos();
    test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
    nav_ListTask(_mii_process_shared_state, _menu);
    test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
    Input input;
    input.press('u');
    _menu->update(&input);
    input.press('u');
    _menu->update(&input);
    input.press('u');
    _menu->update(&input);
    input.press('u');
    _menu->update(&input);

    nav_WipeTask(_mii_process_shared_state, _menu);
    test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
    input.press('u');
    _menu->update(&input);
    input.press('u');
    _menu->update(&input);
    nav_ListTask(_mii_process_shared_state, _menu);
    test_deinitMiiRepos();
}


TEST_CASE("wipe_and_initialize_and_list") {
    test_initMiiRepos();
    test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
    nav_ListTask(_mii_process_shared_state, _menu);
    test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
    Input input;
    input.press('u');
    _menu->update(&input);
    input.press('u');
    _menu->update(&input);
    input.press('u');
    _menu->update(&input);
    input.press('u');
    _menu->update(&input);

    nav_WipeTask(_mii_process_shared_state, _menu);
    test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
    input.press('u');
    _menu->update(&input);
    input.press('u');
    _menu->update(&input);
    nav_InitializeTask(_mii_process_shared_state, _menu);
    test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
    input.press('u');
    _menu->update(&input);
    input.press('u');
    _menu->update(&input);
    input.press('u');
    _menu->update(&input);
    nav_ListTask(_mii_process_shared_state, _menu);
    test_deinitMiiRepos();
}


void test_menu_select_20_Miis_do([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, ApplicationState *menu) {
    menu->render();
    Input input;
    input.press('+');
    menu->update(&input);
    menu->render();

    for (int i = 0; i < 10; i++) {
        input.press('l');
        menu->update(&input);
        input.press('d');
        menu->update(&input);
    }
    menu->render();

    // to next menu
    input.press('a');
    menu->update(&input);
}

void nav_ToggleFavoriteFlag20([[maybe_unused]] MiiProcessSharedState *mii_process_shared_state, MiiRepoSelectState *menu) {
    printf("to select transform\n");
    test_menu_selectTransformTasks_do(mii_process_shared_state, menu);
    printf("select Mii\n");
    //test_menu_selectOnlyOneMii_do(mii_process_shared_state, menu);
    test_menu_select_20_Miis_do(mii_process_shared_state, menu);
    printf("to togglefavoriteFlag\n");
    test_menu_toggleFavoriteFlag_do(mii_process_shared_state, menu);
    printf("\n\n\ndone\n\n\n");
    menu->render();
}

TEST_CASE("menu_selectRepoFFL_toggleFavoriteFlag20_do") {
    test_initMiiRepos();
    test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
    nav_ToggleFavoriteFlag20(_mii_process_shared_state, _menu);
    test_deinitMiiRepos();
}

TEST_CASE("FFL_init_init_do") {
    test_initMiiRepos();
    test_menu_selectRepoFFL_do(_mii_process_shared_state, _menu);
    nav_InitializeTask(_mii_process_shared_state, _menu);
    Input input;
    input.press('a');
    _menu->update(&input);
    test_deinitMiiRepos();
}

TEST_CASE("OSGetTime") {

    OSTime osticks = OSGetTime();
    printf("ticks: %016lx\n",osticks);
 
    printf("years: %ld\n",osticks/3600/24/365);
 

}