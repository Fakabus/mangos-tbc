/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _PLAYER_H
#define _PLAYER_H

#include "Common.h"
#include "Entities/ItemPrototype.h"
#include "Entities/Unit.h"
#include "Entities/Item.h"

#include "Database/DatabaseEnv.h"
#include "Quests/QuestDef.h"
#include "Groups/Group.h"
#include "Entities/Bag.h"
#include "Server/WorldSession.h"
#include "Entities/Pet.h"
#include "Maps/MapReference.h"
#include "Util.h"                                           // for Tokens typedef
#include "Reputation/ReputationMgr.h"
#include "BattleGround/BattleGround.h"
#include "Server/DBCStores.h"
#include "Globals/SharedDefines.h"
#include "Chat/Chat.h"
#include "Server/SQLStorages.h"
#include "Loot/LootMgr.h"

#include<vector>

struct Mail;
class Channel;
class DynamicObject;
class Creature;
class PlayerMenu;
class Transport;
class UpdateMask;
class SpellCastTargets;
class PlayerSocial;
class DungeonPersistentState;
class Spell;
class Item;
struct FactionTemplateEntry;

#ifdef BUILD_PLAYERBOT
#include "PlayerBot/Base/PlayerbotMgr.h"
#include "PlayerBot/Base/PlayerbotAI.h"
#endif

struct AreaTrigger;

typedef std::deque<Mail*> PlayerMails;

#define PLAYER_MAX_SKILLS           127
#define PLAYER_MAX_DAILY_QUESTS     25
#define PLAYER_EXPLORED_ZONES_SIZE  128

// TODO: Maybe this can be implemented in configuration file.
#define PLAYER_NEW_INSTANCE_LIMIT_PER_HOUR 5

// Note: SPELLMOD_* values is aura types in fact
enum SpellModType
{
    SPELLMOD_FLAT               = 107,                      // SPELL_AURA_ADD_FLAT_MODIFIER
    SPELLMOD_PCT                = 108                       // SPELL_AURA_ADD_PCT_MODIFIER
};

// 2^n internal values, they are never sent to the client
enum PlayerUnderwaterState
{
    UNDERWATER_NONE             = 0x00,
    UNDERWATER_INWATER          = 0x01,                     // terrain type is water and player is afflicted by it
    UNDERWATER_INLAVA           = 0x02,                     // terrain type is lava and player is afflicted by it
    UNDERWATER_INSLIME          = 0x04,                     // terrain type is lava and player is afflicted by it
    UNDERWATER_INDARKWATER      = 0x08,                     // terrain type is dark water and player is afflicted by it

    UNDERWATER_EXIST_TIMERS     = 0x10
};

enum BuyBankSlotResult
{
    ERR_BANKSLOT_FAILED_TOO_MANY    = 0,
    ERR_BANKSLOT_INSUFFICIENT_FUNDS = 1,
    ERR_BANKSLOT_NOTBANKER          = 2,
    ERR_BANKSLOT_OK                 = 3
};

enum PlayerSpellState
{
    PLAYERSPELL_UNCHANGED       = 0,
    PLAYERSPELL_CHANGED         = 1,
    PLAYERSPELL_NEW             = 2,
    PLAYERSPELL_REMOVED         = 3
};

struct PlayerSpell
{
    PlayerSpellState state : 8;
    bool active            : 1;                             // show in spellbook
    bool dependent         : 1;                             // learned as result another spell learn, skill grow, quest reward, etc
    bool disabled          : 1;                             // first rank has been learned in result talent learn but currently talent unlearned, save max learned ranks
};

typedef std::unordered_map<uint32, PlayerSpell> PlayerSpellMap;

// Spell modifier (used for modify other spells)
struct SpellModifier
{
    SpellModifier() : charges(0), lastAffected(nullptr) {}

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, uint32 _spellId, uint64 _mask, int16 _charges = 0)
        : op(_op), type(_type), charges(_charges), value(_value), mask(_mask), spellId(_spellId), lastAffected(nullptr)
    {}

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, uint32 _spellId, ClassFamilyMask _mask, int16 _charges = 0)
        : op(_op), type(_type), charges(_charges), value(_value), mask(_mask), spellId(_spellId), lastAffected(nullptr)
    {}

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, SpellEntry const* spellEntry, SpellEffectIndex eff, int16 _charges = 0);

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, Aura const* aura, int16 _charges = 0);

    bool isAffectedOnSpell(SpellEntry const* spell) const;

    SpellModOp   op   : 8;
    SpellModType type : 8;
    int16 charges     : 16;
    int32 value;
    ClassFamilyMask mask;
    uint32 spellId;
    Spell const* lastAffected;                              // mark last charge user, used for cleanup delayed remove spellmods at spell success or restore charges at cast fail (Is one pointer only need for cases mixed castes?)
};

typedef std::list<SpellModifier*> SpellModList;

struct SpellCooldown
{
    time_t end;
    uint16 itemid;
};

typedef std::map<uint32, SpellCooldown> SpellCooldowns;

enum TrainerSpellState
{
    TRAINER_SPELL_GREEN         = 0,
    TRAINER_SPELL_RED           = 1,
    TRAINER_SPELL_GRAY          = 2,
    TRAINER_SPELL_GREEN_DISABLED = 10                       // custom value, not send to client: formally green but learn not allowed
};

enum ActionButtonUpdateState
{
    ACTIONBUTTON_UNCHANGED      = 0,
    ACTIONBUTTON_CHANGED        = 1,
    ACTIONBUTTON_NEW            = 2,
    ACTIONBUTTON_DELETED        = 3
};

enum ActionButtonType
{
    ACTION_BUTTON_SPELL         = 0x00,
    ACTION_BUTTON_C             = 0x01,                     // click?
    ACTION_BUTTON_MACRO         = 0x40,
    ACTION_BUTTON_CMACRO        = ACTION_BUTTON_C | ACTION_BUTTON_MACRO,
    ACTION_BUTTON_ITEM          = 0x80
};

#define ACTION_BUTTON_ACTION(X) (uint32(X) & 0x00FFFFFF)
#define ACTION_BUTTON_TYPE(X)   ((uint32(X) & 0xFF000000) >> 24)
#define MAX_ACTION_BUTTON_ACTION_VALUE (0x00FFFFFF+1)

struct ActionButton
{
    ActionButton() : packedData(0), uState(ACTIONBUTTON_NEW) {}

    uint32 packedData;
    ActionButtonUpdateState uState;

    // helpers
    ActionButtonType GetType() const { return ActionButtonType(ACTION_BUTTON_TYPE(packedData)); }
    uint32 GetAction() const { return ACTION_BUTTON_ACTION(packedData); }
    void SetActionAndType(uint32 action, ActionButtonType type)
    {
        uint32 newData = action | (uint32(type) << 24);
        if (newData != packedData || uState == ACTIONBUTTON_DELETED)
        {
            packedData = newData;
            if (uState != ACTIONBUTTON_NEW)
                uState = ACTIONBUTTON_CHANGED;
        }
    }
};

#define  MAX_ACTION_BUTTONS 132                             // checked in 2.3.0

typedef std::map<uint8, ActionButton> ActionButtonList;

struct PlayerCreateInfoItem
{
    PlayerCreateInfoItem(uint32 id, uint32 amount) : item_id(id), item_amount(amount) {}

    uint32 item_id;
    uint32 item_amount;
};

typedef std::list<PlayerCreateInfoItem> PlayerCreateInfoItems;

struct PlayerClassLevelInfo
{
    PlayerClassLevelInfo() : basehealth(0), basemana(0) {}
    uint16 basehealth;
    uint16 basemana;
};

struct PlayerClassInfo
{
    PlayerClassInfo() : levelInfo(nullptr) { }

    PlayerClassLevelInfo* levelInfo;                        //[level-1] 0..MaxPlayerLevel-1
};

struct PlayerLevelInfo
{
    PlayerLevelInfo() { for (int i = 0; i < MAX_STATS; ++i) stats[i] = 0; }

    uint8 stats[MAX_STATS];
};

typedef std::list<uint32> PlayerCreateInfoSpells;

struct PlayerCreateInfoAction
{
    PlayerCreateInfoAction() : button(0), type(0), action(0) {}
    PlayerCreateInfoAction(uint8 _button, uint32 _action, uint8 _type) : button(_button), type(_type), action(_action) {}

    uint8 button;
    uint8 type;
    uint32 action;
};

typedef std::list<PlayerCreateInfoAction> PlayerCreateInfoActions;

struct PlayerInfo
{
    // existence checked by displayId != 0             // existence checked by displayId != 0
    PlayerInfo() : mapId(0), areaId(0), positionX(0.f), positionY(0.f), positionZ(0.f), orientation(0.f), displayId_m(0), displayId_f(0), levelInfo(nullptr)
    {
    }

    uint32 mapId;
    uint32 areaId;
    float positionX;
    float positionY;
    float positionZ;
    float orientation;
    uint16 displayId_m;
    uint16 displayId_f;
    PlayerCreateInfoItems item;
    PlayerCreateInfoSpells spell;
    PlayerCreateInfoActions action;

    PlayerLevelInfo* levelInfo;                             //[level-1] 0..MaxPlayerLevel-1
};

struct PvPInfo
{
    PvPInfo() : inPvPCombat(false), inPvPEnforcedArea(false), inPvPCapturePoint(false), isPvPFlagCarrier(false), timerPvPRemaining(0), timerPvPContestedRemaining(0) {}

    bool inPvPCombat;
    bool inPvPEnforcedArea;
    bool inPvPCapturePoint;
    bool isPvPFlagCarrier;
    uint32 timerPvPRemaining;
    uint32 timerPvPContestedRemaining;
};

struct DuelInfo
{
    DuelInfo() : initiator(nullptr), opponent(nullptr), startTimer(0), startTime(0), outOfBound(0) {}

    Player* initiator;
    Player* opponent;
    time_t startTimer;
    time_t startTime;
    time_t outOfBound;
};

struct Areas
{
    uint32 areaID;
    uint32 areaFlag;
    float x1;
    float x2;
    float y1;
    float y2;
};

struct EnchantDuration
{
    EnchantDuration() : item(nullptr), slot(MAX_ENCHANTMENT_SLOT), leftduration(0) {};
    EnchantDuration(Item* _item, EnchantmentSlot _slot, uint32 _leftduration) : item(_item), slot(_slot), leftduration(_leftduration) { MANGOS_ASSERT(item); };

    Item* item;
    EnchantmentSlot slot;
    uint32 leftduration;
};

typedef std::list<EnchantDuration> EnchantDurationList;
typedef std::list<Item*> ItemDurationList;

struct LookingForGroupSlot
{
    LookingForGroupSlot() : entry(0), type(0) {}
    bool Empty() const { return !entry && !type; }
    void Clear() { entry = 0; type = 0; }
    void Set(uint32 _entry, uint32 _type) { entry = _entry; type = _type; }
    bool Is(uint32 _entry, uint32 _type) const { return entry == _entry && type == _type; }
    bool canAutoJoin() const { return entry && (type == LFG_TYPE_DUNGEON || type == LFG_TYPE_HEROIC_DUNGEON); }

    uint32 entry;
    uint32 type;
};

#define MAX_LOOKING_FOR_GROUP_SLOT 3

struct LookingForGroup
{
    LookingForGroup() {}
    bool HaveInSlot(LookingForGroupSlot const& slot) const { return HaveInSlot(slot.entry, slot.type); }
    bool HaveInSlot(uint32 _entry, uint32 _type) const
    {
        for (int i = 0; i < MAX_LOOKING_FOR_GROUP_SLOT; ++i)
            if (slots[i].Is(_entry, _type))
                return true;
        return false;
    }

    bool canAutoJoin() const
    {
        for (int i = 0; i < MAX_LOOKING_FOR_GROUP_SLOT; ++i)
            if (slots[i].canAutoJoin())
                return true;
        return false;
    }

    bool Empty() const
    {
        for (int i = 0; i < MAX_LOOKING_FOR_GROUP_SLOT; ++i)
            if (!slots[i].Empty())
                return false;
        return more.Empty();
    }

    LookingForGroupSlot slots[MAX_LOOKING_FOR_GROUP_SLOT];
    LookingForGroupSlot more;
    std::string comment;
};

enum RaidGroupError
{
    ERR_RAID_GROUP_NONE                 = 0,
    ERR_RAID_GROUP_LOWLEVEL             = 1,
    ERR_RAID_GROUP_ONLY                 = 2,
    ERR_RAID_GROUP_FULL                 = 3,
    ERR_RAID_GROUP_REQUIREMENTS_UNMATCH = 4
};

enum DrunkenState
{
    DRUNKEN_SOBER               = 0,
    DRUNKEN_TIPSY               = 1,
    DRUNKEN_DRUNK               = 2,
    DRUNKEN_SMASHED             = 3
};

#define MAX_DRUNKEN             4

enum PlayerFlags
{
    PLAYER_FLAGS_NONE                   = 0x00000000,
    PLAYER_FLAGS_GROUP_LEADER           = 0x00000001,
    PLAYER_FLAGS_AFK                    = 0x00000002,
    PLAYER_FLAGS_DND                    = 0x00000004,
    PLAYER_FLAGS_GM                     = 0x00000008,
    PLAYER_FLAGS_GHOST                  = 0x00000010,
    PLAYER_FLAGS_RESTING                = 0x00000020,
    PLAYER_FLAGS_UNK7                   = 0x00000040,       // admin?
    PLAYER_FLAGS_FFA_PVP                = 0x00000080,
    PLAYER_FLAGS_CONTESTED_PVP          = 0x00000100,       // Player has been involved in a PvP combat and will be attacked by contested guards
    PLAYER_FLAGS_PVP_DESIRED            = 0x00000200,       // Stores player's permanent PvP flag preference
    PLAYER_FLAGS_HIDE_HELM              = 0x00000400,
    PLAYER_FLAGS_HIDE_CLOAK             = 0x00000800,
    PLAYER_FLAGS_PARTIAL_PLAY_TIME      = 0x00001000,       // played long time
    PLAYER_FLAGS_NO_PLAY_TIME           = 0x00002000,       // played too long time
    PLAYER_FLAGS_UNK15                  = 0x00004000,
    PLAYER_FLAGS_UNK16                  = 0x00008000,       // strange visual effect (2.0.1), looks like PLAYER_FLAGS_GHOST flag
    PLAYER_FLAGS_SANCTUARY              = 0x00010000,       // player entered sanctuary
    PLAYER_FLAGS_TAXI_BENCHMARK         = 0x00020000,       // taxi benchmark mode (on/off) (2.0.1)
    PLAYER_FLAGS_PVP_TIMER              = 0x00040000,       // 3.0.2, pvp timer active (after you disable pvp manually)
    PLAYER_FLAGS_COMMENTATOR            = 0x00080000,       // first appeared in TBC
};

// used for PLAYER__FIELD_KNOWN_TITLES field (uint64), (1<<bit_index) without (-1)
// can't use enum for uint64 values
#define PLAYER_TITLE_DISABLED              uint64(0x0000000000000000)
#define PLAYER_TITLE_NONE                  uint64(0x0000000000000001)
#define PLAYER_TITLE_PRIVATE               uint64(0x0000000000000002) // 1
#define PLAYER_TITLE_CORPORAL              uint64(0x0000000000000004) // 2
#define PLAYER_TITLE_SERGEANT_A            uint64(0x0000000000000008) // 3
#define PLAYER_TITLE_MASTER_SERGEANT       uint64(0x0000000000000010) // 4
#define PLAYER_TITLE_SERGEANT_MAJOR        uint64(0x0000000000000020) // 5
#define PLAYER_TITLE_KNIGHT                uint64(0x0000000000000040) // 6
#define PLAYER_TITLE_KNIGHT_LIEUTENANT     uint64(0x0000000000000080) // 7
#define PLAYER_TITLE_KNIGHT_CAPTAIN        uint64(0x0000000000000100) // 8
#define PLAYER_TITLE_KNIGHT_CHAMPION       uint64(0x0000000000000200) // 9
#define PLAYER_TITLE_LIEUTENANT_COMMANDER  uint64(0x0000000000000400) // 10
#define PLAYER_TITLE_COMMANDER             uint64(0x0000000000000800) // 11
#define PLAYER_TITLE_MARSHAL               uint64(0x0000000000001000) // 12
#define PLAYER_TITLE_FIELD_MARSHAL         uint64(0x0000000000002000) // 13
#define PLAYER_TITLE_GRAND_MARSHAL         uint64(0x0000000000004000) // 14
#define PLAYER_TITLE_SCOUT                 uint64(0x0000000000008000) // 15
#define PLAYER_TITLE_GRUNT                 uint64(0x0000000000010000) // 16
#define PLAYER_TITLE_SERGEANT_H            uint64(0x0000000000020000) // 17
#define PLAYER_TITLE_SENIOR_SERGEANT       uint64(0x0000000000040000) // 18
#define PLAYER_TITLE_FIRST_SERGEANT        uint64(0x0000000000080000) // 19
#define PLAYER_TITLE_STONE_GUARD           uint64(0x0000000000100000) // 20
#define PLAYER_TITLE_BLOOD_GUARD           uint64(0x0000000000200000) // 21
#define PLAYER_TITLE_LEGIONNAIRE           uint64(0x0000000000400000) // 22
#define PLAYER_TITLE_CENTURION             uint64(0x0000000000800000) // 23
#define PLAYER_TITLE_CHAMPION              uint64(0x0000000001000000) // 24
#define PLAYER_TITLE_LIEUTENANT_GENERAL    uint64(0x0000000002000000) // 25
#define PLAYER_TITLE_GENERAL               uint64(0x0000000004000000) // 26
#define PLAYER_TITLE_WARLORD               uint64(0x0000000008000000) // 27
#define PLAYER_TITLE_HIGH_WARLORD          uint64(0x0000000010000000) // 28
#define PLAYER_TITLE_GLADIATOR             uint64(0x0000000020000000) // 29
#define PLAYER_TITLE_DUELIST               uint64(0x0000000040000000) // 30
#define PLAYER_TITLE_RIVAL                 uint64(0x0000000080000000) // 31
#define PLAYER_TITLE_CHALLENGER            uint64(0x0000000100000000) // 32
#define PLAYER_TITLE_SCARAB_LORD           uint64(0x0000000200000000) // 33
#define PLAYER_TITLE_CONQUEROR             uint64(0x0000000400000000) // 34
#define PLAYER_TITLE_JUSTICAR              uint64(0x0000000800000000) // 35
#define PLAYER_TITLE_CHAMPION_OF_THE_NAARU uint64(0x0000001000000000) // 36
#define PLAYER_TITLE_MERCILESS_GLADIATOR   uint64(0x0000002000000000) // 37
#define PLAYER_TITLE_OF_THE_SHATTERED_SUN  uint64(0x0000004000000000) // 38
#define PLAYER_TITLE_HAND_OF_ADAL          uint64(0x0000008000000000) // 39
#define PLAYER_TITLE_VENGEFUL_GLADIATOR    uint64(0x0000010000000000) // 40

#define MAX_TITLE_INDEX     64                              // 1 uint64 field

// used in (PLAYER_FIELD_BYTES, 0) byte values
enum PlayerFieldByteFlags
{
    PLAYER_FIELD_BYTE_TRACK_STEALTHED   = 0x02,
    PLAYER_FIELD_BYTE_RELEASE_TIMER     = 0x08,             // Display time till auto release spirit
    PLAYER_FIELD_BYTE_NO_RELEASE_WINDOW = 0x10              // Display no "release spirit" window at all
};

// used in byte (PLAYER_FIELD_BYTES2,1) values
enum PlayerFieldByte2Flags
{
    PLAYER_FIELD_BYTE2_NONE              = 0x00,
    PLAYER_FIELD_BYTE2_DETECT_AMORE_0    = 0x02,            // SPELL_AURA_DETECT_AMORE, not used as value and maybe not relcted to, but used in code as base for mask apply
    PLAYER_FIELD_BYTE2_DETECT_AMORE_1    = 0x04,            // SPELL_AURA_DETECT_AMORE value 1
    PLAYER_FIELD_BYTE2_DETECT_AMORE_2    = 0x08,            // SPELL_AURA_DETECT_AMORE value 2
    PLAYER_FIELD_BYTE2_DETECT_AMORE_3    = 0x10,            // SPELL_AURA_DETECT_AMORE value 3
    PLAYER_FIELD_BYTE2_STEALTH           = 0x20,
    PLAYER_FIELD_BYTE2_INVISIBILITY_GLOW = 0x40
};

enum MirrorTimerType
{
    FATIGUE_TIMER               = 0,
    BREATH_TIMER                = 1,
    FIRE_TIMER                  = 2
};
#define MAX_TIMERS              3
#define DISABLED_MIRROR_TIMER   -1

// 2^n values
enum PlayerExtraFlags
{
    // gm abilities
    PLAYER_EXTRA_GM_ON              = 0x0001,
    PLAYER_EXTRA_GM_ACCEPT_TICKETS  = 0x0002,
    PLAYER_EXTRA_ACCEPT_WHISPERS    = 0x0004,
    PLAYER_EXTRA_TAXICHEAT          = 0x0008,
    PLAYER_EXTRA_GM_INVISIBLE       = 0x0010,
    PLAYER_EXTRA_GM_CHAT            = 0x0020,               // Show GM badge in chat messages
    PLAYER_EXTRA_AUCTION_NEUTRAL    = 0x0040,
    PLAYER_EXTRA_AUCTION_ENEMY      = 0x0080,               // overwrite PLAYER_EXTRA_AUCTION_NEUTRAL

    // other states
    PLAYER_EXTRA_PVP_DEATH          = 0x0100                // store PvP death status until corpse creating.
};

// 2^n values
enum AtLoginFlags
{
    AT_LOGIN_NONE                 = 0x00,
    AT_LOGIN_RENAME               = 0x01,
    AT_LOGIN_RESET_SPELLS         = 0x02,
    AT_LOGIN_RESET_TALENTS        = 0x04,
    // AT_LOGIN_CUSTOMIZE         = 0x08, -- used in post-3.x
    // AT_LOGIN_RESET_PET_TALENTS = 0x10, -- used in post-3.x
    AT_LOGIN_FIRST                = 0x20,
    AT_LOGIN_RESET_TAXINODES      = 0x40,
};

typedef std::map<uint32, QuestStatusData> QuestStatusMap;

enum QuestSlotOffsets
{
    QUEST_ID_OFFSET             = 0,
    QUEST_STATE_OFFSET          = 1,
    QUEST_COUNTS_OFFSET         = 2,
    QUEST_TIME_OFFSET           = 3
};

#define MAX_QUEST_OFFSET 4

enum QuestSlotStateMask
{
    QUEST_STATE_NONE            = 0x0000,
    QUEST_STATE_COMPLETE        = 0x0001,
    QUEST_STATE_FAIL            = 0x0002
};

enum SkillUpdateState
{
    SKILL_UNCHANGED             = 0,
    SKILL_CHANGED               = 1,
    SKILL_NEW                   = 2,
    SKILL_DELETED               = 3
};

struct SkillStatusData
{
    SkillStatusData(uint8 _pos, SkillUpdateState _uState) : pos(_pos), uState(_uState)
    {
    }
    uint8 pos;
    SkillUpdateState uState;
};

typedef std::unordered_map<uint32, SkillStatusData> SkillStatusMap;

enum PlayerSlots
{
    // first slot for item stored (in any way in player m_items data)
    PLAYER_SLOT_START           = 0,
    // last+1 slot for item stored (in any way in player m_items data)
    PLAYER_SLOT_END             = 118,
    PLAYER_SLOTS_COUNT          = (PLAYER_SLOT_END - PLAYER_SLOT_START)
};

#define INVENTORY_SLOT_BAG_0    255

enum EquipmentSlots                                         // 19 slots
{
    EQUIPMENT_SLOT_START        = 0,
    EQUIPMENT_SLOT_HEAD         = 0,
    EQUIPMENT_SLOT_NECK         = 1,
    EQUIPMENT_SLOT_SHOULDERS    = 2,
    EQUIPMENT_SLOT_BODY         = 3,
    EQUIPMENT_SLOT_CHEST        = 4,
    EQUIPMENT_SLOT_WAIST        = 5,
    EQUIPMENT_SLOT_LEGS         = 6,
    EQUIPMENT_SLOT_FEET         = 7,
    EQUIPMENT_SLOT_WRISTS       = 8,
    EQUIPMENT_SLOT_HANDS        = 9,
    EQUIPMENT_SLOT_FINGER1      = 10,
    EQUIPMENT_SLOT_FINGER2      = 11,
    EQUIPMENT_SLOT_TRINKET1     = 12,
    EQUIPMENT_SLOT_TRINKET2     = 13,
    EQUIPMENT_SLOT_BACK         = 14,
    EQUIPMENT_SLOT_MAINHAND     = 15,
    EQUIPMENT_SLOT_OFFHAND      = 16,
    EQUIPMENT_SLOT_RANGED       = 17,
    EQUIPMENT_SLOT_TABARD       = 18,
    EQUIPMENT_SLOT_END          = 19
};

enum InventorySlots                                         // 4 slots
{
    INVENTORY_SLOT_BAG_START    = 19,
    INVENTORY_SLOT_BAG_END      = 23
};

enum InventoryPackSlots                                     // 16 slots
{
    INVENTORY_SLOT_ITEM_START   = 23,
    INVENTORY_SLOT_ITEM_END     = 39
};

enum BankItemSlots                                          // 28 slots
{
    BANK_SLOT_ITEM_START        = 39,
    BANK_SLOT_ITEM_END          = 67
};

enum BankBagSlots                                           // 7 slots
{
    BANK_SLOT_BAG_START         = 67,
    BANK_SLOT_BAG_END           = 74
};

enum BuyBackSlots                                           // 12 slots
{
    // stored in m_buybackitems
    BUYBACK_SLOT_START          = 74,
    BUYBACK_SLOT_END            = 86
};

enum KeyRingSlots                                           // 32 slots
{
    KEYRING_SLOT_START          = 86,
    KEYRING_SLOT_END            = 118
};

struct ItemPosCount
{
    ItemPosCount(uint16 _pos, uint8 _count) : pos(_pos), count(_count) {}
    bool isContainedIn(std::vector<ItemPosCount> const& vec) const;
    uint16 pos;
    uint8 count;
};
typedef std::vector<ItemPosCount> ItemPosCountVec;

enum TradeSlots
{
    TRADE_SLOT_COUNT            = 7,
    TRADE_SLOT_TRADED_COUNT     = 6,
    TRADE_SLOT_NONTRADED        = 6
};

enum TransferAbortReason
{
    TRANSFER_ABORT_NONE                         = 0x00,
    TRANSFER_ABORT_MAX_PLAYERS                  = 0x01,     // Transfer Aborted: instance is full
    TRANSFER_ABORT_NOT_FOUND                    = 0x02,     // Transfer Aborted: instance not found
    TRANSFER_ABORT_TOO_MANY_INSTANCES           = 0x03,     // You have entered too many instances recently.
    TRANSFER_ABORT_ZONE_IN_COMBAT               = 0x05,     // Unable to zone in while an encounter is in progress.
    TRANSFER_ABORT_INSUF_EXPAN_LVL              = 0x06,     // You must have TBC expansion installed to access this area.
    TRANSFER_ABORT_DIFFICULTY                   = 0x07,     // <Normal,Heroic,Epic> difficulty mode is not available for %s.
};

enum InstanceResetWarningType
{
    RAID_INSTANCE_WARNING_HOURS     = 1,                    // WARNING! %s is scheduled to reset in %d hour(s).
    RAID_INSTANCE_WARNING_MIN       = 2,                    // WARNING! %s is scheduled to reset in %d minute(s)!
    RAID_INSTANCE_WARNING_MIN_SOON  = 3,                    // WARNING! %s is scheduled to reset in %d minute(s). Please exit the zone or you will be returned to your bind location!
    RAID_INSTANCE_WELCOME           = 4                     // Welcome to %s. This raid instance is scheduled to reset in %s.
};

// PLAYER_FIELD_ARENA_TEAM_INFO_1_1 offsets
enum ArenaTeamInfoType
{
    ARENA_TEAM_ID               = 0,
    ARENA_TEAM_MEMBER           = 1,                        // 0 - captain, 1 - member
    ARENA_TEAM_GAMES_WEEK       = 2,
    ARENA_TEAM_GAMES_SEASON     = 3,
    ARENA_TEAM_WINS_SEASON      = 4,
    ARENA_TEAM_PERSONAL_RATING  = 5,
    ARENA_TEAM_END              = 6
};

enum RestType
{
    REST_TYPE_NO                = 0,
    REST_TYPE_IN_TAVERN         = 1,
    REST_TYPE_IN_CITY           = 2
};

enum DuelCompleteType
{
    DUEL_INTERRUPTED            = 0,
    DUEL_WON                    = 1,
    DUEL_FLED                   = 2
};

enum TeleportToOptions
{
    TELE_TO_GM_MODE             = 0x01,
    TELE_TO_NOT_LEAVE_TRANSPORT = 0x02,
    TELE_TO_NOT_LEAVE_COMBAT    = 0x04,
    TELE_TO_NOT_UNSUMMON_PET    = 0x08,
    TELE_TO_SPELL               = 0x10,
};

/// Type of environmental damages
enum EnviromentalDamage
{
    DAMAGE_EXHAUSTED            = 0,
    DAMAGE_DROWNING             = 1,
    DAMAGE_FALL                 = 2,
    DAMAGE_LAVA                 = 3,
    DAMAGE_SLIME                = 4,
    DAMAGE_FIRE                 = 5,
    DAMAGE_FALL_TO_VOID         = 6                         // custom case for fall without durability loss
};

enum PlayedTimeIndex
{
    PLAYED_TIME_TOTAL           = 0,
    PLAYED_TIME_LEVEL           = 1
};

#define MAX_PLAYED_TIME_INDEX   2

// used at player loading query list preparing, and later result selection
enum PlayerLoginQueryIndex
{
    PLAYER_LOGIN_QUERY_LOADFROM,
    PLAYER_LOGIN_QUERY_LOADGROUP,
    PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES,
    PLAYER_LOGIN_QUERY_LOADAURAS,
    PLAYER_LOGIN_QUERY_LOADSPELLS,
    PLAYER_LOGIN_QUERY_LOADQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADDAILYQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADREPUTATION,
    PLAYER_LOGIN_QUERY_LOADINVENTORY,
    PLAYER_LOGIN_QUERY_LOADITEMLOOT,
    PLAYER_LOGIN_QUERY_LOADACTIONS,
    PLAYER_LOGIN_QUERY_LOADSOCIALLIST,
    PLAYER_LOGIN_QUERY_LOADHOMEBIND,
    PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS,
    PLAYER_LOGIN_QUERY_LOADDECLINEDNAMES,
    PLAYER_LOGIN_QUERY_LOADGUILD,
    PLAYER_LOGIN_QUERY_LOADARENAINFO,
    PLAYER_LOGIN_QUERY_LOADBGDATA,
    PLAYER_LOGIN_QUERY_LOADSKILLS,
    PLAYER_LOGIN_QUERY_LOADMAILS,
    PLAYER_LOGIN_QUERY_LOADMAILEDITEMS,
    PLAYER_LOGIN_QUERY_LOADWEEKLYQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADMONTHLYQUESTSTATUS,

    MAX_PLAYER_LOGIN_QUERY
};

enum PlayerDelayedOperations
{
    DELAYED_SAVE_PLAYER         = 0x01,
    DELAYED_RESURRECT_PLAYER    = 0x02,
    DELAYED_SPELL_CAST_DESERTER = 0x04,
    DELAYED_END
};

enum ReputationSource
{
    REPUTATION_SOURCE_KILL,
    REPUTATION_SOURCE_QUEST,
    REPUTATION_SOURCE_SPELL
};

// Player summoning auto-decline time (in secs)
#define MAX_PLAYER_SUMMON_DELAY (2*MINUTE)
#define MAX_MONEY_AMOUNT        (0x7FFFFFFF-1)

struct InstancePlayerBind
{
    DungeonPersistentState* state;
    bool perm;
    /* permanent PlayerInstanceBinds are created in Raid/Heroic instances for players
       that aren't already permanently bound when they are inside when a boss is killed
       or when they enter an instance that the group leader is permanently bound to. */
    InstancePlayerBind() : state(nullptr), perm(false) {}
};

enum PlayerRestState
{
    REST_STATE_RESTED           = 0x01,
    REST_STATE_NORMAL           = 0x02,
    REST_STATE_RAF_LINKED       = 0x04                      // Exact use unknown
};

class PlayerTaxi
{
    public:
        PlayerTaxi();
        ~PlayerTaxi() {}
        // Nodes
        void InitTaxiNodesForLevel(uint32 race, uint32 level);
        void LoadTaxiMask(const char* data);

        bool IsTaximaskNodeKnown(uint32 nodeidx) const
        {
            uint8  field   = uint8((nodeidx - 1) / 32);
            uint32 submask = 1 << ((nodeidx - 1) % 32);
            return (m_taximask[field] & submask) == submask;
        }
        bool SetTaximaskNode(uint32 nodeidx)
        {
            uint8  field   = uint8((nodeidx - 1) / 32);
            uint32 submask = 1 << ((nodeidx - 1) % 32);
            if ((m_taximask[field] & submask) != submask)
            {
                m_taximask[field] |= submask;
                return true;
            }
            else
                return false;
        }
        void AppendTaximaskTo(ByteBuffer& data, bool all);

        // Destinations
        bool LoadTaxiDestinationsFromString(const std::string& values, Team team);
        std::string SaveTaxiDestinationsToString();

        void ClearTaxiDestinations() { m_TaxiDestinations.clear(); }
        void AddTaxiDestination(uint32 dest) { m_TaxiDestinations.push_back(dest); }
        uint32 GetTaxiSource() const { return m_TaxiDestinations.empty() ? 0 : m_TaxiDestinations.front(); }
        uint32 GetNextTaxiDestination() const { return m_TaxiDestinations.size() < 2 ? 0 : m_TaxiDestinations[1]; }
        uint32 GetFinalTaxiDestination() const { return m_TaxiDestinations.empty() ? 0 : m_TaxiDestinations.back(); }
        uint32 GetCurrentTaxiPath() const;
        uint32 NextTaxiDestination()
        {
            m_TaxiDestinations.pop_front();
            return GetNextTaxiDestination();
        }
        bool empty() const { return m_TaxiDestinations.empty(); }
        FactionTemplateEntry const* GetFlightMasterFactionTemplate() const;
        void SetFlightMasterFactionTemplateId(uint32 factionTemplateId) { m_flightMasterFactionId = factionTemplateId; }

        friend std::ostringstream& operator<<(std::ostringstream& ss, PlayerTaxi const& taxi);

        std::deque<uint32> const& GetPath() const { return m_TaxiDestinations; }

        uint32 GetLastNode() { return m_lastNode; }
        void SetLastNode(uint32 lastNode) { m_lastNode = lastNode; }
    private:
        TaxiMask m_taximask;
        std::deque<uint32> m_TaxiDestinations;
        uint32 m_lastNode;
        uint32 m_flightMasterFactionId;
};

std::ostringstream& operator<< (std::ostringstream& ss, PlayerTaxi const& taxi);

/// Holder for BattleGround data
struct BGData
{
    BGData() : bgInstanceID(0), bgTypeID(BATTLEGROUND_TYPE_NONE), bgAfkReportedCount(0), bgAfkReportedTimer(0),
        bgTeam(TEAM_NONE), m_needSave(false) {}

    uint32 bgInstanceID;                                    ///< This variable is set to bg->m_InstanceID, saved
    ///  when player is teleported to BG - (it is battleground's GUID)
    BattleGroundTypeId bgTypeID;

    std::set<uint32>   bgAfkReporter;
    uint8              bgAfkReportedCount;
    time_t             bgAfkReportedTimer;

    Team bgTeam;                                            ///< What side the player will be added to, saved

    WorldLocation joinPos;                                  ///< From where player entered BG, saved

    bool m_needSave;                                        ///< true, if saved to DB fields modified after prev. save (marked as "saved" above)
};

struct TradeStatusInfo
{
    TradeStatusInfo() : Status(TRADE_STATUS_BUSY), TraderGuid(), Result(EQUIP_ERR_OK),
        IsTargetResult(false), ItemLimitCategoryId(0), Slot(0) { }

    TradeStatus Status;
    ObjectGuid TraderGuid;
    InventoryResult Result;
    bool IsTargetResult;
    uint32 ItemLimitCategoryId;
    uint8 Slot;
};

class TradeData
{
    public:                                                 // constructors
        TradeData(Player* player, Player* trader) :
            m_player(player),  m_trader(trader), m_accepted(false), m_acceptProccess(false),
            m_money(0), m_spell(0) {}

        Player* GetTrader() const { return m_trader; }
        TradeData* GetTraderData() const;

        Item* GetItem(TradeSlots slot) const;
        bool HasItem(ObjectGuid item_guid) const;

        uint32 GetSpell() const { return m_spell; }
        Item*  GetSpellCastItem() const;
        bool HasSpellCastItem() const { return !m_spellCastItem.IsEmpty(); }

        uint32 GetMoney() const { return m_money; }

        bool IsAccepted() const { return m_accepted; }
        bool IsInAcceptProcess() const { return m_acceptProccess; }

        void SetItem(TradeSlots slot, Item* item);
        void SetSpell(uint32 spell_id, Item* castItem = nullptr);
        void SetMoney(uint32 money);

        void SetAccepted(bool state, bool crosssend = false);

        // must be called only from accept handler helper functions
        void SetInAcceptProcess(bool state) { m_acceptProccess = state; }

    private:                                                // internal functions

        void Update(bool for_trader = true) const;

        Player*    m_player;                                // Player who own of this TradeData
        Player*    m_trader;                                // Player who trade with m_player

        bool       m_accepted;                              // m_player press accept for trade list
        bool       m_acceptProccess;                        // one from player/trader press accept and this processed

        uint32     m_money;                                 // m_player place money to trade

        uint32     m_spell;                                 // m_player apply spell to non-traded slot item
        ObjectGuid m_spellCastItem;                         // applied spell casted by item use

        ObjectGuid m_items[TRADE_SLOT_COUNT];               // traded itmes from m_player side including non-traded slot
};

class Player : public Unit
{
        friend class WorldSession;
        friend void Item::AddToUpdateQueueOf(Player* player);
        friend void Item::RemoveFromUpdateQueueOf(Player* player);
    public:
        explicit Player(WorldSession* session);
        ~Player();

        void CleanupsBeforeDelete() override;

        static UpdateMask updateVisualBits;
        static void InitVisibleBits();

        void AddToWorld() override;
        void RemoveFromWorld() override;

        bool TeleportTo(uint32 mapid, float x, float y, float z, float orientation, uint32 options = 0, AreaTrigger const* at = nullptr);

        bool TeleportTo(WorldLocation const& loc, uint32 options = 0)
        {
            return TeleportTo(loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z, loc.orientation, options);
        }

        bool TeleportToBGEntryPoint();

        void SetSummonPoint(uint32 mapid, float x, float y, float z)
        {
            m_summon_expire = time(nullptr) + MAX_PLAYER_SUMMON_DELAY;
            m_summon_mapid = mapid;
            m_summon_x = x;
            m_summon_y = y;
            m_summon_z = z;
        }
        void SummonIfPossible(bool agree);

        bool Create(uint32 guidlow, const std::string& name, uint8 race, uint8 class_, uint8 gender, uint8 skin, uint8 face, uint8 hairStyle, uint8 hairColor, uint8 facialHair, uint8 outfitId);

        void Update(uint32 update_diff, uint32 time) override;

        static bool BuildEnumData(QueryResult* result,  WorldPacket& p_data);

        void SetInWater(bool apply);

        bool IsInWater() const override { return m_isInWater; }
        bool IsUnderWater() const override;

        void SendInitialPacketsBeforeAddToMap();
        void SendInitialPacketsAfterAddToMap();
        void SendInstanceResetWarning(uint32 mapid, uint32 time);

        Creature* GetNPCIfCanInteractWith(ObjectGuid guid, uint32 npcflagmask);
        GameObject* GetGameObjectIfCanInteractWith(ObjectGuid guid, uint32 gameobject_type = MAX_GAMEOBJECT_TYPE);

        void ToggleAFK();
        void ToggleDND();
        bool isAFK() const { return HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK); }
        bool isDND() const { return HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_DND); }
        ChatTagFlags GetChatTag() const;
        std::string autoReplyMsg;

        PlayerSocial* GetSocial() { return m_social; }
        const PlayerSocial* GetSocial() const { return m_social; }

        PlayerTaxi m_taxi;
        void InitTaxiNodesForLevel() { m_taxi.InitTaxiNodesForLevel(getRace(), getLevel()); }
        bool ActivateTaxiPathTo(std::vector<uint32> const& nodes, Creature* npc = nullptr, uint32 spellid = 0);
        bool ActivateTaxiPathTo(uint32 taxi_path_id, uint32 spellid = 0);
        // mount_id can be used in scripting calls
        void ContinueTaxiFlight() const;
        bool isAcceptTickets() const { return GetSession()->GetSecurity() >= SEC_GAMEMASTER && (m_ExtraFlags & PLAYER_EXTRA_GM_ACCEPT_TICKETS); }
        void SetAcceptTicket(bool on) { if (on) m_ExtraFlags |= PLAYER_EXTRA_GM_ACCEPT_TICKETS; else m_ExtraFlags &= ~PLAYER_EXTRA_GM_ACCEPT_TICKETS; }
        bool isAcceptWhispers() const { return !!(m_ExtraFlags & PLAYER_EXTRA_ACCEPT_WHISPERS); }
        void SetAcceptWhispers(bool on) { if (on) m_ExtraFlags |= PLAYER_EXTRA_ACCEPT_WHISPERS; else m_ExtraFlags &= ~PLAYER_EXTRA_ACCEPT_WHISPERS; }
        bool isGameMaster() const { return m_ExtraFlags & PLAYER_EXTRA_GM_ON; }
        void SetGameMaster(bool on);
        bool isGMChat() const { return GetSession()->GetSecurity() >= SEC_MODERATOR && (m_ExtraFlags & PLAYER_EXTRA_GM_CHAT); }
        void SetGMChat(bool on) { if (on) m_ExtraFlags |= PLAYER_EXTRA_GM_CHAT; else m_ExtraFlags &= ~PLAYER_EXTRA_GM_CHAT; }
        bool isTaxiCheater() const { return !!(m_ExtraFlags & PLAYER_EXTRA_TAXICHEAT); }
        void SetTaxiCheater(bool on) { if (on) m_ExtraFlags |= PLAYER_EXTRA_TAXICHEAT; else m_ExtraFlags &= ~PLAYER_EXTRA_TAXICHEAT; }
        bool isGMVisible() const { return !(m_ExtraFlags & PLAYER_EXTRA_GM_INVISIBLE); }
        void SetGMVisible(bool on);
        void SetPvPDeath(bool on) { if (on) m_ExtraFlags |= PLAYER_EXTRA_PVP_DEATH; else m_ExtraFlags &= ~PLAYER_EXTRA_PVP_DEATH; }

        // 0 = own auction, -1 = enemy auction, 1 = goblin auction
        int GetAuctionAccessMode() const { return m_ExtraFlags & PLAYER_EXTRA_AUCTION_ENEMY ? -1 : (m_ExtraFlags & PLAYER_EXTRA_AUCTION_NEUTRAL ? 1 : 0); }
        void SetAuctionAccessMode(int state)
        {
            m_ExtraFlags &= ~(PLAYER_EXTRA_AUCTION_ENEMY | PLAYER_EXTRA_AUCTION_NEUTRAL);

            if (state < 0)
                m_ExtraFlags |= PLAYER_EXTRA_AUCTION_ENEMY;
            else if (state > 0)
                m_ExtraFlags |= PLAYER_EXTRA_AUCTION_NEUTRAL;
        }


        void GiveXP(uint32 xp, Unit* victim);
        void GiveLevel(uint32 level);

        void InitStatsForLevel(bool reapplyMods = false);

        // Played Time Stuff
        time_t m_logintime;
        time_t m_Last_tick;

        uint32 m_Played_time[MAX_PLAYED_TIME_INDEX];
        uint32 GetTotalPlayedTime() { return m_Played_time[PLAYED_TIME_TOTAL]; }
        uint32 GetLevelPlayedTime() { return m_Played_time[PLAYED_TIME_LEVEL]; }

        void ResetTimeSync();
        void SendTimeSync();

        Player* GetSpellModOwner() const override { return const_cast<Player*>(this); }

        void SetDeathState(DeathState s) override;          // overwrite Unit::SetDeathState

        float GetRestBonus() const { return m_rest_bonus; }
        void SetRestBonus(float rest_bonus_new);

        /**
        * \brief: compute rest bonus
        * \param: time_t timePassed > time from last check
        * \param: bool offline      > is the player was offline?
        * \param: bool inRestPlace  > if it was offline, is the player was in city/tavern/inn?
        * \returns: float
        **/
        float ComputeRest(time_t timePassed, bool offline = false, bool inRestPlace = false) const;

        /**
        * \brief: player is interacting with something.
        * \param: ObjectGuid interactObj > object that interact with this player
        **/
        void DoInteraction(ObjectGuid const& interactObjGuid);
        RestType GetRestType() const { return rest_type; }
        void SetRestType(RestType n_r_type, uint32 areaTriggerId = 0);

        time_t GetTimeInnEnter() const { return time_inn_enter; }
        void UpdateInnerTime(time_t time) { time_inn_enter = time; }

        void RemovePet(PetSaveMode mode);

        void Say(const std::string& text, const uint32 language) const;
        void Yell(const std::string& text, const uint32 language) const;
        void TextEmote(const std::string& text) const;
        void Whisper(const std::string& text, const uint32 language, ObjectGuid receiver);

        /*********************************************************/
        /***                    STORAGE SYSTEM                 ***/
        /*********************************************************/

        void SetVirtualItemSlot(uint8 i, Item* item);
        void SetSheath(SheathState sheathed) override;      // overwrite Unit version
        bool ViableEquipSlots(ItemPrototype const* proto, uint8* viable_slots) const;
        uint8 FindEquipSlot(ItemPrototype const* proto, uint32 slot, bool swap) const;
        uint32 GetItemCount(uint32 item, bool inBankAlso = false, Item* skipItem = nullptr) const;
        Item* GetItemByGuid(ObjectGuid guid) const;
        Item* GetItemByPos(uint16 pos) const;
        Item* GetItemByPos(uint8 bag, uint8 slot) const;
        uint32 GetItemDisplayIdInSlot(uint8 bag, uint8 slot) const;
        Item* GetWeaponForAttack(WeaponAttackType attackType) const { return GetWeaponForAttack(attackType, false, false); }
        Item* GetWeaponForAttack(WeaponAttackType attackType, bool nonbroken, bool useable) const;
        Item* GetShield(bool useable = false) const;
        static uint32 GetAttackBySlot(uint8 slot);          // MAX_ATTACK if not weapon slot
        std::vector<Item*>& GetItemUpdateQueue() { return m_itemUpdateQueue; }
        static bool IsInventoryPos(uint16 pos) { return IsInventoryPos(pos >> 8, pos & 255); }
        static bool IsInventoryPos(uint8 bag, uint8 slot);
        static bool IsEquipmentPos(uint16 pos) { return IsEquipmentPos(pos >> 8, pos & 255); }
        static bool IsEquipmentPos(uint8 bag, uint8 slot);
        static bool IsBagPos(uint16 pos);
        static bool IsBankPos(uint16 pos) { return IsBankPos(pos >> 8, pos & 255); }
        static bool IsBankPos(uint8 bag, uint8 slot);
        bool IsValidPos(uint16 pos, bool explicit_pos) const { return IsValidPos(pos >> 8, pos & 255, explicit_pos); }
        bool IsValidPos(uint8 bag, uint8 slot, bool explicit_pos) const;
        uint8 GetBankBagSlotCount() const { return GetByteValue(PLAYER_BYTES_2, 2); }
        void SetBankBagSlotCount(uint8 count) { SetByteValue(PLAYER_BYTES_2, 2, count); }
        bool HasItemCount(uint32 item, uint32 count, bool inBankAlso = false) const;
        bool HasItemFitToSpellReqirements(SpellEntry const* spellInfo, Item const* ignoreItem = nullptr) const;
        bool CanNoReagentCast(SpellEntry const* spellInfo) const;
        bool HasItemOrGemWithIdEquipped(uint32 item, uint32 count, uint8 except_slot = NULL_SLOT) const;
        InventoryResult CanTakeMoreSimilarItems(Item* pItem) const { return _CanTakeMoreSimilarItems(pItem->GetEntry(), pItem->GetCount(), pItem); }
        InventoryResult CanTakeMoreSimilarItems(uint32 entry, uint32 count) const { return _CanTakeMoreSimilarItems(entry, count, nullptr); }
        InventoryResult CanStoreNewItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, uint32 item, uint32 count, uint32* no_space_count = nullptr) const
        {
            return _CanStoreItem(bag, slot, dest, item, count, nullptr, false, no_space_count);
        }
        InventoryResult CanStoreItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, Item* pItem, bool swap = false) const
        {
            if (!pItem)
                return EQUIP_ERR_ITEM_NOT_FOUND;
            uint32 count = pItem->GetCount();
            return _CanStoreItem(bag, slot, dest, pItem->GetEntry(), count, pItem, swap, nullptr);
        }
        InventoryResult CanStoreItems(Item** pItem, int count) const;
        InventoryResult CanEquipNewItem(uint8 slot, uint16& dest, uint32 item, bool swap) const;
        InventoryResult CanEquipItem(uint8 slot, uint16& dest, Item* pItem, bool swap, bool direct_action = true) const;

        InventoryResult CanEquipUniqueItem(Item* pItem, uint8 except_slot = NULL_SLOT) const;
        InventoryResult CanEquipUniqueItem(ItemPrototype const* itemProto, uint8 except_slot = NULL_SLOT) const;
        InventoryResult CanUnequipItems(uint32 item, uint32 count) const;
        InventoryResult CanUnequipItem(uint16 src, bool swap) const;
        InventoryResult CanBankItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, Item* pItem, bool swap, bool not_loading = true) const;
        InventoryResult CanUseItem(Item* pItem, bool direct_action = true) const;
        bool HasItemTotemCategory(uint32 TotemCategory) const;
        InventoryResult CanUseItem(ItemPrototype const* pItem) const;
        InventoryResult CanUseAmmo(uint32 item) const;
        Item* StoreNewItem(ItemPosCountVec const& pos, uint32 item, bool update, int32 randomPropertyId = 0);
        Item* StoreItem(ItemPosCountVec const& pos, Item* pItem, bool update);
        Item* EquipNewItem(uint16 pos, uint32 item, bool update);
        Item* EquipItem(uint16 pos, Item* pItem, bool update);
        void AutoUnequipOffhandIfNeed();
        bool StoreNewItemInBestSlots(uint32 item_id, uint32 item_count);
        Item* StoreNewItemInInventorySlot(uint32 itemEntry, uint32 amount);

        Item* ConvertItem(Item* item, uint32 newItemId);

        InventoryResult _CanTakeMoreSimilarItems(uint32 entry, uint32 count, Item* pItem, uint32* no_space_count = nullptr) const;
        InventoryResult _CanStoreItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, uint32 entry, uint32 count, Item* pItem = nullptr, bool swap = false, uint32* no_space_count = nullptr) const;

        void ApplyEquipCooldown(Item* pItem);
        void SetAmmo(uint32 item);
        void RemoveAmmo();
        float GetAmmoDPS() const { return m_ammoDPS; }
        bool CheckAmmoCompatibility(const ItemPrototype* ammo_proto) const;
        void QuickEquipItem(uint16 pos, Item* pItem);
        void VisualizeItem(uint8 slot, Item* pItem);
        void SetVisibleItemSlot(uint8 slot, Item* pItem);
        Item* BankItem(ItemPosCountVec const& dest, Item* pItem, bool update)
        {
            return StoreItem(dest, pItem, update);
        }
        void RemoveItem(uint8 bag, uint8 slot, bool update);// see ApplyItemOnStoreSpell notes
        void MoveItemFromInventory(uint8 bag, uint8 slot, bool update);
        // in trade, auction, guild bank, mail....
        void MoveItemToInventory(ItemPosCountVec const& dest, Item* pItem, bool update, bool in_characterInventoryDB = false);
        // in trade, guild bank, mail....
        void RemoveItemDependentAurasAndCasts(Item* pItem);
        void DestroyItem(uint8 bag, uint8 slot, bool update);
        void DestroyItemCount(uint32 item, uint32 count, bool update, bool unequip_check = false, bool inBankAlso = false);
        void DestroyItemCount(Item* item, uint32& count, bool update);
        void DestroyConjuredItems(bool update);
        void DestroyZoneLimitedItem(bool update, uint32 new_zone);
        void SplitItem(uint16 src, uint16 dst, uint32 count);
        void SwapItem(uint16 src, uint16 dst);
        void AddItemToBuyBackSlot(Item* pItem, uint32 money);
        Item* GetItemFromBuyBackSlot(uint32 slot);
        void RemoveItemFromBuyBackSlot(uint32 slot, bool del);

        void TakeExtendedCost(uint32 extendedCostId, uint32 count);

        uint32 GetMaxKeyringSize() const { return KEYRING_SLOT_END - KEYRING_SLOT_START; }
        void SendEquipError(InventoryResult msg, Item* pItem, Item* pItem2 = nullptr, uint32 itemid = 0) const;
        void SendBuyError(BuyResult msg, Creature* pCreature, uint32 item, uint32 param) const;
        void SendSellError(SellResult msg, Creature* pCreature, ObjectGuid itemGuid, uint32 param) const;
        void AddWeaponProficiency(uint32 newflag) { m_WeaponProficiency |= newflag; }
        void AddArmorProficiency(uint32 newflag) { m_ArmorProficiency |= newflag; }
        uint32 GetWeaponProficiency() const { return m_WeaponProficiency; }
        uint32 GetArmorProficiency() const { return m_ArmorProficiency; }
        bool IsTwoHandUsed() const
        {
            Item* mainItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            return mainItem && mainItem->GetProto()->InventoryType == INVTYPE_2HWEAPON;
        }
        void SendNewItem(Item* item, uint32 count, bool received, bool created, bool broadcast = false, bool showInChat = true);
        bool BuyItemFromVendor(ObjectGuid vendorGuid, uint32 item, uint8 count, uint8 bag, uint8 slot);

        float GetReputationPriceDiscount(Creature const* creature) const;
        float GetReputationPriceDiscount(FactionTemplateEntry const* factionTemplate) const;

        Player* GetTrader() const { return m_trade ? m_trade->GetTrader() : nullptr; }
        TradeData* GetTradeData() const { return m_trade; }
        void TradeCancel(bool sendback);

        void UpdateEnchantTime(uint32 time);
        void UpdateItemDuration(uint32 time, bool realtimeonly = false);
        void AddEnchantmentDurations(Item* item);
        void RemoveEnchantmentDurations(Item* item);
        void RemoveAllEnchantments(EnchantmentSlot slot);
        void AddEnchantmentDuration(Item* item, EnchantmentSlot slot, uint32 duration);
        void ApplyEnchantment(Item* item, EnchantmentSlot slot, bool apply, bool apply_dur = true, bool ignore_condition = false);
        void ApplyEnchantment(Item* item, bool apply);
        void SendEnchantmentDurations();
        void AddItemDurations(Item* item);
        void RemoveItemDurations(Item* item);
        void SendItemDurations();
        void LoadCorpse();
        void LoadPet();

        uint32 m_stableSlots;

        /*********************************************************/
        /***                    GOSSIP SYSTEM                  ***/
        /*********************************************************/

        void PrepareGossipMenu(WorldObject* pSource, uint32 menuId = 0);
        void SendPreparedGossip(WorldObject* pSource);
        void OnGossipSelect(WorldObject* pSource, uint32 gossipListId, uint32 menuId);

        uint32 GetGossipTextId(uint32 menuId, WorldObject* pSource);
        uint32 GetGossipTextId(WorldObject* pSource) const;
        uint32 GetDefaultGossipMenuForSource(WorldObject* pSource) const;

        /*********************************************************/
        /***                    QUEST SYSTEM                   ***/
        /*********************************************************/

        // Return player level when QuestLevel is dynamic (-1)
        uint32 GetQuestLevelForPlayer(Quest const* pQuest) const { return pQuest && (pQuest->GetQuestLevel() > 0) ? (uint32)pQuest->GetQuestLevel() : getLevel(); }

        void PrepareQuestMenu(ObjectGuid guid) const;
        void SendPreparedQuest(ObjectGuid guid) const;
        bool IsActiveQuest(uint32 quest_id) const;          // can be taken or taken

        // Quest is taken and not yet rewarded
        // if completed_or_not = 0 (or any other value except 1 or 2) - returns true, if quest is taken and doesn't depend if quest is completed or not
        // if completed_or_not = 1 - returns true, if quest is taken but not completed
        // if completed_or_not = 2 - returns true, if quest is taken and already completed
        bool IsCurrentQuest(uint32 quest_id, uint8 completed_or_not = 0) const; // taken and not yet rewarded

        Quest const* GetNextQuest(ObjectGuid guid, Quest const* pQuest) const;
        bool CanSeeStartQuest(Quest const* pQuest) const;
        bool CanTakeQuest(Quest const* pQuest, bool msg) const;
        bool CanAddQuest(Quest const* pQuest, bool msg) const;
        bool CanCompleteQuest(uint32 quest_id) const;
        bool CanCompleteRepeatableQuest(Quest const* pQuest) const;
        bool CanRewardQuest(Quest const* pQuest, bool msg) const;
        bool CanRewardQuest(Quest const* pQuest, uint32 reward, bool msg) const;
        void AddQuest(Quest const* pQuest, Object* questGiver);
        void CompleteQuest(uint32 quest_id);
        void IncompleteQuest(uint32 quest_id);
        void RewardQuest(Quest const* pQuest, uint32 reward, Object* questGiver, bool announce = true);

        void FailQuest(uint32 quest_id);
        void FailQuest(Quest const* quest);
        void FailQuestsOnDeath();
        bool SatisfyQuestSkill(Quest const* qInfo, bool msg) const;
        bool SatisfyQuestCondition(Quest const* qInfo, bool msg) const;
        bool SatisfyQuestLevel(Quest const* qInfo, bool msg) const;
        bool SatisfyQuestLog(bool msg) const;
        bool SatisfyQuestPreviousQuest(Quest const* qInfo, bool msg) const;
        bool SatisfyQuestClass(Quest const* qInfo, bool msg) const;
        bool SatisfyQuestRace(Quest const* qInfo, bool msg) const;
        bool SatisfyQuestReputation(Quest const* qInfo, bool msg) const;
        bool SatisfyQuestStatus(Quest const* qInfo, bool msg) const;
        bool SatisfyQuestTimed(Quest const* qInfo, bool msg) const;
        bool SatisfyQuestExclusiveGroup(Quest const* qInfo, bool msg) const;
        bool SatisfyQuestNextChain(Quest const* qInfo, bool msg) const;
        bool SatisfyQuestPrevChain(Quest const* qInfo, bool msg) const;
        bool SatisfyQuestDay(Quest const* qInfo, bool msg) const;
        bool SatisfyQuestWeek(Quest const* qInfo) const;
        bool SatisfyQuestMonth(Quest const* qInfo) const;
        bool CanGiveQuestSourceItemIfNeed(Quest const* pQuest, ItemPosCountVec* dest = nullptr) const;
        void GiveQuestSourceItemIfNeed(Quest const* pQuest);
        bool TakeQuestSourceItem(uint32 quest_id, bool msg);
        bool GetQuestRewardStatus(uint32 quest_id) const;
        QuestStatus GetQuestStatus(uint32 quest_id) const;
        void SetQuestStatus(uint32 quest_id, QuestStatus status);

        void SetDailyQuestStatus(uint32 quest_id);
        void SetWeeklyQuestStatus(uint32 quest_id);
        void SetMonthlyQuestStatus(uint32 quest_id);
        void ResetDailyQuestStatus();
        void ResetWeeklyQuestStatus();
        void ResetMonthlyQuestStatus();

        uint16 FindQuestSlot(uint32 quest_id) const;
        uint32 GetQuestSlotQuestId(uint16 slot) const { return GetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_ID_OFFSET); }
        void SetQuestSlot(uint16 slot, uint32 quest_id, uint32 timer = 0)
        {
            SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_ID_OFFSET, quest_id);
            SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_STATE_OFFSET, 0);
            SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_COUNTS_OFFSET, 0);
            SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_TIME_OFFSET, timer);
        }
        void SetQuestSlotCounter(uint16 slot, uint8 counter, uint8 count)
        {
            SetByteValue(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_COUNTS_OFFSET, counter, count);
        }
        void SetQuestSlotState(uint16 slot, uint32 state) { SetFlag(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_STATE_OFFSET, state); }
        void RemoveQuestSlotState(uint16 slot, uint32 state) { RemoveFlag(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_STATE_OFFSET, state); }
        void SetQuestSlotTimer(uint16 slot, uint32 timer) { SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_TIME_OFFSET, timer); }
        void SwapQuestSlot(uint16 slot1, uint16 slot2)
        {
            for (int i = 0; i < MAX_QUEST_OFFSET; ++i)
            {
                uint32 temp1 = GetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot1 + i);
                uint32 temp2 = GetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot2 + i);

                SetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot1 + i, temp2);
                SetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot2 + i, temp1);
            }
        }
        uint32 GetReqKillOrCastCurrentCount(uint32 quest_id, int32 entry);
        void AreaExploredOrEventHappens(uint32 questId);
        void GroupEventHappens(uint32 questId, WorldObject const* pEventObject);
        void ItemAddedQuestCheck(uint32 entry, uint32 count);
        void ItemRemovedQuestCheck(uint32 entry, uint32 count);
        void KilledMonster(CreatureInfo const* cInfo, ObjectGuid guid);
        void KilledMonsterCredit(uint32 entry, ObjectGuid guid = ObjectGuid());
        void CastedCreatureOrGO(uint32 entry, ObjectGuid guid, uint32 spell_id, bool original_caster = true);
        void TalkedToCreature(uint32 entry, ObjectGuid guid);
        void MoneyChanged(uint32 value);
        void ReputationChanged(FactionEntry const* factionEntry);
        bool HasQuestForItem(uint32 itemid) const;
        bool HasQuestForGO(int32 GOId) const;
        void UpdateForQuestWorldObjects();
        bool CanShareQuest(uint32 quest_id) const;

        void SendQuestCompleteEvent(uint32 quest_id) const;
        void SendQuestReward(Quest const* pQuest, uint32 XP) const;
        void SendQuestFailed(uint32 quest_id) const;
        void SendQuestTimerFailed(uint32 quest_id) const;
        void SendCanTakeQuestResponse(uint32 msg) const;
        void SendQuestConfirmAccept(Quest const* pQuest, Player* pReceiver) const;
        void SendPushToPartyResponse(Player* pPlayer, uint32 msg) const;
        void SendQuestUpdateAddItem(Quest const* pQuest, uint32 item_idx, uint32 current, uint32 count);
        void SendQuestUpdateAddCreatureOrGo(Quest const* pQuest, ObjectGuid guid, uint32 creatureOrGO_idx, uint32 count);
        void SendQuestGiverStatusMultiple() const;

        ObjectGuid GetDividerGuid() const { return m_dividerGuid; }
        void SetDividerGuid(ObjectGuid guid) { m_dividerGuid = guid; }
        void ClearDividerGuid() { m_dividerGuid.Clear(); }

        uint32 GetInGameTime() const { return m_ingametime; }

        void SetInGameTime(uint32 time) { m_ingametime = time; }

        void AddTimedQuest(uint32 quest_id) { m_timedquests.insert(quest_id); }
        void RemoveTimedQuest(uint32 quest_id) { m_timedquests.erase(quest_id); }

#ifdef BUILD_PLAYERBOT
        void chompAndTrim(std::string& str);
        bool getNextQuestId(const std::string& pString, unsigned int& pStartPos, unsigned int& pId);
        void skill(std::list<uint32>& m_spellsToLearn);
        bool requiredQuests(const char* pQuestIdString);
        PlayerMails::reverse_iterator GetMailRBegin() { return m_mail.rbegin();}
        PlayerMails::reverse_iterator GetMailREnd() { return m_mail.rend();}
        void UpdateMail();
        uint32 GetSpec();
#endif

        /*********************************************************/
        /***                   LOAD SYSTEM                     ***/
        /*********************************************************/

        bool LoadFromDB(ObjectGuid guid, SqlQueryHolder* holder);

        static uint32 GetZoneIdFromDB(ObjectGuid guid);
        static uint32 GetLevelFromDB(ObjectGuid guid);
        static bool   LoadPositionFromDB(ObjectGuid guid, uint32& mapid, float& x, float& y, float& z, float& o, bool& in_flight);

        /*********************************************************/
        /***                   SAVE SYSTEM                     ***/
        /*********************************************************/

        void SaveToDB();
        void SaveInventoryAndGoldToDB();                    // fast save function for item/money cheating preventing
        void SaveGoldToDB() const;
        static void SetUInt32ValueInArray(Tokens& data, uint16 index, uint32 value);
        static void SavePositionInDB(ObjectGuid guid, uint32 mapid, float x, float y, float z, float o, uint32 zone);

        static void DeleteFromDB(ObjectGuid playerguid, uint32 accountId, bool updateRealmChars = true, bool deleteFinally = false);
        static void DeleteOldCharacters();
        static void DeleteOldCharacters(uint32 keepDays);

        bool m_mailsUpdated;

        void SendPetTameFailure(PetTameFailureReason reason) const;

        void SetBindPoint(ObjectGuid guid) const;
        void SendTalentWipeConfirm(ObjectGuid guid) const;
        void RewardRage(uint32 damage, uint32 weaponSpeedHitFactor, bool attacker);
        void SendPetSkillWipeConfirm() const;
        void RegenerateAll();
        void Regenerate(Powers power);
        void RegenerateHealth();
        void setRegenTimer(uint32 time) {m_regenTimer = time;}
        void setWeaponChangeTimer(uint32 time) {m_weaponChangeTimer = time;}

        uint32 GetMoney() const { return GetUInt32Value(PLAYER_FIELD_COINAGE); }
        void ModifyMoney(int32 d)
        {
            if (d < 0)
                SetMoney(GetMoney() > uint32(-d) ? GetMoney() + d : 0);
            else
                SetMoney(GetMoney() < uint32(MAX_MONEY_AMOUNT - d) ? GetMoney() + d : MAX_MONEY_AMOUNT);

            // "At Gold Limit"
            if (GetMoney() >= MAX_MONEY_AMOUNT)
                SendEquipError(EQUIP_ERR_TOO_MUCH_GOLD, nullptr, nullptr);
        }
        void SetMoney(uint32 value)
        {
            SetUInt32Value(PLAYER_FIELD_COINAGE, value);
            MoneyChanged(value);
        }

        QuestStatusMap& getQuestStatusMap() { return mQuestStatus; };
        const QuestStatusMap& getQuestStatusMap() const { return mQuestStatus; };

        ObjectGuid const& GetSelectionGuid() const override { return m_curSelectionGuid; }
        void SetSelectionGuid(ObjectGuid guid) override { m_curSelectionGuid = guid; SetTargetGuid(guid); }

        uint8 GetComboPoints() const { return m_comboPoints; }
        ObjectGuid const& GetComboTargetGuid() const { return m_comboTargetGuid; }

        void AddComboPoints(Unit* target, int8 count);
        void ClearComboPoints();
        void SendComboPoints() const;

        bool AttackStop(bool targetSwitch = false, bool includingCast = false, bool includingCombo = false) override;

        void SendMailResult(uint32 mailId, MailResponseType mailAction, MailResponseResult mailError, uint32 equipError = 0, uint32 item_guid = 0, uint32 item_count = 0) const;
        void SendNewMail() const;
        void UpdateNextMailTimeAndUnreads();
        void AddNewMailDeliverTime(time_t deliver_time);

        void RemoveMail(uint32 id);

        void AddMail(Mail* mail) { m_mail.push_front(mail);}// for call from WorldSession::SendMailTo
        size_t GetMailSize() const { return m_mail.size(); }
        Mail* GetMail(uint32 id);

        PlayerMails::iterator GetMailBegin() { return m_mail.begin();}
        PlayerMails::iterator GetMailEnd() { return m_mail.end();}

        /*********************************************************/
        /*** MAILED ITEMS SYSTEM ***/
        /*********************************************************/

        uint8 unReadMails;
        time_t m_nextMailDelivereTime;

        typedef std::unordered_map<uint32, Item*> ItemMap;

        ItemMap mMitems;                                    // template defined in objectmgr.cpp

        Item* GetMItem(uint32 id)
        {
            ItemMap::const_iterator itr = mMitems.find(id);
            return itr != mMitems.end() ? itr->second : nullptr;
        }

        void AddMItem(Item* it)
        {
            MANGOS_ASSERT(it);
            // ASSERT deleted, because items can be added before loading
            mMitems[it->GetGUIDLow()] = it;
        }

        bool RemoveMItem(uint32 id)
        {
            return mMitems.erase(id) ? true : false;
        }

        void PetSpellInitialize() const;
        void CharmSpellInitialize() const;
        void PossessSpellInitialize();
        void RemovePetActionBar() const;

        bool HasSpell(uint32 spell) const override;
        bool HasActiveSpell(uint32 spell) const;            // show in spellbook
        TrainerSpellState GetTrainerSpellState(TrainerSpell const* trainer_spell, uint32 reqLevel) const;
        bool IsSpellFitByClassAndRace(uint32 spell_id, uint32* pReqlevel = nullptr) const;
        bool IsNeedCastPassiveLikeSpellAtLearn(SpellEntry const* spellInfo) const;
        bool IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const override;

        void KnockBackFrom(Unit* target, float horizontalSpeed, float verticalSpeed);

        void SendProficiency(ItemClass itemClass, uint32 itemSubclassMask) const;
        void SendInitialSpells() const;
        bool addSpell(uint32 spell_id, bool active, bool learning, bool dependent, bool disabled);
        void learnSpell(uint32 spell_id, bool dependent);
        void removeSpell(uint32 spell_id, bool disabled = false, bool learn_low_rank = true, bool sendUpdate = true);
        void resetSpells();
        void learnDefaultSpells();
        void learnQuestRewardedSpells();
        void learnQuestRewardedSpells(Quest const* quest);
        void learnSpellHighRank(uint32 spellid);

        uint32 GetFreeTalentPoints() const { return GetUInt32Value(PLAYER_CHARACTER_POINTS1); }
        void SetFreeTalentPoints(uint32 points) { SetUInt32Value(PLAYER_CHARACTER_POINTS1, points); }
        void UpdateFreeTalentPoints(bool resetIfNeed = true);
        bool resetTalents(bool no_cost = false);
        uint32 resetTalentsCost() const;
        void InitTalentForLevel();
        void LearnTalent(uint32 talentId, uint32 talentRank);
        uint32 CalculateTalentsPoints() const;

        uint32 GetFreePrimaryProfessionPoints() const { return GetUInt32Value(PLAYER_CHARACTER_POINTS2); }
        void SetFreePrimaryProfessions(uint16 profs) { SetUInt32Value(PLAYER_CHARACTER_POINTS2, profs); }
        void InitPrimaryProfessions();

        PlayerSpellMap const& GetSpellMap() const { return m_spells; }
        PlayerSpellMap&       GetSpellMap()       { return m_spells; }

        void AddSpellMod(SpellModifier* mod, bool apply);
        bool IsAffectedBySpellmod(SpellEntry const* spellInfo, SpellModifier* mod, Spell const* spell = nullptr);
        template <class T> T ApplySpellMod(uint32 spellId, SpellModOp op, T& basevalue, Spell const* spell = nullptr);
        SpellModifier* GetSpellMod(SpellModOp op, uint32 spellId) const;
        void RemoveSpellMods(Spell const* spell);
        void ResetSpellModsDueToCanceledSpell(Spell const* spell);

        void setResurrectRequestData(ObjectGuid guid, uint32 mapId, float X, float Y, float Z, uint32 health, uint32 mana)
        {
            m_resurrectGuid = guid;
            m_resurrectMap = mapId;
            m_resurrectX = X;
            m_resurrectY = Y;
            m_resurrectZ = Z;
            m_resurrectHealth = health;
            m_resurrectMana = mana;
        }
        void clearResurrectRequestData() { setResurrectRequestData(ObjectGuid(), 0, 0.0f, 0.0f, 0.0f, 0, 0); }
        bool isRessurectRequestedBy(ObjectGuid guid) const { return m_resurrectGuid == guid; }
        bool isRessurectRequested() const { return !m_resurrectGuid.IsEmpty(); }
        void ResurectUsingRequestData();

        uint32 getCinematic() const { return m_cinematic; }
        void setCinematic(uint32 cine) { m_cinematic = cine; }

        static bool IsActionButtonDataValid(uint8 button, uint32 action, uint8 type, Player* player);
        ActionButton* addActionButton(uint8 button, uint32 action, uint8 type);
        void removeActionButton(uint8 button);
        void SendInitialActionButtons() const;

        PvPInfo pvpInfo;
        void UpdatePvP(bool state, bool overriding = false);
        void UpdatePvPContested(bool state, bool overriding = false);

        void UpdateZone(uint32 newZone, uint32 newArea);
        void UpdateArea(uint32 newArea);
        uint32 GetCachedZoneId() const { return m_zoneUpdateId; }

        void UpdateZoneDependentAuras();
        void UpdateAreaDependentAuras();                    // subzones
        void UpdateZoneDependentPets();

        void UpdateAfkReport(time_t currTime);
        void UpdatePvPFlagTimer(uint32 diff);
        void UpdatePvPContestedFlagTimer(uint32 diff);

        /** todo: -maybe move UpdateDuelFlag+DuelComplete to independent DuelHandler.. **/
        DuelInfo* duel;
        bool IsInDuelWith(Player const* player) const { return duel && duel->opponent == player && duel->startTime != 0; }
        void UpdateDuelFlag(time_t currTime);
        void CheckDuelDistance(time_t currTime);
        void DuelComplete(DuelCompleteType type);
        void SendDuelCountdown(uint32 counter) const;

        bool IsGroupVisibleFor(Player* p) const;
        bool IsInSameGroupWith(Player const* p) const;
        bool IsInSameRaidWith(Player const* p) const { return p == this || (GetGroup() != nullptr && GetGroup() == p->GetGroup()); }
        void UninviteFromGroup();
        static void RemoveFromGroup(Group* group, ObjectGuid guid);
        void RemoveFromGroup() { RemoveFromGroup(GetGroup(), GetObjectGuid()); }
        void SendUpdateToOutOfRangeGroupMembers();

        void SetInGuild(uint32 GuildId) { SetUInt32Value(PLAYER_GUILDID, GuildId); }
        void SetRank(uint32 rankId) { SetUInt32Value(PLAYER_GUILDRANK, rankId); }
        void SetGuildIdInvited(uint32 GuildId) { m_GuildIdInvited = GuildId; }
        uint32 GetGuildId() const { return GetUInt32Value(PLAYER_GUILDID);  }
        static uint32 GetGuildIdFromDB(ObjectGuid guid);
        uint32 GetRank() const { return GetUInt32Value(PLAYER_GUILDRANK); }
        static uint32 GetRankFromDB(ObjectGuid guid);
        int GetGuildIdInvited() const { return m_GuildIdInvited; }
        static void RemovePetitionsAndSigns(ObjectGuid guid, uint32 type);

        // Arena Team
        void SetInArenaTeam(uint32 ArenaTeamId, uint8 slot)
        {
            SetArenaTeamInfoField(slot, ARENA_TEAM_ID, ArenaTeamId);
        }
        void SetArenaTeamInfoField(uint8 slot, ArenaTeamInfoType type, uint32 value)
        {
            SetUInt32Value(PLAYER_FIELD_ARENA_TEAM_INFO_1_1 + (slot * ARENA_TEAM_END) + type, value);
        }
        uint32 GetArenaTeamId(uint8 slot) const { return GetUInt32Value(PLAYER_FIELD_ARENA_TEAM_INFO_1_1 + (slot * ARENA_TEAM_END) + ARENA_TEAM_ID); }
        uint32 GetArenaPersonalRating(uint8 slot) const { return GetUInt32Value(PLAYER_FIELD_ARENA_TEAM_INFO_1_1 + (slot * ARENA_TEAM_END) + ARENA_TEAM_PERSONAL_RATING); }
        static uint32 GetArenaTeamIdFromDB(ObjectGuid guid, ArenaType type);
        void SetArenaTeamIdInvited(uint32 ArenaTeamId) { m_ArenaTeamIdInvited = ArenaTeamId; }
        uint32 GetArenaTeamIdInvited() const { return m_ArenaTeamIdInvited; }
        static void LeaveAllArenaTeams(ObjectGuid guid);

        void SetDifficulty(Difficulty dungeon_difficulty) { m_dungeonDifficulty = dungeon_difficulty; }
        Difficulty GetDifficulty() const { return m_dungeonDifficulty; }

        bool CanEnterNewInstance(uint32 instanceId);
        void AddNewInstanceId(uint32 instanceId);
        void UpdateNewInstanceIdTimers(TimePoint const& now);

        bool UpdateSkill(uint32 skill_id, uint32 step);
        bool UpdateSkillPro(uint16 SkillId, int32 Chance, uint32 step);

        bool UpdateCraftSkill(uint32 spellid);
        bool UpdateGatherSkill(uint32 SkillId, uint32 SkillValue, uint32 RedLevel, uint32 Multiplicator = 1);
        bool UpdateFishingSkill();

        uint32 GetBaseDefenseSkillValue() const { return GetBaseSkillValue(SKILL_DEFENSE); }
        uint32 GetBaseWeaponSkillValue(WeaponAttackType attType) const;

        uint32 GetPureDefenseSkillValue() const { return GetPureSkillValue(SKILL_DEFENSE); }
        uint32 GetPureWeaponSkillValue(WeaponAttackType attType) const;

        float GetHealthBonusFromStamina() const;
        float GetManaBonusFromIntellect() const;

        bool UpdateStats(Stats stat) override;
        bool UpdateAllStats() override;
        void UpdateResistances(uint32 school) override;
        void UpdateArmor() override;
        void UpdateMaxHealth() override;
        void UpdateMaxPower(Powers power) override;
        void UpdateAttackPowerAndDamage(bool ranged = false) override;
        void UpdateShieldBlockValue();
        void UpdateDamagePhysical(WeaponAttackType attType) override;
        void UpdateSpellDamageAndHealingBonus();
        void ApplyRatingMod(CombatRating cr, int32 value, bool apply);
        void UpdateRating(CombatRating cr);
        void UpdateAllRatings();

        void CalculateMinMaxDamage(WeaponAttackType attType, bool normalized, float& min_damage, float& max_damage);

        void UpdateDefenseBonusesMod();
        float GetMeleeCritFromAgility() const;
        float GetDodgeFromAgility(float amount);
        float GetSpellCritFromIntellect() const;
        float OCTRegenHPPerSpirit() const;
        float OCTRegenMPPerSpirit() const;
        float GetRatingMultiplier(CombatRating cr) const;
        float GetRatingBonusValue(CombatRating cr) const;

        void UpdateBlockPercentage();
        void UpdateCritPercentage(WeaponAttackType attType);
        void UpdateAllCritPercentages();
        void UpdateParryPercentage();
        void UpdateDodgePercentage();
        void UpdateMeleeHitChances();
        void UpdateRangedHitChances();
        void UpdateSpellHitChances();

        void UpdateAllSpellCritChances();
        void UpdateSpellCritChance(uint32 school);
        void UpdateExpertise(WeaponAttackType attType);
        void UpdateManaRegen();

        ObjectGuid const& GetLootGuid() const { return m_lootGuid; }
        void SetLootGuid(ObjectGuid const& guid) { m_lootGuid = guid; }

        void RemovedInsignia(Player* looterPlr);

        WorldSession* GetSession() const { return m_session; }
        void SetSession(WorldSession* s) { m_session = s; }

        void BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const override;
        void DestroyForPlayer(Player* target) const override;
        void SendLogXPGain(uint32 GivenXP, Unit* victim, uint32 RestXP) const;

        uint8 LastSwingErrorMsg() const { return m_swingErrorMsg; }
        void SwingErrorMsg(uint8 val) { m_swingErrorMsg = val; }

        // notifiers
        void SendAttackSwingCantAttack() const;
        void SendAttackSwingCancelAttack() const;
        void SendAttackSwingDeadTarget() const;
        void SendAttackSwingNotStanding() const;
        void SendAttackSwingNotInRange() const;
        void SendAttackSwingBadFacingAttack() const;
        void SendAutoRepeatCancel() const;
        void SendFeignDeathResisted() const;
        void SendExplorationExperience(uint32 Area, uint32 Experience) const;

        void SendDungeonDifficulty(bool IsInGroup) const;
        void ResetInstances(InstanceResetMethod method);
        void SendResetInstanceSuccess(uint32 MapId) const;
        void SendResetInstanceFailed(uint32 reason, uint32 MapId) const;
        void SendResetFailedNotify(uint32 mapid) const;

        bool SetPosition(float x, float y, float z, float orientation, bool teleport = false);
        void UpdateUnderwaterState(Map* m, float x, float y, float z);

        void SendMessageToSet(WorldPacket const& data, bool self) const override;// overwrite Object::SendMessageToSet
        void SendMessageToSetInRange(WorldPacket const& data, float fist, bool self) const override;
        // overwrite Object::SendMessageToSetInRange
        void SendMessageToSetInRange(WorldPacket const& data, float dist, bool self, bool own_team_only) const;

        Corpse* GetCorpse() const;
        void SpawnCorpseBones();
        Corpse* CreateCorpse();
        void KillPlayer();
        uint32 GetResurrectionSpellId() const;
        void ResurrectPlayer(float restore_percent, bool applySickness = false);
        void BuildPlayerRepop();
        void RepopAtGraveyard();

        void DurabilityLossAll(double percent, bool inventory);
        void DurabilityLoss(Item* item, double percent);
        void DurabilityPointsLossAll(int32 points, bool inventory);
        void DurabilityPointsLoss(Item* item, int32 points);
        void DurabilityPointLossForEquipSlot(EquipmentSlots slot);
        uint32 DurabilityRepairAll(bool cost, float discountMod, bool guildBank);
        uint32 DurabilityRepair(uint16 pos, bool cost, float discountMod, bool guildBank);

        void UpdateMirrorTimers();
        void StopMirrorTimers()
        {
            StopMirrorTimer(FATIGUE_TIMER);
            StopMirrorTimer(BREATH_TIMER);
            StopMirrorTimer(FIRE_TIMER);
        }

        void SetLevitate(bool enable) override;
        void SetCanFly(bool enable) override;
        void SetFeatherFall(bool enable) override;
        void SetHover(bool enable) override;
        void SetRoot(bool enable) override;
        void SetWaterWalk(bool enable) override;

        void JoinedChannel(Channel* c);
        void LeftChannel(Channel* c);
        void CleanupChannels();
        void UpdateLocalChannels(uint32 newZone);
        void LeaveLFGChannel();

        void UpdateDefense();
        void UpdateWeaponSkill(WeaponAttackType attType);
        void UpdateCombatSkills(Unit* pVictim, WeaponAttackType attType, bool defence);

        void SetSkill(uint16 id, uint16 currVal, uint16 maxVal, uint16 step = 0);
        uint16 GetMaxSkillValue(uint32 skill) const;        // max + perm. bonus + temp bonus
        uint16 GetPureMaxSkillValue(uint32 skill) const;    // max
        uint16 GetSkillValue(uint32 skill) const;           // skill value + perm. bonus + temp bonus
        uint16 GetBaseSkillValue(uint32 skill) const;       // skill value + perm. bonus
        uint16 GetPureSkillValue(uint32 skill) const;       // skill value
        int16 GetSkillPermBonusValue(uint32 skill) const;
        int16 GetSkillTempBonusValue(uint32 skill) const;
        bool HasSkill(uint32 skill) const;
        void learnSkillRewardedSpells(uint32 id, uint32 value);

        WorldLocation& GetTeleportDest() { return m_teleport_dest; }
        bool IsBeingTeleported() const { return mSemaphoreTeleport_Near || mSemaphoreTeleport_Far; }
        bool IsBeingTeleportedNear() const { return mSemaphoreTeleport_Near; }
        bool IsBeingTeleportedFar() const { return mSemaphoreTeleport_Far; }
        void SetSemaphoreTeleportNear(bool semphsetting) { mSemaphoreTeleport_Near = semphsetting; }
        void SetSemaphoreTeleportFar(bool semphsetting) { mSemaphoreTeleport_Far = semphsetting; }
        void ProcessDelayedOperations();

        void CheckAreaExploreAndOutdoor();

        static Team TeamForRace(uint8 race);
        Team GetTeam() const { return m_team; }
        static uint32 getFactionForRace(uint8 race);
        void setFactionForRace(uint8 race);

        void InitDisplayIds();

        bool IsAtGroupRewardDistance(WorldObject const* pRewardSource) const;
        void RewardSinglePlayerAtKill(Unit* pVictim);
        void RewardPlayerAndGroupAtEvent(uint32 creature_id, WorldObject* pRewardSource);
        void RewardPlayerAndGroupAtCast(WorldObject* pRewardSource, uint32 spellid = 0);
        bool isHonorOrXPTarget(Unit* pVictim) const;

        ReputationMgr&       GetReputationMgr()       { return m_reputationMgr; }
        ReputationMgr const& GetReputationMgr() const { return m_reputationMgr; }
        ReputationRank GetReputationRank(uint32 faction_id) const;
        void RewardReputation(Unit* pVictim, float rate);
        void RewardReputation(Quest const* pQuest);
        int32 CalculateReputationGain(ReputationSource source, int32 rep, int32 maxRep, int32 faction, uint32 creatureOrQuestLevel = 0, bool noAuraBonus = false);

        void UpdateSkillsForLevel();
        void UpdateSkillsToMaxSkillsForLevel();             // for .levelup
        void ModifySkillBonus(uint32 skillid, int32 val, bool talent);

        /*********************************************************/
        /***                  PVP SYSTEM                       ***/
        /*********************************************************/
        void UpdateArenaFields();
        void UpdateHonorFields();
        bool RewardHonor(Unit* pVictim, uint32 groupsize, float honor = -1);
        uint32 GetHonorPoints() const { return GetUInt32Value(PLAYER_FIELD_HONOR_CURRENCY); }
        uint32 GetArenaPoints() const { return GetUInt32Value(PLAYER_FIELD_ARENA_CURRENCY); }
        void SetHonorPoints(uint32 value);
        void SetArenaPoints(uint32 value);
        void ModifyHonorPoints(int32 value);
        void ModifyArenaPoints(int32 value);

        uint8 GetHighestPvPRankIndex() const;
        uint32 GetMaxPersonalArenaRatingRequirement();

        // End of PvP System

        void SetDrunkValue(uint16 newDrunkValue, uint32 itemid = 0);
        uint16 GetDrunkValue() const { return m_drunk; }
        static DrunkenState GetDrunkenstateByValue(uint16 value);

        uint32 GetDeathTimer() const { return m_deathTimer; }
        uint32 GetCorpseReclaimDelay(bool pvp) const;
        void UpdateCorpseReclaimDelay();
        void SendCorpseReclaimDelay(bool load = false) const;

        uint32 GetShieldBlockValue() const override;        // overwrite Unit version (virtual)
        bool CanDualWield() const { return m_canDualWield; }
        void SetCanDualWield(bool value) { m_canDualWield = value; }

        void SetRegularAttackTime();
        void SetBaseModValue(BaseModGroup modGroup, BaseModType modType, float value) { m_auraBaseMod[modGroup][modType] = value; }
        void HandleBaseModValue(BaseModGroup modGroup, BaseModType modType, float amount, bool apply);
        float GetBaseModValue(BaseModGroup modGroup, BaseModType modType) const;
        float GetTotalBaseModValue(BaseModGroup modGroup) const;
        float GetTotalPercentageModValue(BaseModGroup modGroup) const { return m_auraBaseMod[modGroup][FLAT_MOD] + m_auraBaseMod[modGroup][PCT_MOD]; }
        void _ApplyAllStatBonuses();
        void _RemoveAllStatBonuses();
        void SetEnchantmentModifier(uint32 value, WeaponAttackType attType, bool apply);
        uint32 GetEnchantmentModifier(WeaponAttackType attType);

        void _ApplyWeaponDependentAuraMods(Item* item, WeaponAttackType attackType, bool apply);
        void _ApplyWeaponDependentAuraCritMod(Item* item, WeaponAttackType attackType, Aura* aura, bool apply);
        void _ApplyWeaponDependentAuraDamageMod(Item* item, WeaponAttackType attackType, Aura* aura, bool apply);

        void _ApplyItemMods(Item* item, uint8 slot, bool apply);
        void _RemoveAllItemMods();
        void _ApplyAllItemMods();
        void _ApplyItemBonuses(ItemPrototype const* proto, uint8 slot, bool apply);
        void _ApplyAmmoBonuses();
        bool EnchantmentFitsRequirements(uint32 enchantmentcondition, int8 slot) const;
        void ToggleMetaGemsActive(uint8 exceptslot, bool apply);
        void CorrectMetaGemEnchants(uint8 slot, bool apply);
        void InitDataForForm(bool reapplyMods = false);

        void ApplyItemEquipSpell(Item* item, bool apply, bool form_change = false);
        void ApplyEquipSpell(SpellEntry const* spellInfo, Item* item, bool apply, bool form_change = false);
        void UpdateEquipSpellsAtFormChange();
        void CastItemCombatSpell(Unit* Target, WeaponAttackType attType);
        void CastItemUseSpell(Item* item, SpellCastTargets const& targets, uint8 cast_count, uint8 spell_index);

        void ApplyItemOnStoreSpell(Item* item, bool apply);
        void DestroyItemWithOnStoreSpell(Item* item, uint32 spellId);

        void SendInitWorldStates(uint32 zone, uint32 area) const;
        void SendUpdateWorldState(uint32 Field, uint32 Value) const;
        void SendDirectMessage(WorldPacket const& data) const;

        void SendAuraDurationsForTarget(Unit* target);

        PlayerMenu* PlayerTalkClass;
        std::vector<ItemSetEffect*> ItemSetEff;

        /*********************************************************/
        /***               BATTLEGROUND SYSTEM                 ***/
        /*********************************************************/

        bool InBattleGround()       const                { return m_bgData.bgInstanceID != 0; }
        bool InArena()              const;
        uint32 GetBattleGroundId()  const                { return m_bgData.bgInstanceID; }
        BattleGroundTypeId GetBattleGroundTypeId() const { return m_bgData.bgTypeID; }
        BattleGround* GetBattleGround() const;

        static uint32 GetMinLevelForBattleGroundBracketId(BattleGroundBracketId bracket_id, BattleGroundTypeId bgTypeId);
        static uint32 GetMaxLevelForBattleGroundBracketId(BattleGroundBracketId bracket_id, BattleGroundTypeId bgTypeId);
        BattleGroundBracketId GetBattleGroundBracketIdFromLevel(BattleGroundTypeId bgTypeId) const;

        bool InBattleGroundQueue() const
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
                if (m_bgBattleGroundQueueID[i].bgQueueTypeId != BATTLEGROUND_QUEUE_NONE)
                    return true;
            return false;
        }

        BattleGroundQueueTypeId GetBattleGroundQueueTypeId(uint32 index) const { return m_bgBattleGroundQueueID[index].bgQueueTypeId; }
        uint32 GetBattleGroundQueueIndex(BattleGroundQueueTypeId bgQueueTypeId) const
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
                if (m_bgBattleGroundQueueID[i].bgQueueTypeId == bgQueueTypeId)
                    return i;
            return PLAYER_MAX_BATTLEGROUND_QUEUES;
        }
        bool IsInvitedForBattleGroundQueueType(BattleGroundQueueTypeId bgQueueTypeId) const
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
                if (m_bgBattleGroundQueueID[i].bgQueueTypeId == bgQueueTypeId)
                    return m_bgBattleGroundQueueID[i].invitedToInstance != 0;
            return false;
        }
        bool InBattleGroundQueueForBattleGroundQueueType(BattleGroundQueueTypeId bgQueueTypeId) const
        {
            return GetBattleGroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES;
        }

        void SetBattleGroundId(uint32 val, BattleGroundTypeId bgTypeId)
        {
            m_bgData.bgInstanceID = val;
            m_bgData.bgTypeID = bgTypeId;
            m_bgData.m_needSave = true;
        }
        uint32 AddBattleGroundQueueId(BattleGroundQueueTypeId val)
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
            {
                if (m_bgBattleGroundQueueID[i].bgQueueTypeId == BATTLEGROUND_QUEUE_NONE || m_bgBattleGroundQueueID[i].bgQueueTypeId == val)
                {
                    m_bgBattleGroundQueueID[i].bgQueueTypeId = val;
                    m_bgBattleGroundQueueID[i].invitedToInstance = 0;
                    return i;
                }
            }
            return PLAYER_MAX_BATTLEGROUND_QUEUES;
        }
        bool HasFreeBattleGroundQueueId() const
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
                if (m_bgBattleGroundQueueID[i].bgQueueTypeId == BATTLEGROUND_QUEUE_NONE)
                    return true;
            return false;
        }

        void RemoveBattleGroundQueueId(BattleGroundQueueTypeId val)
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
            {
                if (m_bgBattleGroundQueueID[i].bgQueueTypeId == val)
                {
                    m_bgBattleGroundQueueID[i].bgQueueTypeId = BATTLEGROUND_QUEUE_NONE;
                    m_bgBattleGroundQueueID[i].invitedToInstance = 0;
                    return;
                }
            }
        }

        void SetInviteForBattleGroundQueueType(BattleGroundQueueTypeId bgQueueTypeId, uint32 instanceId)
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
                if (m_bgBattleGroundQueueID[i].bgQueueTypeId == bgQueueTypeId)
                    m_bgBattleGroundQueueID[i].invitedToInstance = instanceId;
        }

        bool IsInvitedForBattleGroundInstance(uint32 instanceId) const
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
                if (m_bgBattleGroundQueueID[i].invitedToInstance == instanceId)
                    return true;
            return false;
        }

        WorldLocation const& GetBattleGroundEntryPoint() const { return m_bgData.joinPos; }
        void SetBattleGroundEntryPoint(Player* leader = nullptr);

        void SetBGTeam(Team team) { m_bgData.bgTeam = team; m_bgData.m_needSave = true; }
        Team GetBGTeam() const { return m_bgData.bgTeam ? m_bgData.bgTeam : GetTeam(); }

        void LeaveBattleground(bool teleportToEntryPoint = true);
        bool CanJoinToBattleground() const;
        bool CanReportAfkDueToLimit();
        void ReportedAfkBy(Player* reporter);
        void ClearAfkReports() { m_bgData.bgAfkReporter.clear(); }

        bool GetBGAccessByLevel(BattleGroundTypeId bgTypeId) const;
        bool CanUseBattleGroundObject();
        bool isTotalImmune() const;

        // returns true if the player is in active state for capture point capturing
        bool CanUseCapturePoint() const;

        /*********************************************************/
        /***                    REST SYSTEM                    ***/
        /*********************************************************/

        bool isRested() const { return GetRestTime() >= 10 * IN_MILLISECONDS; }
        uint32 GetXPRestBonus(uint32 xp);
        uint32 GetRestTime() const { return m_restTime; }
        void SetRestTime(uint32 v) { m_restTime = v; }

        /*********************************************************/
        /***              ENVIROMENTAL SYSTEM                  ***/
        /*********************************************************/

        uint32 EnvironmentalDamage(EnviromentalDamage type, uint32 damage);

        /*********************************************************/
        /***               FLOOD FILTER SYSTEM                 ***/
        /*********************************************************/

        void UpdateSpeakTime();
        bool CanSpeak() const;

        /*********************************************************/
        /***                 VARIOUS SYSTEMS                   ***/
        /*********************************************************/
        bool HasMovementFlag(MovementFlags f) const;        // for script access to m_movementInfo.HasMovementFlag
        void UpdateFallInformationIfNeed(MovementInfo const& minfo, uint16 opcode);
        void SetFallInformation(uint32 time, float z)
        {
            m_lastFallTime = time;
            m_lastFallZ = z;
        }
        void HandleFall(MovementInfo const& movementInfo);

        void BuildTeleportAckMsg(WorldPacket& data, float x, float y, float z, float ang) const;

        bool isMovingOrTurning() const { return m_movementInfo.HasMovementFlag(movementOrTurningFlagsMask); }

        bool CanSwim() const { return true; }
        bool CanFly() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_CAN_FLY); }
        bool CanWalk() const { return true; }
        bool IsFlying() const { return m_movementInfo.HasMovementFlag(MOVEFLAG_FLYING); }
        bool IsFreeFlying() const { return HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED) || HasAuraType(SPELL_AURA_FLY); }

        void UpdateClientControl(Unit const* target, bool enabled, bool forced = false) const;

        void SetMover(Unit* target) { m_mover = target ? target : this; }
        Unit* GetMover() const { return m_mover; }
        bool IsSelfMover() const { return m_mover == this; }// normal case for player not controlling other unit

        ObjectGuid const& GetFarSightGuid() const { return GetGuidValue(PLAYER_FARSIGHT); }

        // Transports
        Transport* GetTransport() const { return m_transport; }
        void SetTransport(Transport* t) { m_transport = t; }

        float GetTransOffsetX() const { return m_movementInfo.GetTransportPos()->x; }
        float GetTransOffsetY() const { return m_movementInfo.GetTransportPos()->y; }
        float GetTransOffsetZ() const { return m_movementInfo.GetTransportPos()->z; }
        float GetTransOffsetO() const { return m_movementInfo.GetTransportPos()->o; }
        uint32 GetTransTime() const { return m_movementInfo.GetTransportTime(); }

        uint32 GetSaveTimer() const { return m_nextSave; }
        void   SetSaveTimer(uint32 timer) { m_nextSave = timer; }

        // Recall position
        uint32 m_recallMap;
        float  m_recallX;
        float  m_recallY;
        float  m_recallZ;
        float  m_recallO;
        void   SaveRecallPosition();

        void SetHomebindToLocation(WorldLocation const& loc, uint32 area_id);
        void RelocateToHomebind() { SetLocationMapId(m_homebindMapId); Relocate(m_homebindX, m_homebindY, m_homebindZ); }
        bool TeleportToHomebind(uint32 options = 0) { return TeleportTo(m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ, GetOrientation(), options); }

        Object* GetObjectByTypeMask(ObjectGuid guid, TypeMask typemask);

        // currently visible objects at player client
        GuidSet m_clientGUIDs;

        bool HaveAtClient(WorldObject const* u) { return u == this || m_clientGUIDs.find(u->GetObjectGuid()) != m_clientGUIDs.end(); }

        bool IsVisibleInGridForPlayer(Player* pl) const override;
        bool IsVisibleGloballyFor(Player* pl) const;

        void UpdateVisibilityOf(WorldObject const* viewPoint, WorldObject* target);

        template<class T>
        void UpdateVisibilityOf(WorldObject const* viewPoint, T* target, UpdateData& data, std::set<WorldObject*>& visibleNow);

        // Stealth detection system
        void HandleStealthedUnitsDetection();

        Camera& GetCamera() { return m_camera; }

        uint8 m_forced_speed_changes[MAX_MOVE_TYPE];

        bool HasAtLoginFlag(AtLoginFlags f) const { return !!(m_atLoginFlags & f); }
        void SetAtLoginFlag(AtLoginFlags f) { m_atLoginFlags |= f; }
        void RemoveAtLoginFlag(AtLoginFlags f, bool in_db_also = false);

        LookingForGroup m_lookingForGroup;

        // Temporarily removed pet cache
        uint32 GetTemporaryUnsummonedPetNumber() const { return m_temporaryUnsummonedPetNumber; }
        void SetTemporaryUnsummonedPetNumber(uint32 petnumber) { m_temporaryUnsummonedPetNumber = petnumber; }
        void UnsummonPetTemporaryIfAny();
        void UnsummonPetIfAny();
        void ResummonPetTemporaryUnSummonedIfAny();
        bool IsPetNeedBeTemporaryUnsummoned() const;

        void SendCinematicStart(uint32 CinematicSequenceId) const;

        /*********************************************************/
        /***                 INSTANCE SYSTEM                   ***/
        /*********************************************************/

        typedef std::unordered_map < uint32 /*mapId*/, InstancePlayerBind > BoundInstancesMap;

        void UpdateHomebindTime(uint32 time);

        uint32 m_HomebindTimer;
        bool m_InstanceValid;
        // permanent binds and solo binds by difficulty
        BoundInstancesMap m_boundInstances[MAX_DIFFICULTY];
        InstancePlayerBind* GetBoundInstance(uint32 mapid, Difficulty difficulty);
        BoundInstancesMap& GetBoundInstances(Difficulty difficulty) { return m_boundInstances[difficulty]; }
        void UnbindInstance(uint32 mapid, Difficulty difficulty, bool unload = false);
        void UnbindInstance(BoundInstancesMap::iterator& itr, Difficulty difficulty, bool unload = false);
        InstancePlayerBind* BindToInstance(DungeonPersistentState* save, bool permanent, bool load = false);
        void SendRaidInfo();
        void SendSavedInstances();
        static void ConvertInstancesToGroup(Player* player, Group* group = nullptr, ObjectGuid player_guid = ObjectGuid());
        DungeonPersistentState* GetBoundInstanceSaveForSelfOrGroup(uint32 mapid);

        AreaLockStatus GetAreaTriggerLockStatus(AreaTrigger const* at, uint32& miscRequirement);
        void SendTransferAbortedByLockStatus(MapEntry const* mapEntry, AreaTrigger const* at, AreaLockStatus lockStatus, uint32 miscRequirement = 0) const;

        /*********************************************************/
        /***                   GROUP SYSTEM                    ***/
        /*********************************************************/

        Group* GetGroupInvite() const { return m_groupInvite; }
        void SetGroupInvite(Group* group) { m_groupInvite = group; }
        Group* GetGroup() { return m_group.getTarget(); }
        const Group* GetGroup() const { return (const Group*)m_group.getTarget(); }
        GroupReference& GetGroupRef() { return m_group; }
        void SetGroup(Group* group, int8 subgroup = -1);
        uint8 GetSubGroup() const { return m_group.getSubGroup(); }
        uint32 GetGroupUpdateFlag() const { return m_groupUpdateMask; }
        void SetGroupUpdateFlag(uint32 flag) { m_groupUpdateMask |= flag; }
        const uint64& GetAuraUpdateMask() const { return m_auraUpdateMask; }
        void SetAuraUpdateMask(uint8 slot) { m_auraUpdateMask |= (uint64(1) << slot); }
        Player* GetNextRaidMemberWithLowestLifePercentage(float radius, AuraType noAuraType);
        PartyResult CanUninviteFromGroup() const;
        void UpdateGroupLeaderFlag(const bool remove = false);
        // BattleGround Group System
        void SetBattleGroundRaid(Group* group, int8 subgroup = -1);
        void RemoveFromBattleGroundRaid();
        Group* GetOriginalGroup() const { return m_originalGroup.getTarget(); }
        GroupReference& GetOriginalGroupRef() { return m_originalGroup; }
        uint8 GetOriginalSubGroup() const { return m_originalGroup.getSubGroup(); }
        void SetOriginalGroup(Group* group, int8 subgroup = -1);

        GridReference<Player>& GetGridRef() { return m_gridRef; }
        MapReference& GetMapRef() { return m_mapRef; }

        DeclinedName const* GetDeclinedNames() const { return m_declinedname; }
        bool HasTitle(uint32 bitIndex) const;
        bool HasTitle(CharTitlesEntry const* title) const { return HasTitle(title->bit_index); }
        void SetTitle(CharTitlesEntry const* title, bool lost = false);

#ifdef BUILD_PLAYERBOT
        // A Player can either have a playerbotMgr (to manage its bots), or have playerbotAI (if it is a bot), or
        // neither. Code that enables bots must create the playerbotMgr and set it using SetPlayerbotMgr.
        void SetPlayerbotAI(PlayerbotAI* ai) { assert(!m_playerbotAI && !m_playerbotMgr); m_playerbotAI = ai; }
        PlayerbotAI* GetPlayerbotAI() { return m_playerbotAI; }
        void SetPlayerbotMgr(PlayerbotMgr* mgr) { assert(!m_playerbotAI && !m_playerbotMgr); m_playerbotMgr = mgr; }
        PlayerbotMgr* GetPlayerbotMgr() { return m_playerbotMgr; }
        void SetBotDeathTimer() { m_deathTimer = 0; }
        bool IsInDuel() const { return duel && duel->startTime != 0; }
#endif
        virtual CreatureAI* AI() override { if (m_charmInfo) return m_charmInfo->GetAI(); return nullptr; }
        virtual CombatData* GetCombatData() override { if (m_charmInfo && m_charmInfo->GetCombatData()) return m_charmInfo->GetCombatData(); return m_combatData; }

        void SendLootError(ObjectGuid guid, LootError error) const;

        // cooldown system
        virtual void AddGCD(SpellEntry const& spellEntry, uint32 forcedDuration = 0, bool updateClient = 0) override;
        virtual void AddCooldown(SpellEntry const& spellEntry, ItemPrototype const* itemProto = nullptr, bool permanent = false, uint32 forcedDuration = 0) override;
        virtual void RemoveSpellCooldown(SpellEntry const& spellEntry, bool updateClient = true) override;
        virtual void RemoveSpellCategoryCooldown(uint32 category, bool updateClient = true) override;
        virtual void RemoveAllCooldowns(bool sendOnly = false);
        virtual void LockOutSpells(SpellSchoolMask schoolMask, uint32 duration) override;
        void RemoveSpellLockout(SpellSchoolMask spellSchoolMask, std::set<uint32>* spellAlreadySent = nullptr);
        void SendClearCooldown(uint32 spell_id, Unit* target) const;
        void RemoveArenaSpellCooldowns();
        void _LoadSpellCooldowns(QueryResult* result);
        void _SaveSpellCooldowns();

        template <typename F>
        void RemoveSomeCooldown(F check)
        {
            auto spellCDItr = m_cooldownMap.begin();
            while (spellCDItr != m_cooldownMap.end())
            {
                SpellEntry const* entry = sSpellTemplate.LookupEntry<SpellEntry>(spellCDItr->first);
                if (entry && check(*entry))
                {
                    SendClearCooldown(spellCDItr->first, this);
                    spellCDItr = m_cooldownMap.erase(spellCDItr);
                }
                else
                    ++spellCDItr;
            }
        }

    protected:
        /*********************************************************/
        /***               BATTLEGROUND SYSTEM                 ***/
        /*********************************************************/

        /*
        this is an array of BG queues (BgTypeIDs) in which is player
        */
        struct BgBattleGroundQueueID_Rec
        {
            BattleGroundQueueTypeId bgQueueTypeId;
            uint32 invitedToInstance;
        };

        BgBattleGroundQueueID_Rec m_bgBattleGroundQueueID[PLAYER_MAX_BATTLEGROUND_QUEUES];
        BGData                    m_bgData;

        /*********************************************************/
        /***                    QUEST SYSTEM                   ***/
        /*********************************************************/

        // We allow only one timed quest active at the same time. Below can then be simple value instead of set.
        typedef std::set<uint32> QuestSet;
        QuestSet m_timedquests;
        QuestSet m_weeklyquests;
        QuestSet m_monthlyquests;

        ObjectGuid m_dividerGuid;
        uint32 m_ingametime;

        /*********************************************************/
        /***                   LOAD SYSTEM                     ***/
        /*********************************************************/

        void _LoadActions(QueryResult* result);
        void _LoadAuras(QueryResult* result, uint32 timediff);
        void _LoadBoundInstances(QueryResult* result);
        void _LoadInventory(QueryResult* result, uint32 timediff);
        void _LoadItemLoot(QueryResult* result);
        void _LoadMails(QueryResult* result);
        void _LoadMailedItems(QueryResult* result);
        void _LoadQuestStatus(QueryResult* result);
        void _LoadDailyQuestStatus(QueryResult* result);
        void _LoadWeeklyQuestStatus(QueryResult* result);
        void _LoadMonthlyQuestStatus(QueryResult* result);
        void _LoadGroup(QueryResult* result);
        void _LoadSkills(QueryResult* result);
        void _LoadSpells(QueryResult* result);
        bool _LoadHomeBind(QueryResult* result);
        void _LoadDeclinedNames(QueryResult* result);
        void _LoadArenaTeamInfo(QueryResult* result);
        void _LoadBGData(QueryResult* result);
        void _LoadIntoDataField(const char* data, uint32 startOffset, uint32 count);
        void _LoadCreatedInstanceTimers();
        void _SaveNewInstanceIdTimer();

        /*********************************************************/
        /***                   SAVE SYSTEM                     ***/
        /*********************************************************/

        void _SaveActions();
        void _SaveAuras();
        void _SaveInventory();
        void _SaveMail();
        void _SaveQuestStatus();
        void _SaveDailyQuestStatus();
        void _SaveWeeklyQuestStatus();
        void _SaveMonthlyQuestStatus();
        void _SaveSkills();
        void _SaveSpells();
        void _SaveBGData();
        void _SaveStats();

        void _SetCreateBits(UpdateMask* updateMask, Player* target) const override;
        void _SetUpdateBits(UpdateMask* updateMask, Player* target) const override;

        /*********************************************************/
        /***              ENVIRONMENTAL SYSTEM                 ***/
        /*********************************************************/
        void HandleSobering();
        void SendMirrorTimer(MirrorTimerType Type, uint32 MaxValue, uint32 CurrentValue, int32 Regen);
        void StopMirrorTimer(MirrorTimerType Type);
        void HandleDrowning(uint32 time_diff);
        int32 getMaxTimer(MirrorTimerType timer) const;

        /*********************************************************/
        /***                  HONOR SYSTEM                     ***/
        /*********************************************************/
        time_t m_lastHonorUpdateTime;

        void outDebugStatsValues() const;
        ObjectGuid m_lootGuid;

        Team m_team;
        uint32 m_nextSave;
        time_t m_speakTime;
        uint32 m_speakCount;
        Difficulty m_dungeonDifficulty;

        uint32 m_atLoginFlags;

        Item* m_items[PLAYER_SLOTS_COUNT];
        uint32 m_currentBuybackSlot;

        std::vector<Item*> m_itemUpdateQueue;
        bool m_itemUpdateQueueBlocked;

        uint32 m_ExtraFlags;
        ObjectGuid m_curSelectionGuid;

        ObjectGuid m_comboTargetGuid;
        int8 m_comboPoints;

        QuestStatusMap mQuestStatus;

        SkillStatusMap mSkillStatus;

        uint32 m_GuildIdInvited;
        uint32 m_ArenaTeamIdInvited;

        PlayerMails m_mail;
        PlayerSpellMap m_spells;

        ActionButtonList m_actionButtons;

        float m_auraBaseMod[BASEMOD_END][MOD_END];
        int16 m_baseRatingValue[MAX_COMBAT_RATING];

        uint32 m_enchantmentFlatMod[MAX_ATTACK]; // TODO: Stat system - incorporate generically, exposes a required hidden weapon stat that does not apply when unarmed

        SpellModList m_spellMods[MAX_SPELLMOD];
        int32 m_SpellModRemoveCount;
        EnchantDurationList m_enchantDuration;
        ItemDurationList m_itemDuration;

        ObjectGuid m_resurrectGuid;
        uint32 m_resurrectMap;
        float m_resurrectX, m_resurrectY, m_resurrectZ;
        uint32 m_resurrectHealth, m_resurrectMana;

        WorldSession* m_session;

        typedef std::list<Channel*> JoinedChannelsList;
        JoinedChannelsList m_channels;

        uint32 m_cinematic;

        TradeData* m_trade;

        bool   m_DailyQuestChanged;
        bool   m_WeeklyQuestChanged;
        bool   m_MonthlyQuestChanged;

        uint32 m_drunkTimer;
        uint16 m_drunk;
        uint32 m_weaponChangeTimer;

        uint32 m_zoneUpdateId;
        uint32 m_zoneUpdateTimer;
        uint32 m_areaUpdateId;
        uint32 m_positionStatusUpdateTimer;

        uint32 m_deathTimer;
        time_t m_deathExpireTime;

        uint32 m_restTime;

        uint32 m_WeaponProficiency;
        uint32 m_ArmorProficiency;
        bool m_canDualWield;
        uint8 m_swingErrorMsg;
        float m_ammoDPS;

        //////////////////// Rest System/////////////////////
        time_t time_inn_enter;
        uint32 inn_trigger_id;
        float m_rest_bonus;
        RestType rest_type;
        //////////////////// Rest System/////////////////////

        // Transports
        Transport* m_transport;

        uint32 m_resetTalentsCost;
        time_t m_resetTalentsTime;
        uint32 m_usedTalentCount;

        // Social
        PlayerSocial* m_social;

        // Groups
        GroupReference m_group;
        GroupReference m_originalGroup;
        Group* m_groupInvite;
        uint32 m_groupUpdateMask;
        uint64 m_auraUpdateMask;

        // Player summoning
        time_t m_summon_expire;
        uint32 m_summon_mapid;
        float  m_summon_x;
        float  m_summon_y;
        float  m_summon_z;

        DeclinedName* m_declinedname;
    private:
        // internal common parts for CanStore/StoreItem functions
        InventoryResult _CanStoreItem_InSpecificSlot(uint8 bag, uint8 slot, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool swap, Item* pSrcItem) const;
        InventoryResult _CanStoreItem_InBag(uint8 bag, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool merge, bool non_specialized, Item* pSrcItem, uint8 skip_bag, uint8 skip_slot) const;
        InventoryResult _CanStoreItem_InInventorySlots(uint8 slot_begin, uint8 slot_end, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool merge, Item* pSrcItem, uint8 skip_bag, uint8 skip_slot) const;
        Item* _StoreItem(uint16 pos, Item* pItem, uint32 count, bool clone, bool update);

        void AdjustQuestReqItemCount(Quest const* pQuest, QuestStatusData& questStatusData);

        void SetCanDelayTeleport(bool setting) { m_bCanDelayTeleport = setting; }
        bool IsHasDelayedTeleport() const
        {
            // we should not execute delayed teleports for now dead players but has been alive at teleport
            // because we don't want player's ghost teleported from graveyard
            return m_bHasDelayedTeleport && (isAlive() || !m_bHasBeenAliveAtDelayedTeleport);
        }

        bool SetDelayedTeleportFlagIfCan()
        {
            m_bHasDelayedTeleport = m_bCanDelayTeleport;
            m_bHasBeenAliveAtDelayedTeleport = isAlive();
            return m_bHasDelayedTeleport;
        }

        void ScheduleDelayedOperation(uint32 operation)
        {
            if (operation < DELAYED_END)
                m_DelayedOperations |= operation;
        }

        Unit* m_mover;
        Camera m_camera;

        GridReference<Player> m_gridRef;
        MapReference m_mapRef;

#ifdef BUILD_PLAYERBOT
        PlayerbotAI* m_playerbotAI;
        PlayerbotMgr* m_playerbotMgr;
#endif

        // Homebind coordinates
        uint32 m_homebindMapId;
        uint16 m_homebindAreaId;
        float m_homebindX;
        float m_homebindY;
        float m_homebindZ;

        uint32 m_lastFallTime;
        float  m_lastFallZ;

        LiquidTypeEntry const* m_lastLiquid;

        int32 m_MirrorTimer[MAX_TIMERS];
        uint8 m_MirrorTimerFlags;
        uint8 m_MirrorTimerFlagsLast;
        bool m_isInWater;

        // Current teleport data
        WorldLocation m_teleport_dest;
        uint32 m_teleport_options;
        bool mSemaphoreTeleport_Near;
        bool mSemaphoreTeleport_Far;

        uint32 m_DelayedOperations;
        bool m_bCanDelayTeleport;
        bool m_bHasDelayedTeleport;
        bool m_bHasBeenAliveAtDelayedTeleport;

        uint32 m_DetectInvTimer;

        // Temporary removed pet cache
        uint32 m_temporaryUnsummonedPetNumber;

        ReputationMgr  m_reputationMgr;

        uint32 m_timeSyncCounter;
        uint32 m_timeSyncTimer;
        uint32 m_timeSyncClient;
        uint32 m_timeSyncServer;

        std::unordered_map<uint32, TimePoint> m_enteredInstances;
        uint32 m_createdInstanceClearTimer;
};

void AddItemsSetItem(Player* player, Item* item);
void RemoveItemsSetItem(Player* player, ItemPrototype const* proto);

// "the bodies of template functions must be made available in a header file"
template <class T> T Player::ApplySpellMod(uint32 spellId, SpellModOp op, T& basevalue, Spell const* spell)
{
    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
    if (!spellInfo) return 0;
    int32 totalpct = 0;
    int32 addedFlat = 0;
    for (SpellModList::iterator itr = m_spellMods[op].begin(); itr != m_spellMods[op].end(); ++itr)
    {
        SpellModifier* mod = *itr;

        if (!IsAffectedBySpellmod(spellInfo, mod, spell))
            continue;
        if (mod->type == SPELLMOD_FLAT)
            addedFlat += mod->value;
        else if (mod->type == SPELLMOD_PCT)
        {
            // skip percent mods for null basevalue (most important for spell mods with charges )
            if (basevalue == T(0))
                continue;

            // special case (skip >10sec spell casts for instant cast setting)
            if (mod->op == SPELLMOD_CASTING_TIME  && basevalue >= T(10 * IN_MILLISECONDS) && mod->value <= -100)
                continue;

            totalpct += mod->value;
        }

        if (mod->charges > 0)
        {
            if (!spell)
                spell = FindCurrentSpellBySpellId(spellId);

            // avoid double use spellmod charge by same spell
            if (!mod->lastAffected || mod->lastAffected != spell)
            {
                --mod->charges;

                if (mod->charges == 0)
                {
                    mod->charges = -1;
                    ++m_SpellModRemoveCount;
                }

                mod->lastAffected = spell;
            }
        }
    }

    T diff = basevalue * totalpct / 100 + addedFlat * (100 + totalpct) / 100;
    basevalue = T(basevalue + diff);
    return T(diff);
}

#endif
