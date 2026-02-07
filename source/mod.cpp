#include "mod.h"
#include "evtpatch.h"
#include "patch.h"
#include "util.h"

#include <spm/rel/machi.h>
#include <spm/rel/aa1_01.h>
#include <spm/camdrv.h>
#include <spm/evt_door.h>
#include <spm/evt_sub.h>
#include <spm/fontmgr.h>
#include <spm/mapdrv.h>
#include <spm/map_data.h>
#include <spm/evt_msg.h> 
#include <spm/evt_fairy.h> 
#include <spm/evt_guide.h> 
#include <spm/evt_snd.h> 
#include <spm/evt_fade.h> 
#include <spm/evt_npc.h> 
#include <spm/evt_mario.h>
#include <spm/evt_seq.h> 
#include <spm/mario_pouch.h>
#include <spm/evt_pouch.h>
#include <spm/pausewin.h>
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

  spm::npcdrv::NPCTribeAnimDef animsQueen[] = {
    {0, "Z_1"},
    {1, "S_1"},
    {-1, nullptr}
  };

  const char * queenText = "<majo><col ffddddff><shake><scale 0.7>\n"
  "Only those with the power of the\n"
  "ancients may enter the castle\n"
  "of the tribe of darkness...\n"
  "<k>\n";

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
    {"wa1",1,2,0},//{"wa2",1,25,0},{"wa3",1,25,0},{"wa4",1,26,0},
    {"an1",1,11,0},{"an2",1,10,0},{"an3",1,16,0},{"an4",1,12,0},
    {"ls1",1,1,0},//{"ls2",1,18,0},{"ls3",1,13,0},{"ls4",1,12,0},
};

#define GROUP_COUNT 25

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
static u32 gDoorMappingCount = 0;
char mapNameBuffer[32];


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

static DoorMapping *findDoorMapping(
    const char *sourceMap,
    const char *entranceName)
{
  for (int i = 0; i < gDoorMappingCount; i++)
  {
    if (msl::string::strcmp(gDoorMappings[i].sourceMap, sourceMap) == 0 && msl::string::strcmp(gDoorMappings[i].entranceName, entranceName) == 0)
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
    lab_roomRando:
    int roomIndex  = (spm::system::rand() % groups[groupIndex].count) + 1;

    if (roomIndex > groups[groupIndex].count)
    {
      roomIndex = groups[groupIndex].count;
    }
    if (msl::string::strcmp(groups[groupIndex].name, "mac") == 0)
    {
      if (roomIndex == 10 || roomIndex == 13 || roomIndex > 19 && roomIndex != 30)
      {
        goto lab_roomRando;
      }
    }
    char buffer[32];
    msl::stdio::sprintf(buffer, "%s_%02d", groups[groupIndex].name, roomIndex);
    msl::string::strcpy(mapNameBuffer, buffer);
    return mapNameBuffer;
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
    if (msl::string::strcmp(destinationMap, "ls1_01") == 0)
    {
      return "doa1_l";
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

static bool destinationUsed(const char* map, const char* door)
{
    for (u32 i = 0; i < gDoorMappingCount; i++)
    {
        if (msl::string::strcmp(gDoorMappings[i].destMapName, map) == 0 &&
            msl::string::strcmp(gDoorMappings[i].destDoorName, door) == 0)
        {
            return true;
        }
    }
    return false;
}

static DoorMapping* getOrCreateDoorMapping(
    const char* sourceMap,
    const char* entranceName)
{
    if (!entranceName)
    {
      entranceName = "default";
    }
    DoorMapping* mapping = findDoorMapping(sourceMap, entranceName);

    if (mapping)
    {
      return mapping;
    }

    if (gDoorMappingCount >= MAX_RANDOMIZED_DOORS)
    {
      return nullptr;
    }

    mapping = &gDoorMappings[gDoorMappingCount];

    msl::string::strcpy(mapping->sourceMap, sourceMap);
    msl::string::strcpy(mapping->entranceName, entranceName);

    do
    {
      mapping->destMapName = createRandomDestinationMap();
      mapping->destDoorName = pickRandomDestinationDoor(mapping->destMapName);
      wii::os::OSReport("destMapName %s\n", mapping->destMapName);
      wii::os::OSReport("destDoorName %s\n", mapping->destDoorName);
    } while (!mapping->destMapName || !mapping->destDoorName || destinationUsed(mapping->destMapName, mapping->destDoorName));
    
    mapping->destMapName = allocateMapString(mapNameBuffer);
    gDoorMappingCount++;
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

    if (!sourceMapName)
    {
      return evt_door_set_dokan_descs(evtEntry, firstRun);
    }

    if (msl::string::strstr(spm::spmario::gp->mapName, "ls") != 0)
    {
      return evt_door_set_dokan_descs(evtEntry, firstRun);
    }

    DoorMapping* mapping =
        getOrCreateDoorMapping(sourceMapName, desc->name);

    if (mapping)
    {
        if (!mapping->destDoorName)
        {
            mapping->destDoorName = pickRandomDestinationDoor(mapping->destMapName);
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

  if (!sourceMapName)
  {
    return evt_door_set_map_door_descs(evtEntry, firstRun);
  }

  if (msl::string::strstr(spm::spmario::gp->mapName, "ls") != 0)
  {
    return evt_door_set_map_door_descs(evtEntry, firstRun);
  }

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

s32 new_evt_machi_set_elv_descs(spm::evtmgr::EvtEntry* evtEntry, bool firstRun)
{
    if (!firstRun)
        return evt_machi_set_elv_descs(evtEntry, firstRun);

    if (msl::string::strstr(spm::spmario::gp->mapName, "ls") != 0)
    {
      return evt_machi_set_elv_descs(evtEntry, firstRun);
    }

    spm::evtmgr::EvtVar* args =
        (spm::evtmgr::EvtVar*)evtEntry->pCurData;

    spm::machi::ElvDesc* elvDesc =
        (spm::machi::ElvDesc*)
            spm::evtmgr_cmd::evtGetValue(evtEntry, *args);

    const char * sourceMapName = spm::spmario::gp->mapName;

    if (!sourceMapName)
    {
      return evt_machi_set_elv_descs(evtEntry, firstRun);
    }

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

    // Dialogue to determine quickstart or no
    EVT_BEGIN(determine_quickstart)
    SET(GSW(0), 17)
    SET(GSWF(2), 1)
    SET(GSWF(9), 1)
    SET(GSWF(12), 1)
    SET(GSWF(386), 1)
    SET(GSWF(387), 1)
    SET(GSWF(392), 1)
    SET(GSWF(393), 1)
    SET(GSWF(394), 1)
    SET(GSWF(395), 1)
    SET(GSWF(396), 1)
    SET(GSWF(397), 1)
    SET(GSWF(398), 1)
    SET(GSWF(399), 1) 
    SET(GSWF(420), 1) 
    SET(GSWF(431), 1)
    SET(GSWF(810), 1) 
    USER_FUNC(spm::evt_pouch::evt_pouch_add_item, 50)
    USER_FUNC(spm::evt_pouch::evt_pouch_add_item, 0x0E5)
    USER_FUNC(spm::evt_pouch::evt_pouch_add_item, 0xE7)
    USER_FUNC(spm::evt_pouch::evt_pouch_add_item, 0x0D9)
    USER_FUNC(spm::evt_pouch::evt_pouch_add_item, 0x0DA)
    USER_FUNC(spm::evt_pouch::evt_pouch_add_item, 0x0DB)
    //USER_FUNC(spm::evt_pouch::evt_pouch_add_item, 0x0E0)
    //USER_FUNC(spm::evt_msg::evt_msg_print, 1, PTR(quickstartText), 0, 0)
    //USER_FUNC(spm::evt_msg::evt_msg_select, 1, PTR(quickstartOptions))
    //USER_FUNC(spm::evt_msg::evt_msg_continue)
    //SWITCH(LW(0))
    //END_SWITCH()
    USER_FUNC(spm::evt_seq::evt_seq_set_seq, spm::seqdrv::SEQ_MAPCHANGE, PTR("he1_01"), PTR("doa1_l"))
    RETURN()
    EVT_END()
  
  EVT_BEGIN(return_to_flipside)
    USER_FUNC(spm::evt_mario::evt_mario_key_off, 0)
    WAIT_FRM(5)
    USER_FUNC(spm::evt_mario::evt_mario_set_pose, PTR("D_5"), 0)
    WAIT_FRM(5)
    USER_FUNC(spm::evt_snd::evt_snd_sfxon, PTR("SFX_BS_ZIGEN_HOLE1"))
    USER_FUNC(spm::evt_npc::evt_npc_entry, PTR("queen"), PTR("MOBJ_EFF_queen_tornade"), 0)
    USER_FUNC(spm::evt_npc::evt_npc_set_position, PTR("queen"), LW(0), FLOAT(0.0), LW(2))
    USER_FUNC(spm::evt_npc::evt_npc_set_property, PTR("queen"), 14, (s32)animsQueen)
    USER_FUNC(spm::evt_npc::evt_npc_set_anim, PTR("queen"), 1, 1)
    USER_FUNC(spm::evt_msg::evt_msg_print, 1, PTR(queenText), 0, 0)
    USER_FUNC(spm::evt_snd::evt_snd_sfxon, PTR("SFX_E_MISS_SMASH1"))
    WAIT_MSEC(1000)
    USER_FUNC(spm::evt_fade::evt_set_transition, 4, 3)
    USER_FUNC(spm::evt_seq::evt_seq_set_seq, spm::seqdrv::SEQ_MAPCHANGE, PTR("mac_02"), PTR("dokan_3"))
  EVT_END()

  EVT_BEGIN(ls1_check_pos)
  DO(0)
    USER_FUNC(spm::evt_mario::evt_mario_get_pos, LW(0), LW(1), LW(2))
    IFF_LARGE_EQUAL(LW(0), FLOAT(0.0))
      DO_BREAK()
    END_IF()
    WAIT_FRM(1)
  WHILE()
  SET(LW(4), 0)
  USER_FUNC(spm::evt_pouch::evt_pouch_check_have_item, 0x0DD, LW(3))
  IF_EQUAL(LW(3), 1)
    ADD(LW(4), 1)
  END_IF()
  USER_FUNC(spm::evt_pouch::evt_pouch_check_have_item, 0x0DE, LW(3))
  IF_EQUAL(LW(3), 1)
    ADD(LW(4), 1)
  END_IF()
  USER_FUNC(spm::evt_pouch::evt_pouch_check_have_item, 0x0DF, LW(3))
  IF_EQUAL(LW(3), 1)
    ADD(LW(4), 1)
  END_IF()
  USER_FUNC(spm::evt_pouch::evt_pouch_check_have_item, 0x0E0, LW(3))
  IF_EQUAL(LW(3), 1)
    ADD(LW(4), 1)
  END_IF()
  USER_FUNC(spm::evt_pouch::evt_pouch_check_have_item, 0x0E1, LW(3))
  IF_EQUAL(LW(3), 1)
    ADD(LW(4), 1)
  END_IF()
  USER_FUNC(spm::evt_pouch::evt_pouch_check_have_item, 0x0E2, LW(3))
  IF_EQUAL(LW(3), 1)
    ADD(LW(4), 1)
  END_IF()
  USER_FUNC(spm::evt_pouch::evt_pouch_check_have_item, 0x0E3, LW(3))
  IF_EQUAL(LW(3), 1)
    ADD(LW(4), 1)
  END_IF()
  USER_FUNC(spm::evt_pouch::evt_pouch_check_have_item, 0x0E4, LW(3))
  IF_EQUAL(LW(3), 1)
    ADD(LW(4), 1)
  END_IF()
  IF_EQUAL(LW(4), 8)
    RETURN()
  ELSE()
    RUN_EVT(return_to_flipside)
  END_IF()
  RETURN()
  EVT_END()

  EVT_BEGIN(insertNop)
    SET(LW(0), LW(0))
  RETURN_FROM_CALL()
  
  EVT_BEGIN(gn4)
    SET(GSW(0), 208)
  RETURN_FROM_CALL()
  
  EVT_BEGIN(gn2)
    SET(GSW(0), 189)
  RETURN_FROM_CALL()
  
  EVT_BEGIN(sp2)
    SET(GSW(0), 142)
  RETURN_FROM_CALL()
  
  EVT_BEGIN(ta2)
    SET(GSW(0), 107)
  RETURN_FROM_CALL()
  
  EVT_BEGIN(ta4)
    SET(GSW(0), 120)
  RETURN_FROM_CALL()
  
  EVT_BEGIN(ls1)
    USER_FUNC(spm::evt_guide::evt_guide_flag0_onoff, 0, 0x80)
    SET(GSW(0), 358)
    USER_FUNC(spm::evt_npc::evt_npc_entry, PTR("queen"), PTR("MOBJ_EFF_queen_tornade"), 0)
    USER_FUNC(spm::evt_npc::evt_npc_delete, PTR("queen"))
    RUN_EVT(ls1_check_pos)
  RETURN_FROM_CALL()

  EVT_BEGIN(ls4)
    USER_FUNC(spm::evt_guide::evt_guide_flag0_onoff, 0, 0x80)
  RETURN_FROM_CALL()

  EVT_BEGIN(he1)
    SET(GSW(0), 17)
  RETURN_FROM_CALL()

  EVT_BEGIN(he2_mi1)
    SET(GSW(0), 20)
  RETURN_FROM_CALL()

  EVT_BEGIN(mac_02)
    SET(GSW(0), 359)
  RETURN_FROM_CALL()

  EVT_BEGIN(an1_02)
    SET(GSW(0), 359)
  RETURN_FROM_CALL()

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
    evtpatch::evtmgrExtensionInit();
    evt_door_set_dokan_descs = patch::hookFunction(spm::evt_door::evt_door_set_dokan_descs, new_evt_door_set_dokan_descs);

    evt_door_set_map_door_descs = patch::hookFunction(spm::evt_door::evt_door_set_map_door_descs, new_evt_door_set_map_door_descs);

    evt_machi_set_elv_descs = patch::hookFunction(spm::machi::evt_machi_set_elv_descs, new_evt_machi_set_elv_descs);

    writeWord(spm::mario_pouch::pouchMakePixlNotSelectable, 0x0, BLR);
    writeWord(spm::mario_pouch::pouchMakeCharNotSelectable, 0x0, BLR); 
    writeWord(spm::pausewin::pluswinKeyItemMain, 0x664, NOP);
    writeWord(spm::pausewin::pluswinKeyItemMain, 0x674, NOP);
    writeWord(spm::pausewin::pluswinKeyItemMain, 0x680, NOP);
    spm::map_data::MapData * ls1_md = spm::map_data::mapDataPtr("ls1_01");
    spm::map_data::MapData * ls4_md = spm::map_data::mapDataPtr("ls4_02");
    evtpatch::hookEvtReplace(spm::aa1_01::aa1_01_mario_house_transition_evt, 10, determine_quickstart);
    spm::map_data::MapData * he2_md = spm::map_data::mapDataPtr("he2_07");
    spm::map_data::MapData * mi1_md = spm::map_data::mapDataPtr("mi1_07");
    spm::map_data::MapData * ta2_md = spm::map_data::mapDataPtr("ta2_04");
    spm::map_data::MapData * ta4_md = spm::map_data::mapDataPtr("ta4_12");
    spm::map_data::MapData * sp2_md = spm::map_data::mapDataPtr("sp2_01");
    spm::map_data::MapData * sp2_08_md = spm::map_data::mapDataPtr("sp2_08");
    spm::map_data::MapData * gn1_md = spm::map_data::mapDataPtr("gn1_01");
    spm::map_data::MapData * gn2_md = spm::map_data::mapDataPtr("gn2_02");
    spm::map_data::MapData * gn4_md = spm::map_data::mapDataPtr("gn4_03");
    spm::map_data::MapData * an1_02_md = spm::map_data::mapDataPtr("an1_02");
    spm::map_data::MapData * mac_02_md = spm::map_data::mapDataPtr("mac_02");
    spm::map_data::MapData * ta4_13_md = spm::map_data::mapDataPtr("ta4_13");

    evtpatch::hookEvtReplace(ls1_md->initScript, 1, ls1);
    evtpatch::hookEvtReplace(he2_md->initScript, 1, he2_mi1);
    evtpatch::hookEvtReplace(mi1_md->initScript, 1, he2_mi1);
    evtpatch::hookEvtReplace(ta2_md->initScript, 1, ta2);
    evtpatch::hookEvtReplace(ta4_md->initScript, 1, ta4);
    evtpatch::hookEvtReplace(sp2_md->initScript, 1, sp2);
    evtpatch::hookEvtReplace(sp2_08_md->initScript, 1, sp2);
    evtpatch::hookEvtReplace(gn1_md->initScript, 1, gn4);
    evtpatch::hookEvtReplace(gn2_md->initScript, 1, gn2);
    evtpatch::hookEvtReplace(gn4_md->initScript, 1, gn4);
    evtpatch::hookEvtReplace(gn4_md->initScript, 1, gn4);
    evtpatch::hookEvtReplace(an1_02_md->initScript, 1, an1_02);
    evtpatch::hookEvtReplace(mac_02_md->initScript, 1, mac_02);
    evtpatch::hookEvtReplace(ta4_13_md->initScript, 1, an1_02);
    evtpatch::hookEvtReplace(ls4_md->initScript, 1, ls4);
}

}
