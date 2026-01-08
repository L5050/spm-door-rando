#include "mod.h"
#include "patch.h"
#include "util.h"

#include <spm/rel/machi.h>
#include <spm/camdrv.h>
#include <spm/evt_door.h>
#include <spm/evt_sub.h>
#include <spm/fontmgr.h>
#include <spm/mapdrv.h>
#include <spm/map_data.h>
#include <spm/memory.h>
#include <spm/seqdrv.h>
#include <spm/seqdef.h>
#include <spm/system.h>
#include <wii/os/OSError.h>
#include <msl/string.h>
#include <msl/stdio.h>
#include <wii/gx.h>

namespace mod {
/*
    Title Screen Custom Text
    Prints "SPM Rel Loader" at the top of the title screen
*/

static spm::seqdef::SeqFunc *seq_titleMainReal;
static void seq_titleMainOverride(spm::seqdrv::SeqWork *wp)
{
    wii::gx::GXColor green = {0, 255, 0, 255};
    f32 scale = 0.8f;
    const char * msg = "SPM-Door-Rando";
    spm::fontmgr::FontDrawStart();
    spm::fontmgr::FontDrawEdge();
    spm::fontmgr::FontDrawColor(&green);
    spm::fontmgr::FontDrawScale(scale);
    spm::fontmgr::FontDrawNoiseOff();
    spm::fontmgr::FontDrawRainbowColorOff();
    f32 x = -((spm::fontmgr::FontGetMessageWidth(msg) * scale) / 2);
    spm::fontmgr::FontDrawString(x, 200.0f, msg);
    seq_titleMainReal(wp);
}

static void titleScreenCustomTextPatch()
{
    seq_titleMainReal = spm::seqdef::seq_data[spm::seqdrv::SEQ_TITLE].main;
    spm::seqdef::seq_data[spm::seqdrv::SEQ_TITLE].main = &seq_titleMainOverride;
}

s32 (*evt_door_set_dokan_descs)(spm::evtmgr::EvtEntry* entry, bool firstRun);
s32 (*evt_door_set_map_door_descs)(spm::evtmgr::EvtEntry* entry, bool firstRun);
s32 (*evt_machi_set_elv_descs)(spm::evtmgr::EvtEntry* entry, bool firstRun);

// Credit goes to Seeky's Practice Codes
struct EntranceNameList
{
    int count;
    const char * names[];
};

struct MapGroup
{
    char name[4];
    u16 firstId;
    u16 count;
    EntranceNameList ** entranceNames;
};

static MapGroup groups[] = {
  //  {"aa1", 1,  2, 0}, {"aa2", 1,  2, 0}, {"aa3",  1,  1, 0}, {"aa4", 1,  1, 0}, // cutscene, commenting out for softlock reasons
  //  {"bos", 1,  1, 0}, {"dos", 1,  1, 0}, // misc, commenting out for softlock reasons 
    {"dan", 11, 14, 0}, {"mac", 1, 30, 0}, // Flipside/Flopside
    {"he1", 1,  6, 0}, {"he2", 1,  9, 0}, {"he3",  1,  8, 0}, {"he4", 1, 12, 0}, // chapter 1
    {"mi1", 1, 11, 0}, {"mi2", 1, 11, 0}, {"mi3",  1,  6, 0}, {"mi4", 1, 15, 0}, // chapter 2
    {"ta1", 1,  9, 0}, {"ta2", 1,  6, 0}, {"ta3",  1,  8, 0}, {"ta4", 1, 15, 0}, // chapter 3
    {"sp1", 1,  7, 0}, {"sp2", 1, 10, 0}, {"sp3",  1,  7, 0}, {"sp4", 1, 17, 0}, // chapter 4
    {"gn1", 1,  5, 0}, {"gn2", 1,  6, 0}, {"gn3",  1, 16, 0}, {"gn4", 1, 17, 0}, // chapter 5
    {"wa1", 1, 27, 0}, {"wa2", 1, 25, 0}, {"wa3",  1, 25, 0}, {"wa4", 1, 26, 0}, // chapter 6
    {"an1", 1, 11, 0}, {"an2", 1, 10, 0}, {"an3",  1, 16, 0}, {"an4", 1, 12, 0}, // chapter 7
    {"ls1", 1, 12, 0}, {"ls2", 1, 18, 0}, {"ls3",  1, 13, 0}, {"ls4", 1, 13, 0}, // chapter 8
//  {"mg1", 1,  1, 0}, {"mg2", 1,  5, 0}, {"mg3",  1,  5, 0}, {"mg4", 1,  1, 0}, // minigames
//  {"tst", 1,  2, 0}, {"kaw", 1,  5, 0}                                         // half-removed
};

#define GROUP_COUNT 34

static EntranceNameList * scanScript(const int * script)
{
    // Return an empty list if there's no script
    if (script == nullptr)
    {
        EntranceNameList * list = reinterpret_cast<EntranceNameList *>(new int[1]);
        list->count = 0;
        return list;
    }

    // Initialise entrance type information
    spm::evt_door::DokanDesc * dokans = nullptr;
    int dokanCount = 0;
    spm::evt_door::MapDoorDesc * mapDoors = nullptr;
    int mapDoorCount = 0;
    spm::machi::ElvDesc * elvs = nullptr;
    int elvCount = 0;

    // Initialize 15 entrances for cutscenes and other stuff
    #define OTHERS_MAX 15
    char ** others = new char*[OTHERS_MAX];
    int othersCount = 0;
    
    // Find entrances
    int cmdn;
    int cmd = 0;
    while (cmd != 1) // endscript
    {
        const short * p = reinterpret_cast<const short *>(script);
        cmd = p[1];
        cmdn = p[0];

        if (cmd == 0x5c) // user_func
        {
            u32 func = script[1];
            if (func == (u32) spm::evt_door::evt_door_set_dokan_descs)
            {
                dokans = reinterpret_cast<spm::evt_door::DokanDesc *>(script[2]);
                dokanCount = script[3];
            }
            else if (func == (u32) spm::evt_door::evt_door_set_map_door_descs)
            {
                mapDoors = reinterpret_cast<spm::evt_door::MapDoorDesc *>(script[2]);
                mapDoorCount = script[3];
            }
            else if (func == (u32) spm::machi::evt_machi_set_elv_descs)
            {
                elvs = reinterpret_cast<spm::machi::ElvDesc *>(script[2]);
                elvCount = script[3];
            }
            else if (func == (u32) spm::evt_sub::evt_sub_get_entername)
            {
                const int * next_script = script + cmdn + 1;
                const short * next_p = reinterpret_cast<const short *>(next_script);
                int next_cmd = next_p[1];

                if (next_cmd == 0xc || next_cmd == 0xd) // if_str_equal, if_str_not_equal
                {
                    assert(othersCount < OTHERS_MAX, "Other entrances table too small");
                    
                    char* entrance = reinterpret_cast<char *>(next_script[2]);

                    // Prevent duplicates
                    bool exists = false;

                    // Without severe refactoring this is how we're gonna do it
                    for (int i = 0; i < othersCount; i++)
                        if (msl::string::strcmp(others[i], entrance) == 0)
                        {
                            exists = true;
                            break;
                        }
                    
                    for (int i = 0; i < dokanCount; i++)
                        if (msl::string::strcmp(dokans[i].name, entrance) == 0)
                        {
                            exists = true;
                            break;
                        }
                    
                    for (int i = 0; i < mapDoorCount; i++)
                        if (msl::string::strcmp(mapDoors[i].name_l, entrance) == 0)
                        {
                            exists = true;
                            break;
                        }
                    
                    for (int i = 0; i < elvCount; i++)
                        if (msl::string::strcmp(elvs[i].name, entrance) == 0)
                        {
                            exists = true;
                            break;
                        }

                    if (!exists)
                    {
                        others[othersCount] = entrance;
                        othersCount++;
                    }
                    
                }
            }
        }

        script += cmdn + 1;
    }

    // Create list
    int entranceCount = dokanCount + mapDoorCount + elvCount + othersCount;
    
    EntranceNameList * list = reinterpret_cast<EntranceNameList *>(new int[entranceCount + 1]);
    list->count = entranceCount;

    int n = 0;
    for (int i = 0; i < dokanCount; i++)
        list->names[n++] = dokans[i].name;
    for (int i = 0; i < mapDoorCount; i++)
        list->names[n++] = mapDoors[i].name_l;
    for (int i = 0; i < elvCount; i++)
        list->names[n++] = elvs[i].name;
    for (int i = 0; i < othersCount; i++)
         list->names[n++] = others[i];

    delete[] others;

    return list;
}

void scanEntrances()
{
    // Scan all maps for their entrances
    for (u32 i = 0; i < ARRAY_SIZEOF(groups); i++)
    {
        // Create list pointer array for this group
        groups[i].entranceNames = new EntranceNameList * [groups[i].count];

        // Run for all maps in this group
        for (int j = 0; j < groups[i].count; j++)
        {
            // Generate map name string
            char name[32];
            msl::stdio::sprintf(name, "%s_%02d", groups[i].name, j + 1);
            //wii::os::OSReport("name %s\n", name);
            // Generate list for this map
            spm::map_data::MapData * md = spm::map_data::mapDataPtr(name);
            int * script = md != nullptr ? (int *) md->initScript : nullptr;
            groups[i].entranceNames[j] = scanScript(script);
        }
    }
}

s32 new_evt_door_set_dokan_descs(spm::evtmgr::EvtEntry* evtEntry, bool firstRun)
{
    if (firstRun)
    {
    spm::evtmgr::EvtVar* args = (spm::evtmgr::EvtVar *)evtEntry->pCurData;
    spm::evt_door::DokanDesc* curDesc = (spm::evt_door::DokanDesc *)spm::evtmgr_cmd::evtGetValue(evtEntry, *args);
    s32 _mapGroup = spm::system::rand() % GROUP_COUNT;
    // thanks again to practice codes for already having error management for this
    if (msl::string::strcmp(groups[_mapGroup].name, "mac") == 0)
    {
        // mac is missing 10, 13, 20-21, 23-29
        switch (_mapGroup)
        {
            case 10:
            _mapGroup = 11;
                break;
            case 13:
            _mapGroup = 14;
                break;
            case 20:
            _mapGroup = 22;
                break;
            case 23:
            _mapGroup = 30;
                break;
        }
    }
    s32 roomGroup = spm::system::rand() % groups[_mapGroup].count;
    if (roomGroup == 0) roomGroup += 1;
    char nameT[32];
    char* name = (char*)spm::memory::__memAlloc(spm::memory::Heap::HEAP_MAP, sizeof(nameT));
    msl::stdio::sprintf(name, "%s_%02d", groups[_mapGroup].name, roomGroup);
    wii::os::OSReport("name %s\n", name);
    wii::os::OSReport("name2 %s\n", curDesc->destMapName);
    curDesc->destMapName = name;
    return 0;
    }
    return evt_door_set_dokan_descs(evtEntry, firstRun);
}

s32 new_evt_door_set_map_door_descs(spm::evtmgr::EvtEntry* evtEntry, bool firstRun)
{
    if (firstRun)
    {
    spm::evtmgr::EvtVar* args = (spm::evtmgr::EvtVar *)evtEntry->pCurData;
    spm::evt_door::MapDoorDesc* curDesc = (spm::evt_door::MapDoorDesc *)spm::evtmgr_cmd::evtGetValue(evtEntry, *args);
    s32 _mapGroup = spm::system::rand() % GROUP_COUNT;
    // thanks again to practice codes for already having error management for this
    if (msl::string::strcmp(groups[_mapGroup].name, "mac") == 0)
    {
        // mac is missing 10, 13, 20-21, 23-29
        switch (_mapGroup)
        {
            case 10:
            _mapGroup = 11;
                break;
            case 13:
            _mapGroup = 14;
                break;
            case 20:
            _mapGroup = 22;
                break;
            case 23:
            _mapGroup = 30;
                break;
        }
    }
    s32 roomGroup = spm::system::rand() % groups[_mapGroup].count;
    if (roomGroup == 0) roomGroup += 1;
    char nameT[32];
    char* name = (char*)spm::memory::__memAlloc(spm::memory::Heap::HEAP_MAP, sizeof(nameT));
    msl::stdio::sprintf(name, "%s_%02d", groups[_mapGroup].name, roomGroup);
    wii::os::OSReport("name %s\n", name);
    wii::os::OSReport("name2 %s\n", curDesc->destMapName);
    curDesc->destMapName = name;
    return 0;
    }
    return evt_door_set_map_door_descs(evtEntry, firstRun);
}

s32 new_evt_machi_set_elv_descs(spm::evtmgr::EvtEntry* evtEntry, bool firstRun)
{
    if (firstRun)
    {
    spm::evtmgr::EvtVar* args = (spm::evtmgr::EvtVar *)evtEntry->pCurData;
    spm::machi::ElvDesc* curDesc = (spm::machi::ElvDesc *)spm::evtmgr_cmd::evtGetValue(evtEntry, *args);
    s32 _mapGroup = spm::system::rand() % GROUP_COUNT;
    // thanks again to practice codes for already having error management for this
    if (msl::string::strcmp(groups[_mapGroup].name, "mac") == 0)
    {
        // mac is missing 10, 13, 20-21, 23-29
        switch (_mapGroup)
        {
            case 10:
            _mapGroup = 11;
                break;
            case 13:
            _mapGroup = 14;
                break;
            case 20:
            _mapGroup = 22;
                break;
            case 23:
            _mapGroup = 30;
                break;
        }
    }
    s32 roomGroup = spm::system::rand() % groups[_mapGroup].count;
    if (roomGroup == 0) roomGroup += 1;
    char nameT[32];
    char* name = (char*)spm::memory::__memAlloc(spm::memory::Heap::HEAP_MAP, sizeof(nameT));
    msl::stdio::sprintf(name, "%s_%02d", groups[_mapGroup].name, roomGroup);
    wii::os::OSReport("name %s\n", name);
    wii::os::OSReport("name2 %s\n", curDesc->destMapName);
    curDesc->destMapName = name;
    return 0;
    }
    return evt_machi_set_elv_descs(evtEntry, firstRun);
}

/*
    General mod functions
*/
void main()
{
    wii::os::OSReport("SPM Rel Loader: the mod has ran!\n");
    titleScreenCustomTextPatch();
    scanEntrances();
    evt_door_set_dokan_descs = patch::hookFunction(spm::evt_door::evt_door_set_dokan_descs, new_evt_door_set_dokan_descs);
    evt_door_set_map_door_descs = patch::hookFunction(spm::evt_door::evt_door_set_map_door_descs, new_evt_door_set_map_door_descs);
    evt_machi_set_elv_descs = patch::hookFunction(spm::machi::evt_machi_set_elv_descs, new_evt_machi_set_elv_descs);
}

}
