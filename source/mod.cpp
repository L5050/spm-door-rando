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
 Title Screen Text
=========================
*/

static spm::seqdef::SeqFunc* seq_titleMainReal;

static void seq_titleMainOverride(spm::seqdrv::SeqWork* wp)
{
    wii::gx::GXColor green = {0,255,0,255};
    const char* msg = "SPM Door Rando";
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
    {"mac",1,30,0},
    {"he1",1,6,0},{"he2",1,9,0},{"he3",1,8,0},{"he4",1,12,0},
    {"mi1",1,11,0},{"mi2",1,11,0},{"mi3",1,6,0},{"mi4",1,15,0},
    {"ta1",1,9,0},{"ta2",1,6,0},{"ta3",1,8,0},{"ta4",1,15,0},
    /*{"sp1",1,7,0},*/{"sp2",1,10,0},/*{"sp3",1,7,0},*/{"sp4",1,17,0},
    {"gn1",1,5,0},{"gn2",1,6,0},{"gn3",1,16,0},{"gn4",1,17,0},
    {"wa1",1,27,0},{"wa2",1,25,0},{"wa3",1,25,0},{"wa4",1,26,0},
    {"an1",1,11,0},{"an2",1,10,0},{"an3",1,16,0},{"an4",1,12,0},
    {"ls1",1,12,0},{"ls2",1,18,0},{"ls3",1,13,0},{"ls4",1,12,0},
};

#define GROUP_COUNT 31

/*
=========================
 Persistent Door Mapping
=========================
*/

#define MAX_RANDOMIZED_DOORS 512

struct DoorMapping
{
    char sourceMap[32];     // e.g. "he1_03"
    char entranceName[32];  // e.g. "door_a"
    const char* destMapName;
    const char* destDoorName;
};

static DoorMapping gDoorMappings[MAX_RANDOMIZED_DOORS];
static int gDoorMappingCount = 0;


/*
=========================
 Current Source Group
=========================
*/

static s32 gCurrentMapGroup = -1;

static void initCurrentMapGroup()
{
    if (gCurrentMapGroup != -1)
        return;

    gCurrentMapGroup = spm::system::rand() % GROUP_COUNT;

    wii::os::OSReport(
        "[DoorRando] Source group: %s\n",
        groups[gCurrentMapGroup].name
    );
}

/*
=========================
 Helpers
=========================
*/

static DoorMapping* findDoorMapping(
    const char* sourceMap,
    const char* entranceName)
{
    for (int i = 0; i < gDoorMappingCount; i++)
    {
        if (msl::string::strcmp(
                gDoorMappings[i].sourceMap, sourceMap) == 0 &&
            msl::string::strcmp(
                gDoorMappings[i].entranceName, entranceName) == 0)
        {
            return &gDoorMappings[i];
        }
    }
    return nullptr;
}

static const char* allocateMapString(const char* text)
{
    char* out = (char*)spm::memory::__memAlloc(
        spm::memory::Heap::HEAP_MAIN,
        msl::string::strlen(text) + 1);
    msl::string::strcpy(out, text);
    return out;
}

static const char* createRandomDestinationMap()
{
    int groupIndex = spm::system::rand() % GROUP_COUNT;
    int roomIndex  = (spm::system::rand() % groups[groupIndex].count) + 1;

    char buffer[32];
    msl::stdio::sprintf(
        buffer, "%s_%02d",
        groups[groupIndex].name, roomIndex);
    
    return allocateMapString(buffer);
}

static const char* pickRandomDestinationDoor(const char* destinationMap)
{
    int groupIndex = -1;
    int roomIndex  = -1;

    for (int i = 0; i < GROUP_COUNT; i++)
    {
        if (msl::string::strncmp(
                groups[i].name, destinationMap, 3) == 0)
        {
            groupIndex = i;
            roomIndex =
                (destinationMap[4] - '0') * 10 +
                (destinationMap[5] - '0') - 1;
            break;
        }
    }

    if (groupIndex < 0 || roomIndex < 0)
        return nullptr;

    EntranceNameList* entranceList =
        groups[groupIndex].entranceNames[roomIndex];

    if (!entranceList || entranceList->count == 0)
        return nullptr;

    return entranceList->names[
        spm::system::rand() % entranceList->count];
}

static DoorMapping* getOrCreateDoorMapping(
    const char* sourceMap,
    const char* entranceName)
{
    DoorMapping* mapping =
        findDoorMapping(sourceMap, entranceName);

    if (mapping)
        return mapping;

    if (gDoorMappingCount >= MAX_RANDOMIZED_DOORS)
        return nullptr;

    mapping = &gDoorMappings[gDoorMappingCount++];

    msl::string::strcpy(mapping->sourceMap, sourceMap);
    msl::string::strcpy(mapping->entranceName, entranceName);

    mapping->destMapName  = createRandomDestinationMap();
    mapping->destDoorName = nullptr;

    return mapping;
}


/*
=========================
 Script Scanning
=========================
*/

static EntranceNameList* scanScript(const int* script)
{
    if (!script)
    {
        EntranceNameList* l =
            (EntranceNameList*)new int[1];
        l->count = 0;
        return l;
    }

    spm::evt_door::DokanDesc* d = nullptr; int dc = 0;
    spm::evt_door::MapDoorDesc* m = nullptr; int mc = 0;
    spm::machi::ElvDesc* e = nullptr; int ec = 0;

    #define OMAX 15
    char* others[OMAX];
    int oc = 0;

    int cmd = 0;
    while (cmd != 1)
    {
        const short* p = (const short*)script;
        int cmdn = p[0];
        cmd = p[1];

        if (cmd == 0x5c)
        {
            u32 fn = script[1];
            if (fn == (u32)spm::evt_door::evt_door_set_dokan_descs)
                { d=(decltype(d))script[2]; dc=script[3]; }
            else if (fn == (u32)spm::evt_door::evt_door_set_map_door_descs)
                { m=(decltype(m))script[2]; mc=script[3]; }
            else if (fn == (u32)spm::machi::evt_machi_set_elv_descs)
                { e=(decltype(e))script[2]; ec=script[3]; }
        }

        script += cmdn + 1;
    }

    int total = dc + mc + ec + oc;
    EntranceNameList* list =
        (EntranceNameList*)new int[total + 1];
    list->count = total;

    int n = 0;
    for (int i=0;i<dc;i++) list->names[n++] = d[i].name;
    for (int i=0;i<mc;i++) list->names[n++] = m[i].name_l;
    for (int i=0;i<ec;i++) list->names[n++] = e[i].name;

    return list;
}

static void scanEntrances()
{
    for (u32 i = 0; i < GROUP_COUNT; i++)
    {
        groups[i].entranceNames =
            new EntranceNameList*[groups[i].count];

        for (int j = 0; j < groups[i].count; j++)
        {
            char name[32];
            msl::stdio::sprintf(
                name, "%s_%02d",
                groups[i].name, j+1
            );

            spm::map_data::MapData* md =
                spm::map_data::mapDataPtr(name);

            groups[i].entranceNames[j] =
                scanScript(md ? (int*)md->initScript : nullptr);
        }
    }
}

/*
=========================
 Hooks
=========================
*/

s32 (*evt_door_set_dokan_descs)(spm::evtmgr::EvtEntry*, bool);
s32 (*evt_door_set_map_door_descs)(spm::evtmgr::EvtEntry*, bool);
s32 (*evt_machi_set_elv_descs)(spm::evtmgr::EvtEntry*, bool);

s32 new_evt_door_set_dokan_descs(
    spm::evtmgr::EvtEntry* evtEntry, bool firstRun)
{
    if (!firstRun)
        return evt_door_set_dokan_descs(evtEntry, firstRun);

    spm::evtmgr::EvtVar* args =
        (spm::evtmgr::EvtVar*)evtEntry->pCurData;

    spm::evt_door::DokanDesc* desc =
        (spm::evt_door::DokanDesc*)
            spm::evtmgr_cmd::evtGetValue(evtEntry, *args);

    const char* sourceMapName = desc->mapName;

    DoorMapping* mapping =
        getOrCreateDoorMapping(sourceMapName, desc->name);

    if (mapping)
    {
        if (!mapping->destDoorName)
        {
            mapping->destDoorName =
                pickRandomDestinationDoor(mapping->destMapName);
        }

        desc->destMapName  = mapping->destMapName;
        desc->destDoorName = mapping->destDoorName;
    }

    return 0;
}

s32 new_evt_door_set_map_door_descs(spm::evtmgr::EvtEntry *evtEntry, bool firstRun)
{
  if (!firstRun)
    return evt_door_set_map_door_descs(evtEntry, firstRun);

  spm::evtmgr::EvtVar *args =
      (spm::evtmgr::EvtVar *)evtEntry->pCurData;

  spm::evt_door::MapDoorDesc *mapDoorDesc =
      (spm::evt_door::MapDoorDesc *)
          spm::evtmgr_cmd::evtGetValue(evtEntry, *args);

  const char * sourceMapName = spm::spmario::gp->mapName;
  wii::os::OSReport("sourceMapName %s\n", sourceMapName);

  DoorMapping *mapping =
      getOrCreateDoorMapping(sourceMapName, mapDoorDesc->name_l);

  if (mapping && !mapping->destDoorName)
  {
    mapping->destDoorName =
        pickRandomDestinationDoor(mapping->destMapName);
  }

  if (mapping)
  {
    wii::os::OSReport("destMapName %s\n", mapping->destMapName);
    wii::os::OSReport("destDoorName %s\n", mapping->destDoorName);
    mapDoorDesc->destMapName = mapping->destMapName;
    mapDoorDesc->destDoorName = mapping->destDoorName;
  }

  return 0;
}

s32 new_evt_machi_set_elv_descs(
    spm::evtmgr::EvtEntry* evtEntry, bool firstRun)
{
    if (!firstRun)
        return evt_machi_set_elv_descs(evtEntry, firstRun);

    spm::evtmgr::EvtVar* args =
        (spm::evtmgr::EvtVar*)evtEntry->pCurData;

    spm::machi::ElvDesc* elvDesc =
        (spm::machi::ElvDesc*)
            spm::evtmgr_cmd::evtGetValue(evtEntry, *args);

    const char * sourceMapName = spm::spmario::gp->mapName;

    DoorMapping* mapping =
        getOrCreateDoorMapping(sourceMapName, elvDesc->name);

    if (mapping && !mapping->destDoorName)
    {
        mapping->destDoorName =
            pickRandomDestinationDoor(mapping->destMapName);
    }

    if (mapping)
    {
        elvDesc->destMapName  = mapping->destMapName;
        elvDesc->destDoorName = mapping->destDoorName;
    }

    return 0;
}


/*
=========================
 Entry Point
=========================
*/

void main()
{
    wii::os::OSReport("SPM Door Rando loaded\n");

    titleScreenCustomTextPatch();
    scanEntrances();

    evt_door_set_dokan_descs = patch::hookFunction(spm::evt_door::evt_door_set_dokan_descs, new_evt_door_set_dokan_descs);

    evt_door_set_map_door_descs = patch::hookFunction(spm::evt_door::evt_door_set_map_door_descs, new_evt_door_set_map_door_descs);

    evt_machi_set_elv_descs = patch::hookFunction(spm::machi::evt_machi_set_elv_descs, new_evt_machi_set_elv_descs);
}

}
