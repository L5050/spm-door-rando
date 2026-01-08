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
#include <wii/gx.h>

#include <msl/string.h>
#include <msl/stdio.h>

namespace mod {

/*
    =========================
    Title Screen Custom Text
    =========================
*/

static spm::seqdef::SeqFunc* seq_titleMainReal;

static void seq_titleMainOverride(spm::seqdrv::SeqWork* wp)
{
    wii::gx::GXColor green = {0, 255, 0, 255};
    const char* msg = "SPM-Door-Rando";
    f32 scale = 0.8f;

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
    seq_titleMainReal =
        spm::seqdef::seq_data[spm::seqdrv::SEQ_TITLE].main;
    spm::seqdef::seq_data[spm::seqdrv::SEQ_TITLE].main =
        &seq_titleMainOverride;
}

/*
    =========================
    Map Groups
    =========================
*/

struct EntranceNameList
{
    int count;
    const char* names[];
};

struct MapGroup
{
    char name[4];
    u16 firstId;
    u16 count;
    EntranceNameList** entranceNames;
};

static MapGroup groups[] = {
    /*{"dan", 11, 14, 0},*/ {"mac", 1, 30, 0},
    {"he1", 1, 6, 0}, {"he2", 1, 9, 0}, {"he3", 1, 8, 0}, {"he4", 1, 12, 0},
    {"mi1", 1, 11, 0}, {"mi2", 1, 11, 0}, {"mi3", 1, 6, 0}, {"mi4", 1, 15, 0},
    {"ta1", 1, 9, 0}, {"ta2", 1, 6, 0}, {"ta3", 1, 8, 0}, {"ta4", 1, 15, 0},
    {"sp1", 1, 7, 0}, {"sp2", 1, 10, 0}, {"sp3", 1, 7, 0}, {"sp4", 1, 17, 0},
    {"gn1", 1, 5, 0}, {"gn2", 1, 6, 0}, {"gn3", 1, 16, 0}, {"gn4", 1, 17, 0},
    {"wa1", 1, 27, 0}, {"wa2", 1, 25, 0}, {"wa3", 1, 25, 0}, {"wa4", 1, 26, 0},
    {"an1", 1, 11, 0}, {"an2", 1, 10, 0}, {"an3", 1, 16, 0}, {"an4", 1, 12, 0},
    {"ls1", 1, 12, 0}, {"ls2", 1, 18, 0}, {"ls3", 1, 13, 0}, {"ls4", 1, 12, 0},
};

#define GROUP_COUNT 33

/*
    =========================
    Persistent Door Mapping
    =========================
*/

#define MAX_RANDOMIZED_DOORS 512

struct DoorMapping
{
    char srcGroup[4];
    const char* entrance;
    const char* destMap;
};

static DoorMapping gDoorMap[MAX_RANDOMIZED_DOORS];
static int gDoorMapCount = 0;

/*
    =========================
    Current Map Group
    =========================
*/

static s32 gCurrentMapGroup = -1;

static void initCurrentMapGroup()
{
  if (gCurrentMapGroup != -1)
    return;

  gCurrentMapGroup = spm::system::rand() % GROUP_COUNT;

  if (msl::string::strcmp(groups[gCurrentMapGroup].name, "mac") == 0)
  {
    switch (gCurrentMapGroup)
    {
    case 10:
      gCurrentMapGroup = 11;
      break;
    case 13:
      gCurrentMapGroup = 14;
      break;
    case 20:
      gCurrentMapGroup = 22;
      break;
    case 23:
      gCurrentMapGroup = 30;
      break;
    }
  }

  wii::os::OSReport(
      "[DoorRando] Source group: %s\n",
      groups[gCurrentMapGroup].name);
}

/*
    =========================
    Door Mapping Helpers
    =========================
*/

static const char *findDoorMapping(
    const char *srcGroup,
    const char *entrance)
{
  for (int i = 0; i < gDoorMapCount; i++)
  {
    if (msl::string::strcmp(gDoorMap[i].srcGroup, srcGroup) == 0 &&
        msl::string::strcmp(gDoorMap[i].entrance, entrance) == 0)
    {
      return gDoorMap[i].destMap;
    }
  }
  return nullptr;
}

static const char *createDoorDestination()
{
  s32 destGroup = spm::system::rand() % GROUP_COUNT;

  if (msl::string::strcmp(groups[destGroup].name, "mac") == 0)
  {
    switch (destGroup)
    {
    case 10:
      destGroup = 11;
      break;
    case 13:
      destGroup = 14;
      break;
    case 20:
      destGroup = 22;
      break;
    case 23:
      destGroup = 30;
      break;
    }
  }

  s32 room = (spm::system::rand() % groups[destGroup].count) + 1;

  char buf[32];
  msl::stdio::sprintf(
      buf, "%s_%02d",
      groups[destGroup].name, room);

  char *dest = (char *)spm::memory::__memAlloc(
      spm::memory::Heap::HEAP_MAP,
      msl::string::strlen(buf) + 1);
  msl::string::strcpy(dest, buf);

  return dest;
}

static const char *getOrCreateDestination(const char *entrance)
{
  initCurrentMapGroup();

  const char *srcGroup = groups[gCurrentMapGroup].name;

  const char *existing =
      findDoorMapping(srcGroup, entrance);

  if (existing)
    return existing;

  const char *dest = createDoorDestination();

  if (gDoorMapCount < MAX_RANDOMIZED_DOORS)
  {
    DoorMapping *m = &gDoorMap[gDoorMapCount++];
    msl::string::strcpy(m->srcGroup, srcGroup);
    m->entrance = entrance;
    m->destMap = dest;

    wii::os::OSReport(
        "[DoorRando] %s:%s -> %s\n",
        srcGroup, entrance, dest);
  }

  return dest;
}

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

/*
    =========================
    Hooked Event Functions
    =========================
*/

s32 (*evt_door_set_dokan_descs)(spm::evtmgr::EvtEntry*, bool);
s32 (*evt_door_set_map_door_descs)(spm::evtmgr::EvtEntry*, bool);
s32 (*evt_machi_set_elv_descs)(spm::evtmgr::EvtEntry*, bool);

s32 new_evt_door_set_dokan_descs(
    spm::evtmgr::EvtEntry* evtEntry, bool firstRun)
{
    if (firstRun)
    {
        auto* args = (spm::evtmgr::EvtVar*)evtEntry->pCurData;
        auto* curDesc = (spm::evt_door::DokanDesc*) spm::evtmgr_cmd::evtGetValue(evtEntry, *args);
         wii::os::OSReport("Dest: %s\n", curDesc->destDoorName);

        curDesc->destMapName = getOrCreateDestination(curDesc->name);
        return 0;
    }
    return evt_door_set_dokan_descs(evtEntry, firstRun);
}

s32 new_evt_door_set_map_door_descs(
    spm::evtmgr::EvtEntry* evtEntry, bool firstRun)
{
    if (firstRun)
    {
        auto* args = (spm::evtmgr::EvtVar*)evtEntry->pCurData;
        auto* curDesc = (spm::evt_door::MapDoorDesc*) spm::evtmgr_cmd::evtGetValue(evtEntry, *args);

        curDesc->destMapName = getOrCreateDestination(curDesc->name_l);
        return 0;
    }
    return evt_door_set_map_door_descs(evtEntry, firstRun);
}

s32 new_evt_machi_set_elv_descs(
    spm::evtmgr::EvtEntry* evtEntry, bool firstRun)
{
    if (firstRun)
    {
        auto* args = (spm::evtmgr::EvtVar*)evtEntry->pCurData;
        auto* curDesc = (spm::machi::ElvDesc*)
            spm::evtmgr_cmd::evtGetValue(evtEntry, *args);

        curDesc->destMapName =
            getOrCreateDestination(curDesc->name);
        return 0;
    }
    return evt_machi_set_elv_descs(evtEntry, firstRun);
}

/*
    =========================
    Mod Entry Point
    =========================
*/

void main()
{
    wii::os::OSReport("SPM Door Rando loaded!\n");

    titleScreenCustomTextPatch();
    scanEntrances();
    evt_door_set_dokan_descs =
        patch::hookFunction(
            spm::evt_door::evt_door_set_dokan_descs,
            new_evt_door_set_dokan_descs);

    evt_door_set_map_door_descs =
        patch::hookFunction(
            spm::evt_door::evt_door_set_map_door_descs,
            new_evt_door_set_map_door_descs);

    evt_machi_set_elv_descs =
        patch::hookFunction(
            spm::machi::evt_machi_set_elv_descs,
            new_evt_machi_set_elv_descs);
}

} // namespace mod
