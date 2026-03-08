# SaveMyMiis

Linux Mii Manager.

`savemymiis` calls SaveMii Mii management libs through the test framework DocTest. Which means no fancy UI, just an executable that expects the name of a testcase to execute, that will perform the task at hand, and then will return a "multiline log-like" version of the formatted screens that are shown when you run SaveMii in a Wii U.

It was originally used to test all SaveMii mii management functionality as more tasks and repo types were incrementally added, perform regression testing, mem leak hunting, ... Its `main` function has become a horrible beast growing out of control ..., but nevertheless it has been very useful.

You can:

- Backup / Restore / Wipe full repos
- Export / Import / Wipe individual miis 
- Transform individual miis on any repo. You can:
    - Make a mii local to the console, transfer ownership from one mii to another, transfer physical attributes from one mii to another, convert betwen Normal/Special/Temporal miis, togle copy and share attributes, update mii Id, make a mii a favorite one, update its CRC checksum

If you configure FFL and RFL repos to point to a CEMU or a Dolphin installation respectively, you can apply any modification and instantly check what has happened with the mii.

IMPORTANT!! Make always a copy of your files before playing with them!

Usage:

```sh
    savemymiis -tc=REPO_task

    where

        REPO can be FFL, FFL_S, RFL, RFL_S, ACCOUNT, ACCOUNT_S

            (FFL = Wii U DB, RFL = Wii DB, ACCOUNT = Wii U Profiles, and *_S are stage folders)

            PATH to the different repos are configured in config.in file. If they do not exist,
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

```
`make_local` uses information from file `ambient.in`, where the values for some console attributes (serial_id, device_hash, mac_adress, ...) should be defined. If you have a mii from the target repo, it is easier to use `transfer_ownership` task. If not, you will need at least:
- For Wii/Dolphin: `mac_adress` is enough. Get it from the Internet Settings.
- For Wii U/CEMU: you will need only `author_id`. If you have MiiMaker installed in CEMU, simply create one mii and copy the higlighted 16 hex digit output of `FFL_list` for this mii in to the `author_id` attribute in ambient.in. `FFL_make_local` will then transform all miis in any repo where you apply this task to local CEMU ones. Remember that this will change Mii Ids and break any game association that depends on the Mii Id value.


# Mii DBs format

## Revolution Face Lib / RFL (Wii) db file

Format is described in [WiiBrew / Mii data](https://wiibrew.org/wiki/Mii_data) and [WiiBrew / RFL_DB](https://wiibrew.org/wiki//shared2/menu/FaceLib/RFL_DB.dat).

Main fields:

Offset  | Lenght| Description
--------|-------|-------------
0       | 4     | 'Magic' header RNOD
4      | 0x4A * 100  | Array of Mii Data
0x1CEC |1 | 0x80
... |  | 0x0?
0x1D00 | 4  | 'Magic' Header RNHD
0x1D04 | ...  | Mii Parade Data
0x1F1DE|2|CRC16 from 0x0 to 0x1F1DD
0x1F1E0| ...| Unknown

Length: 0x0BE6C0
 

## caFe  Face Lib / FFL (Wii U) db file

Offset  | Lenght| Description
--------|-------|-------------
0       | 4     | 'Magic' header FFFOC
4       | 4     | Incremental counter (updates)
8       | 0x5C * 3000  | Array of Mii Data
0x43628 | 0xA * 50  | Favorite section. Array of MiiId of the favorite miis. If the Mii Id is not already in stadio.sav, Mii Maker will delete it from this section.
0x4383E |2       |CRC16 from 0x0 to 0x4383D 

Lenght: 0x43840

MiiData is described in [HEYimHeroic pages (le format)](https://github.com/HEYimHeroic/mii2studio/blob/master/mii_data_ver3.ksy)
or in [WUT pages (be format)](https://wut.devkitpro.org/miidata_8h_source.html)

## stadio.sav file

Contains information on how the Miis are shown in in MiiMaker app

Offset  | Lenght| Description
--------|-------|-------------
0       |4      | 'Magic' MSU\0?
4       |2      |  "5" Major?
6       |2      |  "1" Minor?
0x10    |8      | Last Update timestamp: "ticks" since 1/1/2000 (OSGetTime()).
0x18    |4      | Another update counter
0x1C    |1      | If 0, initial welcome screens are shown. If 1, they are skipped
0x40    |1      | 0/1 to enable/disable message "You can save up too 3000 miis.." after you select  "Select Mii" option.
0x43    |1      | 0/1 to enable/disable message "Choose features to create ..."
0x43    |1      | 0/1 to enable/disable message "Mii characters with inappropriate names ..."
0x4000  |0x40 * 3000      | Stadio mii data for each mii
0x32e00 |0x40 * 12      | Stadio mii data for profile miis
0x33100 | 8     |  Global Mii number counter, incremented every time a mii is created
0x33108 | 8     |  Global Update counter, incremented every time a mii is updated
0x33110 | 4     |  Maximum number of miis concurrently "alive" in any point of time (not including profile ones)
0x33117 | 1     | Seems to be always initialized to 0x3B

Length: 0x34000


### Stadio mii data 

Offset  | Lenght| Description
--------|-------|-------------
0       | 8     | Mii number assigned when the mii was created 
8       | 8     | (Global) Update number of the last modification
0x10    | 0xA   | Mii Id
0x1A    | 2     | Frame asigned in Mii Makers frames screen, counting from profile miis, that always appear first
0x1C    | 1     | Mii Pose, different ones from 0 to 0xe, repeating poses from then 
0x1E    | 1     | Controls shining stars over a Mii when entering Mii Maker frames screen: 0 -> no stars, 1 -> green stars (after modification) , 2 -> yellow stars (after creation) 

Length: 0x40
