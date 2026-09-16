#include "eud_compat.h"
#include "proto.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NOX_EUD_DATA_BEGIN 0x00581450u
#define NOX_EUD_DATA_SPLIT_1 0x00587000u
#define NOX_EUD_DATA_SPLIT_2 0x005D4594u
#define NOX_EUD_DATA_END_EXCLUSIVE 0x0097EE69u

/*
 * Reloaded/Panic EUD scripts also use unused NoxScript builtins as the
 * portable entry points for DWORD memory I/O.
 */
#define NOX_EUD_SET_DWORD_BUILTIN 0x59
#define NOX_EUD_CREATE_OBJECT_AT_BUILTIN 0x35
#define NOX_EUD_MONSTER_ACTION_PUSH_BUILTIN 0x4C
#define NOX_EUD_INVOKE_RAW_BUILTIN 0x1F
#define NOX_EUD_MEM_ALLOC_BUILTIN 0x5F
#define NOX_EUD_NPC_EQUIPMENT_BUILTIN 0x5A
#define NOX_EUD_SPELL_LOOKUP_BUILTIN 0x5E
#define NOX_EUD_PLAY_SOUND_BUILTIN 0x74
#define NOX_EUD_BIND_BUILTIN 0xA5
#define NOX_EUD_UNIT_TO_PTR_BUILTIN 0xB8
#define NOX_EUD_GET_DWORD_BUILTIN 0xB9

/* Panic EUD v171 non-DWORD helpers installed by UOperatorInitNon4ByteHandler. */
#define NOX_EUD_SET_BYTE_TARGET 0x00750E5Cu
#define NOX_EUD_GET_BYTE_TARGET 0x00750E74u
#define NOX_EUD_SET_WORD_TARGET 0x00750E94u
#define NOX_EUD_GET_WORD_TARGET 0x00750EACu

/* Known original data pointers used by Panic's portable bootstrap/helpers. */
#define NOX_EUD_PLAYER_OBJECT_TABLE 0x0062F9E0u
#define NOX_EUD_PLAYER_RECORD_STRIDE 0x000012DCu
#define NOX_EUD_PLAYER_COUNT 32u
#define NOX_EUD_SCRIPT_TABLE_POINTER 0x0075AE28u
#define NOX_EUD_SCRIPT_FUNCTION_COUNT 0x0075AE2Cu
#define NOX_EUD_SCRIPT_STRING_COUNT 0x0075AE24u
#define NOX_EUD_SCRIPT_STRING_TABLE 0x0097BB40u
#define NOX_EUD_TIMER_FREE_LIST_POINTER 0x0083395Cu
#define NOX_EUD_OBJECT_POOL_POINTER 0x00752064u
#define NOX_EUD_ACTIVE_OBJECT_HEAD 0x00750710u
#define NOX_EUD_THINGDB_TABLE_POINTER 0x007520D4u
#define NOX_EUD_THINGDB_SPRITE_TABLE_POINTER 0x0069F224u
#define NOX_EUD_THINGDB_COUNT 0x0069F228u
#define NOX_EUD_GAMEDATA_POINTER 0x006552D8u
#define NOX_EUD_SMART_MEMORY_HEAD 0x005956DCu
#define NOX_EUD_SMART_MEMORY_CODE 0x0059567Cu
#define NOX_EUD_SMART_DESTRUCTOR_SLOT 0x0059824Cu
#define NOX_EUD_RECOVERY_DESTRUCTOR_SLOT 0x0059821Cu
#define NOX_EUD_SMART_DESTRUCTOR_ORIGINAL 0x005060D0u
#define NOX_EUD_RECOVERY_DESTRUCTOR_ORIGINAL 0x0042A6E0u
#define NOX_EUD_RECOVERY_NORMAL_GUARD 0x00852980u
#define NOX_EUD_SCRIPT_CALLER 0x00979720u
#define NOX_EUD_SCRIPT_TRIGGER 0x00979724u
#define NOX_EUD_SPELLDB_BASE 0x00663EF0u
#define NOX_EUD_SPELLDB_RECORD_SIZE 80u
#define NOX_EUD_SPELLDB_RECORD_COUNT 137u
#define NOX_EUD_ABILITYDB_BASE 0x00666A24u
#define NOX_EUD_ABILITYDB_RECORD_SIZE 52u
#define NOX_EUD_ABILITYDB_RECORD_COUNT 5u
#define NOX_EUD_MAGIC_MISSILE_UPDATE 0x0053BDA0u

/* Original runtime structure sizes/offsets that survive in the decomp. */
#define NOX_EUD_OBJECT_SIZE 0x304u
#define NOX_EUD_OBJECT_SCRIPT_ID_OFFSET 0x02Cu
#define NOX_EUD_OBJECT_OWNER_OFFSET 0x1FCu
#define NOX_EUD_OBJECT_PLAYER_EXT_OFFSET 0x2ECu
#define NOX_EUD_OBJECT_COLLIDE_HANDLER_OFFSET 0x2B8u
#define NOX_EUD_OBJECT_PICKUP_HANDLER_OFFSET 0x2C4u
#define NOX_EUD_OBJECT_DISCARD_HANDLER_OFFSET 0x2C8u
#define NOX_EUD_OBJECT_DEATH_HANDLER_OFFSET 0x2D4u
#define NOX_EUD_OBJECT_USE_ITEM_HANDLER_OFFSET 0x2DCu
#define NOX_EUD_OBJECT_DISCARD_CALLBACK_OFFSET 0x090u
#define NOX_EUD_OBJECT_PICKUP_CALLBACK_OFFSET 0x0B8u
#define NOX_EUD_OBJECT_DEATH_CALLBACK_OFFSET 0x228u
#define NOX_EUD_OBJECT_SHARED_CALLBACK_OFFSET 0x2FCu
#define NOX_EUD_OBJECT_PICKUP_ONCE_CALLBACK_OFFSET 0x300u
#define NOX_EUD_UNIT_EXT_PLAYER_SPAN 0x118u
#define NOX_EUD_UNIT_EXT_MONSTER_SPAN 0x7FCu
#define NOX_EUD_PLAYER_EXT_WEAPON_OFFSET 0x068u
#define NOX_EUD_PLAYER_EXT_NEXT_WEAPON_OFFSET 0x06Cu
#define NOX_EUD_PLAYER_EXT_INFO_OFFSET 0x114u
#define NOX_EUD_MONSTER_ACTION_SIZE 0x018u
#define NOX_EUD_MONSTER_ACTION_FIRST_OFFSET 0x228u
#define NOX_EUD_MONSTER_ACTION_LAST_OFFSET 0x438u
#define NOX_EUD_SCRIPT_RECORD_SIZE 0x030u
#define NOX_EUD_SCRIPT_LOCALS_OFFSET 0x01Cu
#define NOX_EUD_SCRIPT_LOCALS_COUNT_OFFSET 0x010u
#define NOX_EUD_TIMER_NODE_SIZE 0x020u
#define NOX_EUD_POOL_ALLOCATED_LIST_OFFSET 0x070u
#define NOX_EUD_POOL_NODE_NEXT_OFFSET 0x008u
#define NOX_EUD_POOL_NODE_DATA_OFFSET 0x010u
#define NOX_EUD_THINGDB_SERVER_RECORD_SPAN 0x0D4u
#define NOX_EUD_THINGDB_SPRITE_RECORD_SPAN 0x04Cu
#define NOX_EUD_GAMEDATA_TABLE_COUNT 26u
#define NOX_EUD_GAMEDATA_TABLE_RECORD_SIZE 12u
#define NOX_EUD_GAMEDATA_SUBTABLE_RECORD_SIZE 8u
#define NOX_EUD_MAX_STRING_SPAN 4096u
#define NOX_EUD_ALLOC_MAGIC_MISSILE 0x00000001u

/*
 * Tokens are intentionally outside the original executable data image and
 * remain positive signed 32-bit values. Pointer arithmetic performed by EUD
 * scripts therefore works without exposing a host address to the script.
 */
#define NOX_EUD_TOKEN_BEGIN 0x20000000u
#define NOX_EUD_TOKEN_STRIDE 0x00020000u
#define NOX_EUD_TOKEN_COUNT 4096u
#define NOX_EUD_TOKEN_END_EXCLUSIVE \
  (NOX_EUD_TOKEN_BEGIN + NOX_EUD_TOKEN_STRIDE * NOX_EUD_TOKEN_COUNT)

#define NOX_EUD_CALLBACK_ENTRY_COUNT 4096u
#define NOX_EUD_CALLBACK_ENTRY_MASK (NOX_EUD_CALLBACK_ENTRY_COUNT - 1u)
#define NOX_EUD_RESTORE_ENTRY_COUNT 1024u

typedef enum nox_eud_pointer_kind
{
  NOX_EUD_POINTER_NONE = 0,
  NOX_EUD_POINTER_OBJECT,
  NOX_EUD_POINTER_UNIT_EXT,
  NOX_EUD_POINTER_MONSTER_ACTION,
  NOX_EUD_POINTER_WALL,
  NOX_EUD_POINTER_WALL_DETAILS,
  NOX_EUD_POINTER_TIMER_NODE,
  NOX_EUD_POINTER_SCRIPT_TABLE,
  NOX_EUD_POINTER_SCRIPT_LOCALS,
  NOX_EUD_POINTER_ALLOC,
  NOX_EUD_POINTER_NATIVE_HANDLER,
  NOX_EUD_POINTER_THINGDB_TABLE,
  NOX_EUD_POINTER_THINGDB_RECORD,
  NOX_EUD_POINTER_THINGDB_VALUE,
  NOX_EUD_POINTER_SPRITE_TABLE,
  NOX_EUD_POINTER_SPRITE_RECORD,
  NOX_EUD_POINTER_GAMEDATA_ROOT,
  NOX_EUD_POINTER_GAMEDATA_SUBTABLE,
  NOX_EUD_POINTER_GAMEDATA_VALUE_DESC,
  NOX_EUD_POINTER_GAMEDATA_VALUES,
  NOX_EUD_POINTER_CSTRING,
  NOX_EUD_POINTER_WSTRING
} nox_eud_pointer_kind;

typedef struct nox_eud_pointer_entry
{
  unsigned char *host;
  unsigned char *owner;
  uint32_t span;
  uint32_t flags;
  int object_id;
  nox_eud_pointer_kind kind;
} nox_eud_pointer_entry;

static nox_eud_pointer_entry nox_eud_pointers[NOX_EUD_TOKEN_COUNT];
static unsigned char nox_eud_pointer_quarantined[NOX_EUD_TOKEN_COUNT];
static unsigned int nox_eud_pointer_next_slot;
static unsigned int nox_eud_live_alloc_count;
static uint32_t nox_eud_smart_destructor_target;
static uint32_t nox_eud_recovery_destructor_target;

typedef struct nox_eud_restore_entry
{
  unsigned char *target;
  uint32_t original;
} nox_eud_restore_entry;

static nox_eud_restore_entry nox_eud_restore_entries[NOX_EUD_RESTORE_ENTRY_COUNT];
static unsigned int nox_eud_restore_entry_count;

typedef enum nox_eud_callback_type
{
  NOX_EUD_CALLBACK_COLLIDE = 0,
  NOX_EUD_CALLBACK_PICKUP,
  NOX_EUD_CALLBACK_DISCARD,
  NOX_EUD_CALLBACK_DEATH,
  NOX_EUD_CALLBACK_USE_ITEM,
  NOX_EUD_CALLBACK_COUNT
} nox_eud_callback_type;

typedef struct nox_eud_callback_entry
{
  unsigned char *object;
  int object_id;
  uint32_t original_handler[NOX_EUD_CALLBACK_COUNT];
  uint32_t helper_target[NOX_EUD_CALLBACK_COUNT];
  int callback_90;
  int callback_B8;
  int callback_228;
  int callback_2FC;
  int pickup_once_callback;
  unsigned int pending_mask;
  unsigned int installed_mask;
  unsigned int slot_valid_mask;
  unsigned int pickup_once_tracked;
  int discard_bypass_callback;
  uint32_t discard_bypass_previous_handler;
  unsigned int discard_bypass_previous_eud;
} nox_eud_callback_entry;

static nox_eud_callback_entry nox_eud_callbacks[NOX_EUD_CALLBACK_ENTRY_COUNT];

static int __cdecl nox_eud_callback_collide(int self, int other, float2 *point);
static int __cdecl nox_eud_callback_pickup(int holder, int item, int flags, int arg4);
static int __cdecl nox_eud_callback_discard(int holder, int item, float2 *point);
static int __cdecl nox_eud_callback_discard_bypass(int holder, int item, float2 *point);
static void __cdecl nox_eud_callback_death(int self);
static int __cdecl nox_eud_callback_use_item(int user, int item);
static int nox_eud_script_function_is_valid(int callback);

static int nox_eud_range_is_mapped(uint32_t address, size_t size)
{
  uint64_t end;

  if (!size || address < NOX_EUD_DATA_BEGIN)
    return 0;

  end = (uint64_t)address + (uint64_t)size;
  return end <= NOX_EUD_DATA_END_EXCLUSIVE;
}

static unsigned char *nox_eud_data_byte(uint32_t address)
{
  if (address < NOX_EUD_DATA_SPLIT_1)
    return &byte_581450[address - NOX_EUD_DATA_BEGIN];
  if (address < NOX_EUD_DATA_SPLIT_2)
    return &byte_587000[address - NOX_EUD_DATA_SPLIT_1];
  return &byte_5D4594[address - NOX_EUD_DATA_SPLIT_2];
}

static uint32_t nox_eud_host_read_u32(const unsigned char *host)
{
  uint32_t result;

  result = (uint32_t)host[0];
  result |= (uint32_t)host[1] << 8;
  result |= (uint32_t)host[2] << 16;
  result |= (uint32_t)host[3] << 24;
  return result;
}

static void nox_eud_host_write_u32(unsigned char *host, uint32_t value)
{
  host[0] = (unsigned char)value;
  host[1] = (unsigned char)(value >> 8);
  host[2] = (unsigned char)(value >> 16);
  host[3] = (unsigned char)(value >> 24);
}

static int nox_eud_host_to_legacy_data(const unsigned char *host, uint32_t *address)
{
  uintptr_t value;
  uintptr_t begin;

  if (!host || !address)
    return 0;

  value = (uintptr_t)host;
  begin = (uintptr_t)&byte_581450[0];
  if (value >= begin && value < begin + sizeof(byte_581450))
  {
    *address = NOX_EUD_DATA_BEGIN + (uint32_t)(value - begin);
    return 1;
  }

  begin = (uintptr_t)&byte_587000[0];
  if (value >= begin && value < begin + sizeof(byte_587000))
  {
    *address = NOX_EUD_DATA_SPLIT_1 + (uint32_t)(value - begin);
    return 1;
  }

  begin = (uintptr_t)&byte_5D4594[0];
  if (value >= begin && value < begin + sizeof(byte_5D4594))
  {
    *address = NOX_EUD_DATA_SPLIT_2 + (uint32_t)(value - begin);
    return 1;
  }

  return 0;
}

static int nox_eud_object_pool_contains(const unsigned char *host)
{
  uint32_t pool;
  uint32_t node;
  unsigned int guard;

  if (!host)
    return 0;
  pool = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_OBJECT_POOL_POINTER));
  if (!pool)
    return 0;
  node = nox_eud_host_read_u32((unsigned char *)(uintptr_t)pool + NOX_EUD_POOL_ALLOCATED_LIST_OFFSET);
  guard = 0;
  while (node && guard++ < 65536u)
  {
    if ((unsigned char *)(uintptr_t)(node + NOX_EUD_POOL_NODE_DATA_OFFSET) == host)
      return 1;
    node = nox_eud_host_read_u32((unsigned char *)(uintptr_t)node + NOX_EUD_POOL_NODE_NEXT_OFFSET);
  }
  return 0;
}

static int nox_eud_pointer_is_valid(const nox_eud_pointer_entry *entry)
{
  int object;
  uint32_t current;

  if (!entry || !entry->host || entry->kind == NOX_EUD_POINTER_NONE)
    return 0;

  switch (entry->kind)
  {
    case NOX_EUD_POINTER_OBJECT:
      if (!nox_eud_object_pool_contains(entry->host))
        return 0;
      current = nox_eud_host_read_u32(entry->host + NOX_EUD_OBJECT_SCRIPT_ID_OFFSET);
      return current == (uint32_t)entry->object_id && (entry->host[16] & 0x20) == 0;

    case NOX_EUD_POINTER_UNIT_EXT:
      object = sub_511B60(entry->object_id);
      if (!object)
        return 0;
      current = nox_eud_host_read_u32((unsigned char *)(uintptr_t)(uint32_t)object + NOX_EUD_OBJECT_PLAYER_EXT_OFFSET);
      return current == (uint32_t)(uintptr_t)entry->host;

    case NOX_EUD_POINTER_MONSTER_ACTION:
    {
      unsigned char *object_host;
      unsigned char *unit_ext;
      uintptr_t action_offset;

      object = sub_511B60(entry->object_id);
      if (!object || !entry->owner)
        return 0;
      object_host = (unsigned char *)(uintptr_t)(uint32_t)object;
      current = nox_eud_host_read_u32(object_host + NOX_EUD_OBJECT_PLAYER_EXT_OFFSET);
      unit_ext = (unsigned char *)(uintptr_t)current;
      if (unit_ext != entry->owner || (object_host[8] & 2) == 0)
        return 0;
      if ((uintptr_t)entry->host < (uintptr_t)unit_ext)
        return 0;
      action_offset = (uintptr_t)entry->host - (uintptr_t)unit_ext;
      if (action_offset < NOX_EUD_MONSTER_ACTION_FIRST_OFFSET ||
          action_offset > NOX_EUD_MONSTER_ACTION_LAST_OFFSET ||
          (action_offset - NOX_EUD_MONSTER_ACTION_FIRST_OFFSET) % NOX_EUD_MONSTER_ACTION_SIZE != 0)
        return 0;
      if ((signed char)unit_ext[0x220] < 0)
        return 0;
      return (action_offset - NOX_EUD_MONSTER_ACTION_FIRST_OFFSET) / NOX_EUD_MONSTER_ACTION_SIZE <=
             (unsigned char)unit_ext[0x220];
    }

    case NOX_EUD_POINTER_WALL:
      current = (uint32_t)sub_410580(entry->object_id & 0xFF, (entry->object_id >> 8) & 0xFF);
      return current == (uint32_t)(uintptr_t)entry->host;

    case NOX_EUD_POINTER_WALL_DETAILS:
      current = (uint32_t)sub_410580(entry->object_id & 0xFF, (entry->object_id >> 8) & 0xFF);
      if (!current || current != (uint32_t)(uintptr_t)entry->owner)
        return 0;
      current = nox_eud_host_read_u32((unsigned char *)(uintptr_t)current + 0x1C);
      return current == (uint32_t)(uintptr_t)entry->host;

    case NOX_EUD_POINTER_SCRIPT_TABLE:
    {
      uint32_t count;
      uint64_t span;

      current = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_SCRIPT_TABLE_POINTER));
      if (current != (uint32_t)(uintptr_t)entry->host)
        return 0;
      count = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_SCRIPT_FUNCTION_COUNT));
      span = (uint64_t)count * NOX_EUD_SCRIPT_RECORD_SIZE;
      return span == entry->span;
    }

    case NOX_EUD_POINTER_SCRIPT_LOCALS:
    {
      unsigned char *table;
      uint32_t count;
      uint64_t table_span;
      uint64_t locals_span;

      current = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_SCRIPT_TABLE_POINTER));
      if (!current || !entry->owner)
        return 0;

      table = (unsigned char *)(uintptr_t)current;
      count = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_SCRIPT_FUNCTION_COUNT));
      table_span = (uint64_t)count * NOX_EUD_SCRIPT_RECORD_SIZE;
      if (!table_span || table_span >= NOX_EUD_TOKEN_STRIDE ||
          (uintptr_t)entry->owner < (uintptr_t)table ||
          (uint64_t)((uintptr_t)entry->owner - (uintptr_t)table) + NOX_EUD_SCRIPT_RECORD_SIZE > table_span)
        return 0;

      current = nox_eud_host_read_u32(entry->owner + NOX_EUD_SCRIPT_LOCALS_OFFSET);
      if (current != (uint32_t)(uintptr_t)entry->host)
        return 0;

      count = nox_eud_host_read_u32(entry->owner + NOX_EUD_SCRIPT_LOCALS_COUNT_OFFSET);
      locals_span = (uint64_t)count * 4u;
      if (!locals_span)
        locals_span = 4;
      return locals_span == entry->span;
    }

    case NOX_EUD_POINTER_TIMER_NODE:
      current = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_TIMER_FREE_LIST_POINTER));
      return current == (uint32_t)(uintptr_t)entry->host;

    case NOX_EUD_POINTER_ALLOC:
      return entry->host != 0 && entry->span != 0;

    case NOX_EUD_POINTER_NATIVE_HANDLER:
      if (!entry->owner || !nox_eud_object_pool_contains(entry->owner))
        return 0;
      current = nox_eud_host_read_u32(entry->owner + NOX_EUD_OBJECT_SCRIPT_ID_OFFSET);
      return current == (uint32_t)entry->object_id;

    case NOX_EUD_POINTER_THINGDB_TABLE:
      current = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_THINGDB_TABLE_POINTER));
      return current == (uint32_t)(uintptr_t)entry->host;

    case NOX_EUD_POINTER_SPRITE_TABLE:
      current = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_THINGDB_SPRITE_TABLE_POINTER));
      return current == (uint32_t)(uintptr_t)entry->host;

    case NOX_EUD_POINTER_THINGDB_RECORD:
      if (!entry->owner || entry->object_id < 0 ||
          nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_THINGDB_TABLE_POINTER)) !=
              (uint32_t)(uintptr_t)entry->owner)
        return 0;
      current = nox_eud_host_read_u32(entry->owner + 4u * (uint32_t)entry->object_id);
      return current == (uint32_t)(uintptr_t)entry->host;

    case NOX_EUD_POINTER_SPRITE_RECORD:
      if (!entry->owner || entry->object_id < 0 ||
          nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_THINGDB_SPRITE_TABLE_POINTER)) !=
              (uint32_t)(uintptr_t)entry->owner)
        return 0;
      current = nox_eud_host_read_u32(entry->owner + 4u * (uint32_t)entry->object_id);
      return current == (uint32_t)(uintptr_t)entry->host;

    case NOX_EUD_POINTER_THINGDB_VALUE:
      return entry->owner && nox_eud_host_read_u32(entry->owner) == (uint32_t)(uintptr_t)entry->host;

    case NOX_EUD_POINTER_GAMEDATA_ROOT:
      current = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_GAMEDATA_POINTER));
      return current == (uint32_t)(uintptr_t)entry->host;

    case NOX_EUD_POINTER_GAMEDATA_SUBTABLE:
      if (!entry->owner)
        return 0;
      return nox_eud_host_read_u32(entry->owner) == (uint32_t)(uintptr_t)entry->host &&
             (uint64_t)nox_eud_host_read_u32(entry->owner + 4) *
                     NOX_EUD_GAMEDATA_SUBTABLE_RECORD_SIZE ==
                 entry->span;

    case NOX_EUD_POINTER_GAMEDATA_VALUE_DESC:
      return entry->owner && nox_eud_host_read_u32(entry->owner + 4) == (uint32_t)(uintptr_t)entry->host;

    case NOX_EUD_POINTER_GAMEDATA_VALUES:
      return entry->owner && nox_eud_host_read_u32(entry->owner) == (uint32_t)(uintptr_t)entry->host &&
             (uint64_t)nox_eud_host_read_u32(entry->owner + 4) * 4u == entry->span;

    case NOX_EUD_POINTER_CSTRING:
    case NOX_EUD_POINTER_WSTRING:
      return entry->host != 0 && entry->span != 0;

    default:
      return 0;
  }
}

static int nox_eud_pointer_decode(uint32_t address, size_t size, nox_eud_pointer_entry **entry,
                                  uint32_t *offset);

static uint32_t nox_eud_pointer_token(unsigned int slot)
{
  return NOX_EUD_TOKEN_BEGIN + slot * NOX_EUD_TOKEN_STRIDE;
}

static uint32_t nox_eud_pointer_register(nox_eud_pointer_kind kind, unsigned char *host, uint32_t span,
                                         int object_id, unsigned char *owner)
{
  unsigned int i;
  unsigned int slot;
  nox_eud_pointer_entry *entry;

  if (!host || !span || span >= NOX_EUD_TOKEN_STRIDE)
    return 0;

  for (i = 0; i < NOX_EUD_TOKEN_COUNT; ++i)
  {
    entry = &nox_eud_pointers[i];
    if (entry->kind == kind && entry->host == host && entry->owner == owner && entry->object_id == object_id &&
        nox_eud_pointer_is_valid(entry))
    {
      entry->span = span;
      return nox_eud_pointer_token(i);
    }
  }

  for (i = 0; i < NOX_EUD_TOKEN_COUNT; ++i)
  {
    slot = (nox_eud_pointer_next_slot + i) % NOX_EUD_TOKEN_COUNT;
    entry = &nox_eud_pointers[slot];
    if (!nox_eud_pointer_quarantined[slot] &&
        (entry->kind == NOX_EUD_POINTER_NONE || !nox_eud_pointer_is_valid(entry)))
    {
      entry->host = host;
      entry->owner = owner;
      entry->span = span;
      entry->flags = 0;
      entry->object_id = object_id;
      entry->kind = kind;
      nox_eud_pointer_next_slot = (slot + 1) % NOX_EUD_TOKEN_COUNT;
      return nox_eud_pointer_token(slot);
    }
  }

  return 0;
}

static uint32_t nox_eud_register_object_pointer(uint32_t pointer)
{
  unsigned char *host;
  int object_id;

  if (!pointer)
    return 0;

  host = (unsigned char *)(uintptr_t)pointer;
  if (!nox_eud_object_pool_contains(host) || (host[16] & 0x20) != 0)
    return 0;
  object_id = (int)nox_eud_host_read_u32(host + NOX_EUD_OBJECT_SCRIPT_ID_OFFSET);

  return nox_eud_pointer_register(NOX_EUD_POINTER_OBJECT, host, NOX_EUD_OBJECT_SIZE, object_id, 0);
}

static uint32_t nox_eud_register_unit_ext(uint32_t pointer, int object_id)
{
  int object;
  unsigned char *object_host;
  uint32_t span;

  if (!pointer)
    return 0;

  object = sub_511B60(object_id);
  if (!object)
    return 0;
  object_host = (unsigned char *)(uintptr_t)(uint32_t)object;
  if (object_host[8] & 2)
    span = NOX_EUD_UNIT_EXT_MONSTER_SPAN;
  else if (object_host[8] & 4)
    span = NOX_EUD_UNIT_EXT_PLAYER_SPAN;
  else
    return 0;

  return nox_eud_pointer_register(NOX_EUD_POINTER_UNIT_EXT, (unsigned char *)(uintptr_t)pointer,
                                  span, object_id, 0);
}

static uint32_t nox_eud_register_monster_action(int *action, int object_id, unsigned char *unit_ext)
{
  if (!action || !unit_ext)
    return 0;

  return nox_eud_pointer_register(NOX_EUD_POINTER_MONSTER_ACTION, (unsigned char *)action,
                                  NOX_EUD_MONSTER_ACTION_SIZE, object_id, unit_ext);
}

static uint32_t nox_eud_register_wall(uint32_t pointer)
{
  unsigned char *host;
  int wall_key;

  if (!pointer)
    return 0;
  host = (unsigned char *)(uintptr_t)pointer;
  wall_key = host[5] | ((int)host[6] << 8);
  if ((uint32_t)sub_410580(wall_key & 0xFF, (wall_key >> 8) & 0xFF) != pointer)
    return 0;

  return nox_eud_pointer_register(NOX_EUD_POINTER_WALL, host, 0x20u, wall_key, 0);
}

static uint32_t nox_eud_register_wall_details(uint32_t pointer, unsigned char *wall, int wall_key)
{
  if (!pointer || !wall)
    return 0;

  return nox_eud_pointer_register(NOX_EUD_POINTER_WALL_DETAILS,
                                  (unsigned char *)(uintptr_t)pointer, 0x20u, wall_key, wall);
}

static uint32_t nox_eud_register_script_table(uint32_t pointer)
{
  uint32_t count;
  uint64_t span;

  if (!pointer)
    return 0;

  count = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_SCRIPT_FUNCTION_COUNT));
  span = (uint64_t)count * NOX_EUD_SCRIPT_RECORD_SIZE;
  if (!span || span >= NOX_EUD_TOKEN_STRIDE)
    return 0;

  return nox_eud_pointer_register(NOX_EUD_POINTER_SCRIPT_TABLE, (unsigned char *)(uintptr_t)pointer,
                                  (uint32_t)span, 0, 0);
}

static uint32_t nox_eud_register_timer_node(uint32_t pointer)
{
  if (!pointer)
    return 0;

  return nox_eud_pointer_register(NOX_EUD_POINTER_TIMER_NODE, (unsigned char *)(uintptr_t)pointer,
                                  NOX_EUD_TIMER_NODE_SIZE, 0, 0);
}

static uint32_t nox_eud_register_cstring(uint32_t pointer)
{
  const unsigned char *host;
  uint32_t span;

  if (!pointer)
    return 0;
  host = (const unsigned char *)(uintptr_t)pointer;
  for (span = 0; span < NOX_EUD_MAX_STRING_SPAN; ++span)
  {
    if (!host[span])
    {
      ++span;
      break;
    }
  }
  if (!span || span > NOX_EUD_MAX_STRING_SPAN)
    return 0;
  span = (span + 3u) & ~3u;
  if (span > NOX_EUD_MAX_STRING_SPAN)
    span = NOX_EUD_MAX_STRING_SPAN;
  return nox_eud_pointer_register(NOX_EUD_POINTER_CSTRING, (unsigned char *)(uintptr_t)pointer,
                                  span, 0, 0);
}

static uint32_t nox_eud_register_wstring(uint32_t pointer)
{
  const unsigned char *host;
  uint32_t span;
  int terminated = 0;

  if (!pointer)
    return 0;
  host = (const unsigned char *)(uintptr_t)pointer;
  /* Panic SpellDB probes a DWORD marker immediately after the UTF-16 NUL. */
  for (span = 0; span + 6u <= NOX_EUD_MAX_STRING_SPAN; span += 2u)
  {
    if (host[span] == 0 && host[span + 1u] == 0)
    {
      span += 6u;
      terminated = 1;
      break;
    }
  }
  if (!terminated)
    return 0;
  span = (span + 3u) & ~3u;
  return nox_eud_pointer_register(NOX_EUD_POINTER_WSTRING, (unsigned char *)(uintptr_t)pointer,
                                  span, 0, 0);
}

static int nox_eud_is_wide_string_pointer_field(uint32_t address)
{
  uint32_t relative;
  uint32_t record_offset;

  if (address >= NOX_EUD_SPELLDB_BASE &&
      address < NOX_EUD_SPELLDB_BASE + NOX_EUD_SPELLDB_RECORD_COUNT * NOX_EUD_SPELLDB_RECORD_SIZE)
  {
    relative = address - NOX_EUD_SPELLDB_BASE;
    record_offset = relative % NOX_EUD_SPELLDB_RECORD_SIZE;
    return record_offset == 0u || record_offset == 4u;
  }

  if (address >= NOX_EUD_ABILITYDB_BASE &&
      address < NOX_EUD_ABILITYDB_BASE + NOX_EUD_ABILITYDB_RECORD_COUNT * NOX_EUD_ABILITYDB_RECORD_SIZE)
  {
    relative = address - NOX_EUD_ABILITYDB_BASE;
    record_offset = relative % NOX_EUD_ABILITYDB_RECORD_SIZE;
    return record_offset == 0u || record_offset == 4u;
  }

  return 0;
}

static uint32_t nox_eud_register_thingdb_table(uint32_t pointer, int sprite)
{
  uint32_t count;
  uint64_t span;
  nox_eud_pointer_kind kind;

  if (!pointer)
    return 0;
  count = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_THINGDB_COUNT)) & 0xFFFFu;
  span = (uint64_t)count * 4u;
  if (!span || span >= NOX_EUD_TOKEN_STRIDE)
    return 0;
  kind = sprite ? NOX_EUD_POINTER_SPRITE_TABLE : NOX_EUD_POINTER_THINGDB_TABLE;
  return nox_eud_pointer_register(kind, (unsigned char *)(uintptr_t)pointer, (uint32_t)span, 0, 0);
}

static uint32_t nox_eud_register_thingdb_record(uint32_t pointer, nox_eud_pointer_entry *table,
                                                int thing_id, int sprite)
{
  nox_eud_pointer_kind kind;
  uint32_t span;

  if (!pointer || !table || thing_id < 0)
    return 0;
  kind = sprite ? NOX_EUD_POINTER_SPRITE_RECORD : NOX_EUD_POINTER_THINGDB_RECORD;
  span = sprite ? NOX_EUD_THINGDB_SPRITE_RECORD_SPAN : NOX_EUD_THINGDB_SERVER_RECORD_SPAN;
  return nox_eud_pointer_register(kind, (unsigned char *)(uintptr_t)pointer, span, thing_id, table->host);
}

static uint32_t nox_eud_register_thingdb_value(uint32_t pointer, unsigned char *pointer_field)
{
  if (!pointer || !pointer_field)
    return 0;
  return nox_eud_pointer_register(NOX_EUD_POINTER_THINGDB_VALUE,
                                  (unsigned char *)(uintptr_t)pointer, 4u, 0, pointer_field);
}

static uint32_t nox_eud_register_gamedata_root(uint32_t pointer)
{
  if (!pointer)
    return 0;
  return nox_eud_pointer_register(NOX_EUD_POINTER_GAMEDATA_ROOT, (unsigned char *)(uintptr_t)pointer,
                                  NOX_EUD_GAMEDATA_TABLE_COUNT * NOX_EUD_GAMEDATA_TABLE_RECORD_SIZE,
                                  0, 0);
}

static uint32_t nox_eud_register_gamedata_subtable(uint32_t pointer, unsigned char *descriptor)
{
  uint32_t count;
  uint64_t span;

  if (!pointer || !descriptor)
    return 0;
  count = nox_eud_host_read_u32(descriptor + 4);
  span = (uint64_t)count * NOX_EUD_GAMEDATA_SUBTABLE_RECORD_SIZE;
  if (!span || span >= NOX_EUD_TOKEN_STRIDE)
    return 0;
  return nox_eud_pointer_register(NOX_EUD_POINTER_GAMEDATA_SUBTABLE,
                                  (unsigned char *)(uintptr_t)pointer, (uint32_t)span, 0, descriptor);
}

static uint32_t nox_eud_register_gamedata_value_desc(uint32_t pointer, unsigned char *subtable_entry)
{
  if (!pointer || !subtable_entry)
    return 0;
  return nox_eud_pointer_register(NOX_EUD_POINTER_GAMEDATA_VALUE_DESC,
                                  (unsigned char *)(uintptr_t)pointer, 8u, 0, subtable_entry);
}

static uint32_t nox_eud_register_gamedata_values(uint32_t pointer, unsigned char *descriptor)
{
  uint32_t count;
  uint64_t span;

  if (!pointer || !descriptor)
    return 0;
  count = nox_eud_host_read_u32(descriptor + 4);
  span = (uint64_t)count * 4u;
  if (!span || span >= NOX_EUD_TOKEN_STRIDE)
    return 0;
  return nox_eud_pointer_register(NOX_EUD_POINTER_GAMEDATA_VALUES,
                                  (unsigned char *)(uintptr_t)pointer, (uint32_t)span, 0, descriptor);
}

static uint32_t nox_eud_register_native_handler(uint32_t handler, unsigned char *object)
{
  int object_id;

  if (!handler || !object || !nox_eud_object_pool_contains(object))
    return 0;
  object_id = (int)nox_eud_host_read_u32(object + NOX_EUD_OBJECT_SCRIPT_ID_OFFSET);
  return nox_eud_pointer_register(NOX_EUD_POINTER_NATIVE_HANDLER,
                                  (unsigned char *)(uintptr_t)handler, 1u, object_id, object);
}

static uint32_t nox_eud_alloc(uint32_t size)
{
  unsigned char *host;
  uint32_t token;
  nox_eud_pointer_entry *entry;
  uint32_t offset;
  unsigned int slot;

  if (!size || size >= NOX_EUD_TOKEN_STRIDE)
    return 0;
  host = (unsigned char *)malloc(size);
  if (!host)
    return 0;
  token = nox_eud_pointer_register(NOX_EUD_POINTER_ALLOC, host, size, 0, 0);
  if (!token)
  {
    free(host);
    return 0;
  }
  if (!nox_eud_pointer_decode(token, 1, &entry, &offset))
  {
    slot = (token - NOX_EUD_TOKEN_BEGIN) / NOX_EUD_TOKEN_STRIDE;
    free(host);
    if (slot < NOX_EUD_TOKEN_COUNT)
    {
      memset(&nox_eud_pointers[slot], 0, sizeof(nox_eud_pointers[slot]));
      nox_eud_pointer_quarantined[slot] = 1;
    }
    return 0;
  }
  entry->flags = 0;
  ++nox_eud_live_alloc_count;
  return token;
}

#ifdef NOX_EUD_COMPAT_TESTING
uint32_t nox_eud_test_alloc(uint32_t size)
{
  return nox_eud_alloc(size);
}
#endif

static int nox_eud_record_restore(unsigned char *target)
{
  unsigned int i;

  if (!target)
    return 0;
  for (i = 0; i < nox_eud_restore_entry_count; ++i)
  {
    if (nox_eud_restore_entries[i].target == target)
      return 1;
  }
  if (nox_eud_restore_entry_count >= NOX_EUD_RESTORE_ENTRY_COUNT)
    return 0;
  nox_eud_restore_entries[nox_eud_restore_entry_count].target = target;
  nox_eud_restore_entries[nox_eud_restore_entry_count].original = nox_eud_host_read_u32(target);
  ++nox_eud_restore_entry_count;
  return 1;
}

static void nox_eud_restore_references_to_alloc(const nox_eud_pointer_entry *entry)
{
  unsigned int i;
  uintptr_t begin;
  uintptr_t end;
  uintptr_t value;

  if (!entry || entry->kind != NOX_EUD_POINTER_ALLOC || !entry->host)
    return;
  begin = (uintptr_t)entry->host;
  end = begin + entry->span;
  for (i = 0; i < nox_eud_restore_entry_count; ++i)
  {
    if (!nox_eud_restore_entries[i].target)
      continue;
    value = (uintptr_t)nox_eud_host_read_u32(nox_eud_restore_entries[i].target);
    if (value >= begin && value < end)
      nox_eud_host_write_u32(nox_eud_restore_entries[i].target,
                             nox_eud_restore_entries[i].original);
  }
}

static void nox_eud_restore_all_references(void)
{
  unsigned int i;

  for (i = nox_eud_restore_entry_count; i > 0; --i)
  {
    nox_eud_restore_entry *entry = &nox_eud_restore_entries[i - 1];
    if (entry->target)
      nox_eud_host_write_u32(entry->target, entry->original);
  }
  memset(nox_eud_restore_entries, 0, sizeof(nox_eud_restore_entries));
  nox_eud_restore_entry_count = 0;
}

static void nox_eud_detach_alloc(nox_eud_pointer_entry *entry)
{
  uint32_t current;
  uintptr_t update;

  if (!entry || entry->kind != NOX_EUD_POINTER_ALLOC || !entry->owner)
    return;
  if (!nox_eud_object_pool_contains(entry->owner) ||
      nox_eud_host_read_u32(entry->owner + NOX_EUD_OBJECT_SCRIPT_ID_OFFSET) != (uint32_t)entry->object_id)
    return;
  current = nox_eud_host_read_u32(entry->owner + NOX_EUD_OBJECT_PLAYER_EXT_OFFSET);
  if (current == (uint32_t)(uintptr_t)entry->host)
    nox_eud_host_write_u32(entry->owner + NOX_EUD_OBJECT_PLAYER_EXT_OFFSET, 0);
  update = (uintptr_t)sub_53BDA0;
  if (update <= UINT32_MAX &&
      nox_eud_host_read_u32(entry->owner + 0x2E8u) == (uint32_t)update)
    nox_eud_host_write_u32(entry->owner + 0x2E8u, 0);
}

static int nox_eud_free_alloc(uint32_t token)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;
  unsigned int slot;

  if (!nox_eud_pointer_decode(token, 1, &entry, &offset) || entry->kind != NOX_EUD_POINTER_ALLOC || offset != 0)
    return 0;
  slot = (unsigned int)(entry - nox_eud_pointers);
  nox_eud_restore_references_to_alloc(entry);
  nox_eud_detach_alloc(entry);
  free(entry->host);
  memset(entry, 0, sizeof(*entry));
  nox_eud_pointer_quarantined[slot] = 1;
  if (nox_eud_live_alloc_count)
    --nox_eud_live_alloc_count;
  return 1;
}

static uint32_t nox_eud_token_for_host_pointer(const unsigned char *host)
{
  unsigned int i;
  nox_eud_pointer_entry *entry;
  uintptr_t value;
  uintptr_t begin;

  if (!host)
    return 0;
  value = (uintptr_t)host;
  for (i = 0; i < NOX_EUD_TOKEN_COUNT; ++i)
  {
    entry = &nox_eud_pointers[i];
    if (!nox_eud_pointer_is_valid(entry))
      continue;
    if (entry->kind == NOX_EUD_POINTER_ALLOC)
    {
      begin = (uintptr_t)entry->host;
      if (value >= begin && value < begin + entry->span)
        return nox_eud_pointer_token(i) + (uint32_t)(value - begin);
    }
    if (entry->host == host && entry->kind != NOX_EUD_POINTER_NATIVE_HANDLER)
      return nox_eud_pointer_token(i);
  }
  return 0;
}

static int nox_eud_resolve_alloc_pointer(uint32_t token, unsigned char **host, nox_eud_pointer_entry **entry_out,
                                         uint32_t *offset_out)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;

  if (!host || !nox_eud_pointer_decode(token, 1, &entry, &offset) || entry->kind != NOX_EUD_POINTER_ALLOC)
    return 0;
  *host = entry->host + offset;
  if (entry_out)
    *entry_out = entry;
  if (offset_out)
    *offset_out = offset;
  return 1;
}

static uint32_t nox_eud_alloc_for_object(unsigned char *object, nox_eud_pointer_entry **entry_out)
{
  unsigned int i;
  int object_id;

  if (!object || !nox_eud_object_pool_contains(object))
    return 0;
  object_id = (int)nox_eud_host_read_u32(object + NOX_EUD_OBJECT_SCRIPT_ID_OFFSET);
  for (i = 0; i < NOX_EUD_TOKEN_COUNT; ++i)
  {
    nox_eud_pointer_entry *entry = &nox_eud_pointers[i];
    if (entry->kind == NOX_EUD_POINTER_ALLOC && nox_eud_pointer_is_valid(entry) &&
        entry->owner == object && entry->object_id == object_id)
    {
      if (entry_out)
        *entry_out = entry;
      return nox_eud_pointer_token(i);
    }
  }
  return 0;
}

static int nox_eud_address_is_token(uint32_t address)
{
  return address >= NOX_EUD_TOKEN_BEGIN && address < NOX_EUD_TOKEN_END_EXCLUSIVE;
}

static int nox_eud_pointer_decode(uint32_t address, size_t size, nox_eud_pointer_entry **entry,
                                  uint32_t *offset)
{
  uint32_t relative;
  uint32_t inner;
  unsigned int slot;
  nox_eud_pointer_entry *candidate;

  if (!entry || !offset || !size || address < NOX_EUD_TOKEN_BEGIN || address >= NOX_EUD_TOKEN_END_EXCLUSIVE)
    return 0;

  relative = address - NOX_EUD_TOKEN_BEGIN;
  slot = relative / NOX_EUD_TOKEN_STRIDE;
  inner = relative % NOX_EUD_TOKEN_STRIDE;
  candidate = &nox_eud_pointers[slot];

  if (!nox_eud_pointer_is_valid(candidate) || (uint64_t)inner + size > candidate->span)
    return 0;

  *entry = candidate;
  *offset = inner;
  return 1;
}

static int nox_eud_target_matches_dwords(uint32_t target, const uint32_t *helper, size_t count)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;
  size_t i;

  if (!helper || !count ||
      !nox_eud_pointer_decode(target, count * sizeof(*helper), &entry, &offset) ||
      entry->kind != NOX_EUD_POINTER_SCRIPT_LOCALS)
    return 0;

  for (i = 0; i < count; ++i)
  {
    if (nox_eud_host_read_u32(entry->host + offset + 4 * i) != helper[i])
      return 0;
  }
  return 1;
}

static int nox_eud_target_matches_bytes(uint32_t target, const unsigned char *helper, size_t size)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;

  if (!helper || !size || !nox_eud_pointer_decode(target, size, &entry, &offset) ||
      entry->kind != NOX_EUD_POINTER_SCRIPT_LOCALS)
    return 0;
  return memcmp(entry->host + offset, helper, size) == 0;
}

static int nox_eud_target_code(uint32_t target, size_t size, nox_eud_pointer_entry **entry_out,
                               uint32_t *offset_out)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;

  if (!nox_eud_pointer_decode(target, size, &entry, &offset))
    return 0;
  if (entry->kind != NOX_EUD_POINTER_SCRIPT_LOCALS && entry->kind != NOX_EUD_POINTER_ALLOC)
    return 0;
  if (entry_out)
    *entry_out = entry;
  if (offset_out)
    *offset_out = offset;
  return 1;
}

static int nox_eud_relative_call_is(const unsigned char *code, uint32_t target, uint32_t call_offset,
                                    uint32_t expected)
{
  int32_t relative;

  if (!code || code[call_offset] != 0xE8)
    return 0;
  relative = (int32_t)nox_eud_host_read_u32(code + call_offset + 1);
  return target + call_offset + 5u + (uint32_t)relative == expected;
}

static int nox_eud_target_is_mem_alloc_helper(uint32_t target)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;
  const unsigned char *code;

  if (!nox_eud_target_code(target, 28, &entry, &offset) || entry->kind != NOX_EUD_POINTER_SCRIPT_LOCALS)
    return 0;
  code = entry->host + offset;
  if (code[0] != 0x55 || code[1] != 0x50 || code[7] != 0x50 || code[13] != 0x50 ||
      code[19] != 0x83 || code[20] != 0xC4 || code[21] != 0x08 || code[22] != 0x58 ||
      code[23] != 0x5D || code[24] != 0xC3 || code[25] != 0x90 || code[26] != 0x90 || code[27] != 0x90)
    return 0;
  return nox_eud_relative_call_is(code, target, 2, 0x00507250u) &&
         nox_eud_relative_call_is(code, target, 8, 0x00403560u) &&
         nox_eud_relative_call_is(code, target, 14, 0x00507230u);
}

static int nox_eud_target_is_mem_free_helper(uint32_t target)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;
  const unsigned char *code;

  if (!nox_eud_target_code(target, 20, &entry, &offset) || entry->kind != NOX_EUD_POINTER_SCRIPT_LOCALS)
    return 0;
  code = entry->host + offset;
  if (code[0] != 0x50 || code[6] != 0x50 || code[12] != 0x83 || code[13] != 0xC4 ||
      code[14] != 0x04 || code[15] != 0x58 || code[16] != 0xC3 || code[17] != 0x90 ||
      code[18] != 0x90 || code[19] != 0x90)
    return 0;
  return nox_eud_relative_call_is(code, target, 1, 0x00507250u) &&
         nox_eud_relative_call_is(code, target, 7, 0x0040425Du);
}

static int nox_eud_target_is_dword_copy_helper(uint32_t target)
{
  static const unsigned char helper[] = {
      0x56, 0x57, 0x51, 0xB8, 0x50, 0x72, 0x50, 0x00, 0xFF, 0xD0, 0x8B, 0x30, 0x8B,
      0x78, 0x04, 0x8B, 0x48, 0x08, 0xF3, 0xA5, 0x59, 0x5F, 0x5E, 0x31, 0xC0, 0xC3};

  return nox_eud_target_matches_bytes(target, helper, sizeof(helper));
}

static int nox_eud_target_is_bind_helper(uint32_t target)
{
  static const unsigned char prefix[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C};
  static const unsigned char after_pop_args[] = {0x89, 0x45, 0xF4};
  static const unsigned char after_pop_function[] = {0x89, 0x45, 0xF8};
  static const unsigned char body[] = {
      0x89, 0x45, 0xFC, 0x51, 0x31, 0xC9, 0x8A, 0x48, 0x08, 0x84, 0xC9, 0x74,
      0x14, 0x8B, 0x45, 0xF4, 0xFE, 0xC9, 0x51, 0x8B, 0x04, 0x88, 0x50, 0x90};
  static const unsigned char before_dispatch[] = {
      0x59, 0x59, 0xEB, 0xE8, 0xB8, 0x24, 0x97, 0x97, 0x00, 0xFF,
      0x30, 0x8D, 0x40, 0xFC, 0xFF, 0x30, 0x8B, 0x45, 0xF8, 0x50};
  static const unsigned char tail[] = {0x83, 0xC4, 0x18, 0x59, 0x5D, 0xC3, 0x90};
  nox_eud_pointer_entry *entry;
  uint32_t offset;
  const unsigned char *code;

  if (!nox_eud_target_code(target, 88u, &entry, &offset) ||
      entry->kind != NOX_EUD_POINTER_SCRIPT_LOCALS)
    return 0;
  code = entry->host + offset;
  if (memcmp(code, prefix, sizeof(prefix)) != 0 ||
      memcmp(code + 11u, after_pop_args, sizeof(after_pop_args)) != 0 ||
      memcmp(code + 19u, after_pop_function, sizeof(after_pop_function)) != 0 ||
      memcmp(code + 27u, body, sizeof(body)) != 0 ||
      memcmp(code + 56u, before_dispatch, sizeof(before_dispatch)) != 0 ||
      memcmp(code + 81u, tail, sizeof(tail)) != 0)
    return 0;
  return nox_eud_relative_call_is(code, target, 6u, 0x00507250u) &&
         nox_eud_relative_call_is(code, target, 14u, 0x00507250u) &&
         nox_eud_relative_call_is(code, target, 22u, 0x00507250u) &&
         nox_eud_relative_call_is(code, target, 51u, 0x00507230u) &&
         nox_eud_relative_call_is(code, target, 76u, 0x00507310u);
}

typedef enum nox_eud_recovery_kind
{
  NOX_EUD_RECOVERY_NORMAL = 0,
  NOX_EUD_RECOVERY_MAP_CHANGED = 1
} nox_eud_recovery_kind;

static int nox_eud_target_is_recovery_helper(uint32_t target, nox_eud_recovery_kind *kind,
                                              uint32_t *head_holder, uint32_t *previous)
{
  static const unsigned char normal_prefix[] = {
      0x56, 0x55, 0x8B, 0x05, 0x80, 0x29, 0x85, 0x00, 0x85, 0xC0, 0x74, 0x16, 0x8B, 0x05};
  static const unsigned char normal_middle[] = {
      0x85, 0xC0, 0x74, 0x0C, 0x8B, 0x30, 0x8B, 0x68, 0x04, 0x89, 0x2E, 0x8B,
      0x40, 0x08, 0xEB, 0xF0, 0x5D, 0x5E, 0xC7, 0x05, 0x1C, 0x82, 0x59, 0x00};
  static const unsigned char changed_prefix[] = {0x56, 0x55, 0x8B, 0x05};
  static const unsigned char changed_middle[] = {
      0x85, 0xC0, 0x74, 0x0C, 0x8B, 0x30, 0x8B, 0x68, 0x04, 0x89, 0x2E, 0x8B,
      0x40, 0x08, 0xEB, 0xF0, 0x5D, 0x5E, 0xC7, 0x05, 0x1C, 0x82, 0x59, 0x00};
  nox_eud_pointer_entry *entry;
  nox_eud_pointer_entry *holder_entry;
  uint32_t offset;
  uint32_t holder_offset;
  uint32_t previous2;
  const unsigned char *code;

  if (!kind || !head_holder || !previous)
    return 0;
  if (nox_eud_target_code(target, 52u, &entry, &offset) && entry->kind == NOX_EUD_POINTER_ALLOC)
  {
    code = entry->host + offset;
    if (memcmp(code, normal_prefix, sizeof(normal_prefix)) == 0 &&
        memcmp(code + 18u, normal_middle, sizeof(normal_middle)) == 0 &&
        code[46] == 0x68 && code[51] == 0xC3)
    {
      *head_holder = nox_eud_host_read_u32(code + 14u);
      *previous = nox_eud_host_read_u32(code + 42u);
      previous2 = nox_eud_host_read_u32(code + 47u);
      if (*previous != previous2)
        return 0;
      if (!nox_eud_pointer_decode(*head_holder, 4u, &holder_entry, &holder_offset) ||
          holder_entry->kind != NOX_EUD_POINTER_ALLOC)
        return 0;
      *kind = NOX_EUD_RECOVERY_NORMAL;
      return 1;
    }
  }
  if (!nox_eud_target_code(target, 44u, &entry, &offset) || entry->kind != NOX_EUD_POINTER_ALLOC)
    return 0;
  code = entry->host + offset;
  if (memcmp(code, changed_prefix, sizeof(changed_prefix)) != 0 ||
      memcmp(code + 8u, changed_middle, sizeof(changed_middle)) != 0 ||
      code[36] != 0x68 || code[41] != 0xC3 || code[42] != 0x90 || code[43] != 0x90)
    return 0;
  *head_holder = nox_eud_host_read_u32(code + 4u);
  *previous = nox_eud_host_read_u32(code + 32u);
  previous2 = nox_eud_host_read_u32(code + 37u);
  if (*previous != previous2)
    return 0;
  if (!nox_eud_pointer_decode(*head_holder, 4u, &holder_entry, &holder_offset) ||
      holder_entry->kind != NOX_EUD_POINTER_ALLOC)
    return 0;
  *kind = NOX_EUD_RECOVERY_MAP_CHANGED;
  return 1;
}

static int nox_eud_recovery_chain_is_valid(uint32_t target)
{
  unsigned int depth;
  nox_eud_recovery_kind kind;
  uint32_t holder;
  uint32_t previous;

  for (depth = 0; depth < 8u; ++depth)
  {
    if (target == NOX_EUD_RECOVERY_DESTRUCTOR_ORIGINAL)
      return 1;
    if (!nox_eud_target_is_recovery_helper(target, &kind, &holder, &previous) || previous == target)
      return 0;
    target = previous;
  }
  return 0;
}

static int nox_eud_target_is_discard_bypass_helper(uint32_t target, int *callback, uint32_t *previous)
{
  static const unsigned char prefix[] = {
      0x53, 0x56, 0x8B, 0x5C, 0x24, 0x0C, 0x8B, 0x74, 0x24, 0x10, 0x68};
  static const unsigned char middle[] = {
      0x56, 0x53, 0xFF, 0xB0, 0x8C, 0x00, 0x00, 0x00, 0xB8, 0x10, 0x73, 0x50,
      0x00, 0xFF, 0xD0, 0x83, 0xC4, 0x0C, 0x58, 0x5E, 0x5B, 0x68};
  static const unsigned char suffix[] = {0xC3, 0x90, 0x90};
  nox_eud_pointer_entry *entry;
  uint32_t offset;
  const unsigned char *code;

  if (!callback || !previous || !nox_eud_target_code(target, 44, &entry, &offset) ||
      entry->kind != NOX_EUD_POINTER_ALLOC)
    return 0;
  code = entry->host + offset;
  if (memcmp(code, prefix, sizeof(prefix)) != 0 ||
      memcmp(code + 15, middle, sizeof(middle)) != 0 ||
      memcmp(code + 41, suffix, sizeof(suffix)) != 0)
    return 0;
  *callback = (int)nox_eud_host_read_u32(code + 11);
  *previous = nox_eud_host_read_u32(code + 37);
  return nox_eud_script_function_is_valid(*callback);
}

static int nox_eud_target_is_unit_to_ptr_helper(uint32_t target)
{
  static const uint32_t helper[] = {
      0x50725068u, 0x2414FF00u, 0x511B6068u, 0x54FF5000u, 0xC4830424u,
      0x7230680Cu, 0xFF500050u, 0x83042454u, 0xC03108C4u, 0x909090C3u};

  return nox_eud_target_matches_dwords(target, helper, sizeof(helper) / sizeof(helper[0]));
}

static int nox_eud_target_is_spell_lookup_helper(uint32_t target)
{
  static const uint32_t helper[] = {
      0x50725068u, 0x2414FF00u, 0x4085048Bu, 0x680097BBu, 0x004243F0u, 0x2454FF50u,
      0x08C48304u, 0x50723068u, 0x54FF5000u, 0xC4830424u, 0xC3C0310Cu};

  return nox_eud_target_matches_dwords(target, helper, sizeof(helper) / sizeof(helper[0]));
}

static int nox_eud_target_is_create_object_at_helper(uint32_t target)
{
  static const uint32_t helper[] = {
      0xAA506856u, 0x5068004Du, 0xFF005072u, 0xFF502414u, 0x50042454u,
      0x082454FFu, 0x4085048Bu, 0x680097BBu, 0x004E3810u, 0x2454FF50u,
      0x08C48304u, 0xF685F08Bu, 0x006A0A74u, 0x2454FF56u, 0x08C48314u,
      0x50723068u, 0x54FF5600u, 0xC4830424u, 0x5EC03118u, 0x909090C3u};

  return nox_eud_target_matches_dwords(target, helper, sizeof(helper) / sizeof(helper[0]));
}

static int nox_eud_target_is_remove_sneak_helper(uint32_t target)
{
  static const uint32_t helper[] = {
      0x72506850u, 0x14FF0050u, 0xC3006824u, 0x046A004Fu,
      0x2454FF50u, 0x10C48308u, 0x9090C358u};

  return nox_eud_target_matches_dwords(target, helper, sizeof(helper) / sizeof(helper[0]));
}

static int nox_eud_target_is_monster_action_push_helper(uint32_t target)
{
  static const uint32_t helper[] = {
      0x50685650u, 0xFF005072u, 0x708D2414u, 0xA2606804u,
      0x36FF0050u, 0x54FF30FFu, 0xC4830824u, 0x7230680Cu,
      0xFF500050u, 0x83042454u, 0x585E0CC4u, 0x909090C3u};

  return nox_eud_target_matches_dwords(target, helper, sizeof(helper) / sizeof(helper[0]));
}

static int nox_eud_target_is_xtra_object_spec_helper(uint32_t target)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;
  const unsigned char *code;
  int32_t relative;
  uint32_t call_target;

  if (!nox_eud_pointer_decode(target, 24, &entry, &offset) ||
      entry->kind != NOX_EUD_POINTER_SCRIPT_LOCALS)
    return 0;
  code = entry->host + offset;
  if (code[0] != 0xE8 || code[5] != 0x50 || code[6] != 0xE8 || code[11] != 0x50 ||
      code[12] != 0xE8 || code[17] != 0x58 || code[18] != 0x58 || code[19] != 0x31 ||
      code[20] != 0xC0 || code[21] != 0xC3 || code[22] != 0x90 || code[23] != 0x90)
    return 0;

  relative = (int32_t)nox_eud_host_read_u32(code + 1);
  call_target = target + 5u + (uint32_t)relative;
  if (call_target != 0x00507250u)
    return 0;
  relative = (int32_t)nox_eud_host_read_u32(code + 7);
  call_target = target + 11u + (uint32_t)relative;
  if (call_target != 0x004E3450u)
    return 0;
  relative = (int32_t)nox_eud_host_read_u32(code + 13);
  call_target = target + 17u + (uint32_t)relative;
  return call_target == 0x00507230u;
}

static int nox_eud_target_is_wall_lookup_helper(uint32_t target)
{
  static const unsigned char helper[] = {
      0xB8, 0x50, 0x72, 0x50, 0x00, 0xFF, 0xD0, 0xFF, 0x70, 0x04, 0xFF, 0x30,
      0xB8, 0x80, 0x05, 0x41, 0x00, 0xFF, 0xD0, 0x50, 0xB8, 0x30, 0x72, 0x50,
      0x00, 0xFF, 0xD0, 0x58, 0x31, 0xC0, 0x83, 0xC4, 0x08, 0xC3};

  return nox_eud_target_matches_bytes(target, helper, sizeof(helper));
}

static int nox_eud_target_is_create_magic_wall_helper(uint32_t target)
{
  static const unsigned char helper[] = {
      0xB8, 0x50, 0x72, 0x50, 0x00, 0xFF, 0xD0, 0xFF, 0x70, 0x10, 0xFF, 0x70,
      0x0C, 0xFF, 0x70, 0x08, 0xFF, 0x70, 0x04, 0xFF, 0x30, 0xB8, 0x00, 0xFD,
      0x4F, 0x00, 0xFF, 0xD0, 0x83, 0xC4, 0x14, 0x31, 0xC0, 0xC3};

  return nox_eud_target_matches_bytes(target, helper, sizeof(helper));
}

static int nox_eud_target_is_add_breakable_wall_helper(uint32_t target)
{
  static const unsigned char helper[] = {
      0xB8, 0x50, 0x72, 0x50, 0x00, 0xFF, 0xD0, 0xFF, 0x30, 0xB8,
      0x40, 0x08, 0x41, 0x00, 0xFF, 0xD0, 0x58, 0x31, 0xC0, 0xC3};

  return nox_eud_target_matches_bytes(target, helper, sizeof(helper));
}

static int nox_eud_target_is_net_load_fx_helper(uint32_t target)
{
  static const unsigned char helper[] = {
      0xB8, 0x50, 0x72, 0x50, 0x00, 0xFF, 0xD0, 0xFF, 0x70, 0x0C, 0xFF, 0x70,
      0x08, 0xFF, 0x70, 0x04, 0xFF, 0x30, 0xB8, 0x50, 0x31, 0x52, 0x00, 0xFF,
      0xD0, 0x83, 0xC4, 0x10, 0x31, 0xC0, 0xC3, 0x90};

  return nox_eud_target_matches_bytes(target, helper, sizeof(helper));
}

static int nox_eud_target_is_summon_fx_helper(uint32_t target)
{
  static const unsigned char helper[] = {
      0xB8, 0x50, 0x72, 0x50, 0x00, 0xFF, 0xD0, 0xFF, 0x30, 0xFF, 0x70, 0x04,
      0xFF, 0x70, 0x08, 0xFF, 0x70, 0x0C, 0xFF, 0x70, 0x10, 0xB8, 0xF0, 0x36,
      0x52, 0x00, 0xFF, 0xD0, 0x83, 0xC4, 0x14, 0x31, 0xC0, 0xC3};

  return nox_eud_target_matches_bytes(target, helper, sizeof(helper));
}

static int nox_eud_target_is_npc_equipment_helper(uint32_t target, int *equip_mode)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;
  const unsigned char *code;
  int32_t relative;
  uint32_t call_target;

  if (!equip_mode || !nox_eud_pointer_decode(target, 36, &entry, &offset) ||
      entry->kind != NOX_EUD_POINTER_SCRIPT_LOCALS)
    return 0;
  code = entry->host + offset;
  if (code[0] != 0x55 || code[1] != 0x8B || code[2] != 0xEC || code[3] != 0x50 ||
      code[4] != 0x83 || code[5] != 0xEC || code[6] != 0x04 || code[7] != 0xE8 ||
      code[12] != 0x89 || code[13] != 0x04 || code[14] != 0x24 || code[15] != 0xE8 ||
      code[20] != 0xFF || code[21] != 0x34 || code[22] != 0x24 || code[23] != 0x50 ||
      code[24] != 0xE8 || code[29] != 0x83 || code[30] != 0xC4 || code[31] != 0x0C ||
      code[32] != 0x58 || code[33] != 0x5D || code[34] != 0xC3 || code[35] != 0x90)
    return 0;

  relative = (int32_t)nox_eud_host_read_u32(code + 8);
  if (target + 12u + (uint32_t)relative != 0x00507250u)
    return 0;
  relative = (int32_t)nox_eud_host_read_u32(code + 16);
  if (target + 20u + (uint32_t)relative != 0x00507250u)
    return 0;
  relative = (int32_t)nox_eud_host_read_u32(code + 25);
  call_target = target + 29u + (uint32_t)relative;
  if (call_target == 0x004F2F70u)
    *equip_mode = 1;
  else if (call_target == 0x004F2FB0u)
    *equip_mode = 0;
  else
    return 0;
  return 1;
}

static int nox_eud_target_is_play_sound_helper(uint32_t target)
{
  static const uint32_t helper[] = {
      0x50196068u, 0x72506800u, 0x50560050u, 0x082454FFu,
      0x54FFF08Bu, 0x006A0824u, 0x5650006Au, 0x1C2454FFu,
      0x5810C483u, 0x08C4835Eu, 0x909090C3u};

  return nox_eud_target_matches_dwords(target, helper, sizeof(helper) / sizeof(helper[0]));
}

static int nox_eud_target_is_collide_callback_helper(uint32_t target)
{
  static const uint32_t helper[] = {
      0x50731068u, 0x50565500u, 0x14246C8Bu, 0x1824748Bu,
      0x02FC858Bu, 0x56550000u, 0x2454FF50u, 0x0CC48318u,
      0x835D5E58u, 0x90C304C4u};

  return nox_eud_target_matches_dwords(target, helper, sizeof(helper) / sizeof(helper[0]));
}

static int nox_eud_target_is_pickup_callback_helper(uint32_t target)
{
  static const uint32_t helper[] = {
      0x50731068u, 0x55565300u, 0x14245C8Bu, 0x1824748Bu,
      0x00B8AE8Bu, 0x53560000u, 0x2454FF55u, 0x0CC48318u,
      0x835B5E5Du, 0x90C304C4u};

  return nox_eud_target_matches_dwords(target, helper, sizeof(helper) / sizeof(helper[0]));
}

static int nox_eud_target_is_discard_callback_helper(uint32_t target)
{
  static const uint32_t helper[] = {
      0x50731068u, 0x55565300u, 0x14245C8Bu, 0x1824748Bu,
      0x0090AE8Bu, 0x53560000u, 0x2454FF55u, 0x0CC48318u,
      0x835B5E5Du, 0x90C304C4u};

  return nox_eud_target_matches_dwords(target, helper, sizeof(helper) / sizeof(helper[0]));
}

static int nox_eud_target_is_death_callback_helper(uint32_t target)
{
  static const uint32_t helper[] = {
      0x50731068u, 0x8B565000u, 0x8B102474u, 0x00022886u,
      0x006A5600u, 0x2454FF50u, 0x0CC48314u, 0xC483585Eu,
      0x9090C304u};

  return nox_eud_target_matches_dwords(target, helper, sizeof(helper) / sizeof(helper[0]));
}

static int nox_eud_target_is_use_item_callback_helper(uint32_t target)
{
  static const uint32_t helper[] = {
      0x50731068u, 0x50565500u, 0x1424748Bu, 0x18246C8Bu,
      0x02FC858Bu, 0x56550000u, 0x2454FF50u, 0x0CC48318u,
      0x835D5E58u, 0x90C304C4u};

  return nox_eud_target_matches_dwords(target, helper, sizeof(helper) / sizeof(helper[0]));
}

#define NOX_EUD_CALLBACK_SLOT_90_VALID 0x01u
#define NOX_EUD_CALLBACK_SLOT_B8_VALID 0x02u
#define NOX_EUD_CALLBACK_SLOT_228_VALID 0x04u
#define NOX_EUD_CALLBACK_SLOT_2FC_VALID 0x08u

static uint32_t nox_eud_callback_handler_offset(nox_eud_callback_type type)
{
  switch (type)
  {
    case NOX_EUD_CALLBACK_COLLIDE:
      return NOX_EUD_OBJECT_COLLIDE_HANDLER_OFFSET;
    case NOX_EUD_CALLBACK_PICKUP:
      return NOX_EUD_OBJECT_PICKUP_HANDLER_OFFSET;
    case NOX_EUD_CALLBACK_DISCARD:
      return NOX_EUD_OBJECT_DISCARD_HANDLER_OFFSET;
    case NOX_EUD_CALLBACK_DEATH:
      return NOX_EUD_OBJECT_DEATH_HANDLER_OFFSET;
    case NOX_EUD_CALLBACK_USE_ITEM:
      return NOX_EUD_OBJECT_USE_ITEM_HANDLER_OFFSET;
    default:
      return UINT32_MAX;
  }
}

static uint32_t nox_eud_callback_slot_offset(nox_eud_callback_type type)
{
  switch (type)
  {
    case NOX_EUD_CALLBACK_COLLIDE:
    case NOX_EUD_CALLBACK_USE_ITEM:
      return NOX_EUD_OBJECT_SHARED_CALLBACK_OFFSET;
    case NOX_EUD_CALLBACK_PICKUP:
      return NOX_EUD_OBJECT_PICKUP_CALLBACK_OFFSET;
    case NOX_EUD_CALLBACK_DISCARD:
      return NOX_EUD_OBJECT_DISCARD_CALLBACK_OFFSET;
    case NOX_EUD_CALLBACK_DEATH:
      return NOX_EUD_OBJECT_DEATH_CALLBACK_OFFSET;
    default:
      return UINT32_MAX;
  }
}

static uint32_t nox_eud_callback_wrapper_address(nox_eud_callback_type type)
{
  uintptr_t address;

  switch (type)
  {
    case NOX_EUD_CALLBACK_COLLIDE:
      address = (uintptr_t)nox_eud_callback_collide;
      break;
    case NOX_EUD_CALLBACK_PICKUP:
      address = (uintptr_t)nox_eud_callback_pickup;
      break;
    case NOX_EUD_CALLBACK_DISCARD:
      address = (uintptr_t)nox_eud_callback_discard;
      break;
    case NOX_EUD_CALLBACK_DEATH:
      address = (uintptr_t)nox_eud_callback_death;
      break;
    case NOX_EUD_CALLBACK_USE_ITEM:
      address = (uintptr_t)nox_eud_callback_use_item;
      break;
    default:
      return 0;
  }

  if (address > UINT32_MAX)
    return 0;
  return (uint32_t)address;
}

static int nox_eud_callback_entry_is_valid(const nox_eud_callback_entry *entry)
{
  int current;
  uint32_t current_id;

  if (!entry || !entry->object)
    return 0;
  current_id = nox_eud_host_read_u32(entry->object + NOX_EUD_OBJECT_SCRIPT_ID_OFFSET);
  if (current_id != (uint32_t)entry->object_id || (entry->object[16] & 0x20) != 0)
    return 0;
  current = sub_511B60(entry->object_id);
  return current == (int)(uintptr_t)entry->object;
}

static unsigned int nox_eud_callback_hash(const unsigned char *object)
{
  uintptr_t value = (uintptr_t)object;

  value ^= value >> 11;
  value ^= value >> 19;
  return (unsigned int)(value >> 4) & NOX_EUD_CALLBACK_ENTRY_MASK;
}

static nox_eud_callback_entry *nox_eud_callback_entry_for_object(unsigned char *object, int create)
{
  unsigned int start;
  unsigned int i;
  nox_eud_callback_entry *entry;
  nox_eud_callback_entry *free_entry = 0;
  int object_id;

  if (!object || (object[16] & 0x20) != 0)
    return 0;
  object_id = (int)nox_eud_host_read_u32(object + NOX_EUD_OBJECT_SCRIPT_ID_OFFSET);
  if (sub_511B60(object_id) != (int)(uintptr_t)object)
    return 0;
  start = nox_eud_callback_hash(object);

  for (i = 0; i < NOX_EUD_CALLBACK_ENTRY_COUNT; ++i)
  {
    entry = &nox_eud_callbacks[(start + i) & NOX_EUD_CALLBACK_ENTRY_MASK];
    if (entry->object == object && entry->object_id == object_id)
    {
      if (nox_eud_callback_entry_is_valid(entry))
        return entry;
      if (!free_entry)
        free_entry = entry;
    }
    else if (!entry->object)
    {
      if (!free_entry)
        free_entry = entry;
      break;
    }
    else if (create && !free_entry && !nox_eud_callback_entry_is_valid(entry))
    {
      free_entry = entry;
    }
  }

  if (!create || !free_entry)
    return 0;
  memset(free_entry, 0, sizeof(*free_entry));
  free_entry->object = object;
  free_entry->object_id = object_id;
  free_entry->callback_90 = -1;
  free_entry->callback_B8 = -1;
  free_entry->callback_228 = -1;
  free_entry->callback_2FC = -1;
  free_entry->pickup_once_callback = -1;
  free_entry->discard_bypass_callback = -1;
  return free_entry;
}

static int nox_eud_script_function_is_valid(int callback)
{
  uint32_t table;
  uint32_t count;

  if (callback < 0)
    return 0;
  table = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_SCRIPT_TABLE_POINTER));
  count = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_SCRIPT_FUNCTION_COUNT));
  return table != 0 && (uint32_t)callback < count;
}

static int nox_eud_callback_target_matches(nox_eud_callback_type type, uint32_t target)
{
  switch (type)
  {
    case NOX_EUD_CALLBACK_COLLIDE:
      return nox_eud_target_is_collide_callback_helper(target);
    case NOX_EUD_CALLBACK_PICKUP:
      return nox_eud_target_is_pickup_callback_helper(target);
    case NOX_EUD_CALLBACK_DISCARD:
      return nox_eud_target_is_discard_callback_helper(target);
    case NOX_EUD_CALLBACK_DEATH:
      return nox_eud_target_is_death_callback_helper(target);
    case NOX_EUD_CALLBACK_USE_ITEM:
      return nox_eud_target_is_use_item_callback_helper(target);
    default:
      return 0;
  }
}

static int nox_eud_callback_for_type(const nox_eud_callback_entry *entry, nox_eud_callback_type type,
                                     int *callback)
{
  unsigned int valid;
  int value;

  if (!entry || !callback)
    return 0;
  switch (type)
  {
    case NOX_EUD_CALLBACK_COLLIDE:
    case NOX_EUD_CALLBACK_USE_ITEM:
      valid = NOX_EUD_CALLBACK_SLOT_2FC_VALID;
      value = entry->callback_2FC;
      break;
    case NOX_EUD_CALLBACK_PICKUP:
      valid = NOX_EUD_CALLBACK_SLOT_B8_VALID;
      value = entry->callback_B8;
      break;
    case NOX_EUD_CALLBACK_DISCARD:
      valid = NOX_EUD_CALLBACK_SLOT_90_VALID;
      value = entry->callback_90;
      break;
    case NOX_EUD_CALLBACK_DEATH:
      valid = NOX_EUD_CALLBACK_SLOT_228_VALID;
      value = entry->callback_228;
      break;
    default:
      return 0;
  }
  if (!(entry->slot_valid_mask & valid) || !nox_eud_script_function_is_valid(value))
    return 0;
  *callback = value;
  return 1;
}

static int nox_eud_callback_install(nox_eud_callback_entry *entry, nox_eud_callback_type type)
{
  uint32_t handler_offset;
  uint32_t wrapper;
  unsigned int bit;

  if (!entry || !nox_eud_callback_entry_is_valid(entry))
    return 0;
  handler_offset = nox_eud_callback_handler_offset(type);
  wrapper = nox_eud_callback_wrapper_address(type);
  if (handler_offset == UINT32_MAX || !wrapper)
    return 0;
  bit = 1u << (unsigned int)type;
  nox_eud_host_write_u32(entry->object + handler_offset, wrapper);
  entry->installed_mask |= bit;
  entry->pending_mask &= ~bit;
  return 1;
}

static void nox_eud_callback_install_pending_for_slot(nox_eud_callback_entry *entry, uint32_t slot_offset)
{
  unsigned int i;

  if (!entry)
    return;
  for (i = 0; i < NOX_EUD_CALLBACK_COUNT; ++i)
  {
    nox_eud_callback_type type = (nox_eud_callback_type)i;
    if ((entry->pending_mask & (1u << i)) && nox_eud_callback_slot_offset(type) == slot_offset)
      nox_eud_callback_install(entry, type);
  }
}

static int nox_eud_callback_write_shadow(nox_eud_callback_entry *entry, uint32_t offset, int callback)
{
  unsigned int relevant_mask = 0;

  if (!entry)
    return 0;
  if (offset == NOX_EUD_OBJECT_SHARED_CALLBACK_OFFSET)
  {
    relevant_mask = (1u << NOX_EUD_CALLBACK_COLLIDE) | (1u << NOX_EUD_CALLBACK_USE_ITEM);
    if (!((entry->pending_mask | entry->installed_mask) & relevant_mask))
      return 0;
    if (!nox_eud_script_function_is_valid(callback))
      return 1;
    entry->callback_2FC = callback;
    entry->slot_valid_mask |= NOX_EUD_CALLBACK_SLOT_2FC_VALID;
  }
  else if (offset == NOX_EUD_OBJECT_PICKUP_CALLBACK_OFFSET)
  {
    relevant_mask = 1u << NOX_EUD_CALLBACK_PICKUP;
    if (!((entry->pending_mask | entry->installed_mask) & relevant_mask))
      return 0;
    if (!nox_eud_script_function_is_valid(callback))
      return 1;
    entry->callback_B8 = callback;
    entry->slot_valid_mask |= NOX_EUD_CALLBACK_SLOT_B8_VALID;
  }
  else if (offset == NOX_EUD_OBJECT_DISCARD_CALLBACK_OFFSET)
  {
    relevant_mask = 1u << NOX_EUD_CALLBACK_DISCARD;
    if (!((entry->pending_mask | entry->installed_mask) & relevant_mask))
      return 0;
    if (!nox_eud_script_function_is_valid(callback))
      return 1;
    entry->callback_90 = callback;
    entry->slot_valid_mask |= NOX_EUD_CALLBACK_SLOT_90_VALID;
  }
  else if (offset == NOX_EUD_OBJECT_DEATH_CALLBACK_OFFSET)
  {
    relevant_mask = 1u << NOX_EUD_CALLBACK_DEATH;
    if (!((entry->pending_mask | entry->installed_mask) & relevant_mask))
      return 0;
    if (!nox_eud_script_function_is_valid(callback))
      return 1;
    entry->callback_228 = callback;
    entry->slot_valid_mask |= NOX_EUD_CALLBACK_SLOT_228_VALID;
  }
  else
  {
    return 0;
  }

  nox_eud_callback_install_pending_for_slot(entry, offset);
  return 1;
}

static int nox_eud_try_callback_write(uint32_t address, uint32_t value)
{
  nox_eud_pointer_entry *pointer;
  nox_eud_callback_entry *entry;
  uint32_t offset;
  unsigned int i;

  if (!nox_eud_pointer_decode(address, 4, &pointer, &offset) || pointer->kind != NOX_EUD_POINTER_OBJECT)
    return 0;

  entry = nox_eud_callback_entry_for_object(pointer->host, 0);

  if (offset == NOX_EUD_OBJECT_DISCARD_HANDLER_OFFSET)
  {
    int callback;
    uint32_t previous;
    uint32_t previous_handler = 0;
    unsigned int previous_eud = 0;
    nox_eud_pointer_entry *previous_entry;
    uint32_t previous_offset;
    uintptr_t wrapper_address;

    if (nox_eud_target_is_discard_bypass_helper(value, &callback, &previous))
    {
      if (entry && (entry->installed_mask & (1u << NOX_EUD_CALLBACK_DISCARD)) &&
          previous == entry->helper_target[NOX_EUD_CALLBACK_DISCARD])
      {
        previous_eud = 1;
      }
      else if (previous && nox_eud_pointer_decode(previous, 1, &previous_entry, &previous_offset) &&
               previous_entry->kind == NOX_EUD_POINTER_NATIVE_HANDLER && previous_offset == 0 &&
               previous_entry->owner == pointer->host &&
               nox_eud_host_read_u32(pointer->host + offset) ==
                   (uint32_t)(uintptr_t)previous_entry->host)
      {
        previous_handler = (uint32_t)(uintptr_t)previous_entry->host;
      }
      else if (previous != 0)
      {
        return 1;
      }

      if (!entry)
        entry = nox_eud_callback_entry_for_object(pointer->host, 1);
      if (!entry)
        return 1;
      if (!(entry->installed_mask & (1u << NOX_EUD_CALLBACK_DISCARD)) &&
          !(entry->pending_mask & (1u << NOX_EUD_CALLBACK_DISCARD)))
        entry->original_handler[NOX_EUD_CALLBACK_DISCARD] = nox_eud_host_read_u32(entry->object + offset);

      wrapper_address = (uintptr_t)nox_eud_callback_discard_bypass;
      if (wrapper_address > UINT32_MAX)
        return 1;
      entry->discard_bypass_callback = callback;
      entry->discard_bypass_previous_handler = previous_handler;
      entry->discard_bypass_previous_eud = previous_eud;
      entry->helper_target[NOX_EUD_CALLBACK_DISCARD] = value;
      entry->pending_mask &= ~(1u << NOX_EUD_CALLBACK_DISCARD);
      entry->installed_mask |= 1u << NOX_EUD_CALLBACK_DISCARD;
      nox_eud_host_write_u32(entry->object + offset, (uint32_t)wrapper_address);
      return 1;
    }
  }

  for (i = 0; i < NOX_EUD_CALLBACK_COUNT; ++i)
  {
    nox_eud_callback_type type = (nox_eud_callback_type)i;
    unsigned int bit = 1u << i;

    if (offset != nox_eud_callback_handler_offset(type))
      continue;
    if (!nox_eud_callback_target_matches(type, value))
      return 1;
    if (!entry)
      entry = nox_eud_callback_entry_for_object(pointer->host, 1);
    if (!entry)
      return 1;
    if (!(entry->installed_mask & bit) && !(entry->pending_mask & bit))
      entry->original_handler[type] = nox_eud_host_read_u32(entry->object + offset);
    entry->helper_target[type] = value;
    entry->pending_mask |= bit;
    return 1;
  }

  if (offset == NOX_EUD_OBJECT_PICKUP_ONCE_CALLBACK_OFFSET)
  {
    if (value != UINT32_MAX && !nox_eud_script_function_is_valid((int)value))
      return 1;
    if (!entry)
      entry = nox_eud_callback_entry_for_object(pointer->host, 1);
    if (!entry)
      return 1;
    entry->pickup_once_callback = (int)value;
    entry->pickup_once_tracked = 1;
    nox_eud_host_write_u32(entry->object + offset, value);
    return 1;
  }

  if (entry && nox_eud_callback_write_shadow(entry, offset, (int)value))
    return 1;
  return 0;
}

static int nox_eud_try_callback_read(uint32_t address, uint32_t *value)
{
  nox_eud_pointer_entry *pointer;
  nox_eud_callback_entry *entry;
  uint32_t offset;
  unsigned int i;

  if (!value || !nox_eud_pointer_decode(address, 4, &pointer, &offset) ||
      pointer->kind != NOX_EUD_POINTER_OBJECT)
    return 0;
  entry = nox_eud_callback_entry_for_object(pointer->host, 0);

  if (entry)
  {
    for (i = 0; i < NOX_EUD_CALLBACK_COUNT; ++i)
    {
      if (offset == nox_eud_callback_handler_offset((nox_eud_callback_type)i) &&
          entry->helper_target[i] != 0)
      {
        *value = entry->helper_target[i];
        return 1;
      }
    }
    if (offset == NOX_EUD_OBJECT_SHARED_CALLBACK_OFFSET &&
        (entry->slot_valid_mask & NOX_EUD_CALLBACK_SLOT_2FC_VALID))
    {
      *value = (uint32_t)entry->callback_2FC;
      return 1;
    }
    if (offset == NOX_EUD_OBJECT_PICKUP_CALLBACK_OFFSET &&
        (entry->slot_valid_mask & NOX_EUD_CALLBACK_SLOT_B8_VALID))
    {
      *value = (uint32_t)entry->callback_B8;
      return 1;
    }
    if (offset == NOX_EUD_OBJECT_DISCARD_CALLBACK_OFFSET &&
        (entry->slot_valid_mask & NOX_EUD_CALLBACK_SLOT_90_VALID))
    {
      *value = (uint32_t)entry->callback_90;
      return 1;
    }
    if (offset == NOX_EUD_OBJECT_DEATH_CALLBACK_OFFSET &&
        (entry->slot_valid_mask & NOX_EUD_CALLBACK_SLOT_228_VALID))
    {
      *value = (uint32_t)entry->callback_228;
      return 1;
    }
  }

  if (offset == NOX_EUD_OBJECT_DISCARD_HANDLER_OFFSET)
  {
    uint32_t handler = nox_eud_host_read_u32(pointer->host + offset);
    *value = handler ? nox_eud_register_native_handler(handler, pointer->host) : 0;
    return 1;
  }

  if (offset == NOX_EUD_OBJECT_PICKUP_ONCE_CALLBACK_OFFSET)
  {
    *value = nox_eud_host_read_u32(pointer->host + offset);
    return 1;
  }
  return 0;
}

static nox_eud_callback_entry *nox_eud_callback_for_host(int object)
{
  if (!object)
    return 0;
  return nox_eud_callback_entry_for_object((unsigned char *)(uintptr_t)(uint32_t)object, 0);
}

static int __cdecl nox_eud_callback_collide(int self, int other, float2 *point)
{
  nox_eud_callback_entry *entry = nox_eud_callback_for_host(self);
  int callback;

  (void)point;
  if (!entry || !nox_eud_callback_for_type(entry, NOX_EUD_CALLBACK_COLLIDE, &callback))
    return 0;
  sub_507310(callback, other, self);
  return 1;
}

static int __cdecl nox_eud_callback_pickup(int holder, int item, int flags, int arg4)
{
  nox_eud_callback_entry *entry = nox_eud_callback_for_host(item);
  int callback;

  (void)flags;
  (void)arg4;
  if (!entry || !nox_eud_callback_for_type(entry, NOX_EUD_CALLBACK_PICKUP, &callback))
    return 0;
  return sub_507310(callback, holder, item);
}

static int __cdecl nox_eud_callback_discard(int holder, int item, float2 *point)
{
  nox_eud_callback_entry *entry = nox_eud_callback_for_host(item);
  int callback;

  (void)point;
  if (!entry || !nox_eud_callback_for_type(entry, NOX_EUD_CALLBACK_DISCARD, &callback))
    return 0;
  return sub_507310(callback, holder, item);
}

static int __cdecl nox_eud_callback_discard_bypass(int holder, int item, float2 *point)
{
  nox_eud_callback_entry *entry = nox_eud_callback_for_host(item);
  int callback;
  int (__cdecl *previous)(int, int, float2 *);

  if (!entry || !nox_eud_script_function_is_valid(entry->discard_bypass_callback))
    return sub_4ED290(holder, item, point);

  sub_507310(entry->discard_bypass_callback, holder, item);
  if (entry->discard_bypass_previous_eud &&
      nox_eud_callback_for_type(entry, NOX_EUD_CALLBACK_DISCARD, &callback))
    return sub_507310(callback, holder, item);

  if (entry->discard_bypass_previous_handler)
  {
    previous = (int (__cdecl *)(int, int, float2 *))(uintptr_t)entry->discard_bypass_previous_handler;
    return previous(holder, item, point);
  }
  return sub_4ED290(holder, item, point);
}

static void __cdecl nox_eud_callback_death(int self)
{
  nox_eud_callback_entry *entry = nox_eud_callback_for_host(self);
  int callback;

  if (entry && nox_eud_callback_for_type(entry, NOX_EUD_CALLBACK_DEATH, &callback))
    sub_507310(callback, 0, self);
}

static int __cdecl nox_eud_callback_use_item(int user, int item)
{
  nox_eud_callback_entry *entry = nox_eud_callback_for_host(item);
  int callback;

  if (!entry || !nox_eud_callback_for_type(entry, NOX_EUD_CALLBACK_USE_ITEM, &callback))
    return 0;
  sub_507310(callback, user, item);
  return 1;
}

static int nox_eud_object_read_allowed(uint32_t offset, size_t size)
{
  uint64_t end = (uint64_t)offset + size;

  if (offset >= 4 && end <= 20)
    return 1;
  if (offset >= 44 && end <= 48)
    return 1;
  if (offset >= 56 && end <= 64)
    return 1;
  if (offset >= 0x7C && end <= 0x80)
    return 1;
  if (size == 4 && (offset == 0x1C || offset == 0x34 || offset == 0x78 ||
                    offset == 0x21C || offset == 0x220 || offset == 0x224))
    return 1;
  if (offset >= NOX_EUD_OBJECT_OWNER_OFFSET && end <= NOX_EUD_OBJECT_OWNER_OFFSET + 4)
    return 1;
  if (offset >= NOX_EUD_OBJECT_PLAYER_EXT_OFFSET && end <= NOX_EUD_OBJECT_PLAYER_EXT_OFFSET + 4)
    return 1;
  return 0;
}

static int nox_eud_monster_ext_field_allowed(const nox_eud_pointer_entry *entry, uint32_t offset, size_t size)
{
  int object;
  unsigned char *object_host;

  object = sub_511B60(entry->object_id);
  if (!object)
    return 0;
  object_host = (unsigned char *)(uintptr_t)(uint32_t)object;
  if ((object_host[8] & 2) == 0)
    return 0;

  if (size == 4 && (offset == 0x228 || offset == 0x22C || offset == 0x230 ||
                    offset == 0x234 || offset == 0x238 || offset == 0x23C ||
                    offset == 0x520 || offset == 0x528 || offset == 0x54C ||
                    offset == 0x5A0 || offset == 0x5A8 || offset == 0x5B0 ||
                    offset == 0x5B8 || offset == 0x5C0 || offset == 0x5C8 ||
                    offset == 0x7F8))
    return 1;
  if (size == 4 && offset >= 0x5D0 && offset < 0x7F8 && (offset & 3u) == 0)
    return 1;
  return 0;
}

static int nox_eud_unit_ext_has_class(const nox_eud_pointer_entry *entry, unsigned char class_mask)
{
  int object;

  object = sub_511B60(entry->object_id);
  if (!object)
    return 0;
  return (*(unsigned char *)((uintptr_t)(uint32_t)object + 8) & class_mask) != 0;
}

static int nox_eud_unit_ext_read_allowed(const nox_eud_pointer_entry *entry, uint32_t offset, size_t size)
{
  uint64_t end = (uint64_t)offset + size;

  if (nox_eud_unit_ext_has_class(entry, 4))
  {
    if (offset >= 4 && end <= 12)
      return 1;
    if (offset >= 0x58 && end <= 0x5C)
      return 1;
    if (offset >= NOX_EUD_PLAYER_EXT_WEAPON_OFFSET && end <= NOX_EUD_PLAYER_EXT_NEXT_WEAPON_OFFSET + 4)
      return 1;
    if (offset >= NOX_EUD_PLAYER_EXT_INFO_OFFSET && end <= NOX_EUD_PLAYER_EXT_INFO_OFFSET + 4)
      return 1;
  }
  return nox_eud_monster_ext_field_allowed(entry, offset, size);
}

static int nox_eud_dynamic_read_allowed(const nox_eud_pointer_entry *entry, uint32_t offset, size_t size)
{
  uint32_t record_offset;

  switch (entry->kind)
  {
    case NOX_EUD_POINTER_OBJECT:
      return nox_eud_object_read_allowed(offset, size);

    case NOX_EUD_POINTER_UNIT_EXT:
      return nox_eud_unit_ext_read_allowed(entry, offset, size);

    case NOX_EUD_POINTER_MONSTER_ACTION:
      return size == 4 && (offset == 0 || offset == 4 || offset == 12 || offset == 16);

    case NOX_EUD_POINTER_WALL:
      if (size == 1 && (offset == 4 || offset == 7))
        return 1;
      return size == 4 && (offset == 4 || offset == 0x1C);

    case NOX_EUD_POINTER_WALL_DETAILS:
      return size == 4 && offset == 0x1C;

    case NOX_EUD_POINTER_TIMER_NODE:
      return offset >= 8 && (uint64_t)offset + size <= 12;

    case NOX_EUD_POINTER_SCRIPT_TABLE:
      record_offset = offset % NOX_EUD_SCRIPT_RECORD_SIZE;
      return size == 4 && record_offset == NOX_EUD_SCRIPT_LOCALS_OFFSET;

    case NOX_EUD_POINTER_SCRIPT_LOCALS:
    case NOX_EUD_POINTER_CSTRING:
    case NOX_EUD_POINTER_WSTRING:
      return 1;

    case NOX_EUD_POINTER_ALLOC:
      if ((entry->flags & NOX_EUD_ALLOC_MAGIC_MISSILE) && offset < 8u && (uint64_t)offset + size > 0)
        return size == 4 && (offset == 0 || offset == 4);
      return 1;

    case NOX_EUD_POINTER_THINGDB_TABLE:
    case NOX_EUD_POINTER_SPRITE_TABLE:
      return size == 4 && (offset & 3u) == 0;

    case NOX_EUD_POINTER_THINGDB_RECORD:
      if (size != 4)
        return 0;
      return offset == 1u * 4u || offset == 7u * 4u || offset == 12u * 4u ||
             offset == 13u * 4u || offset == 14u * 4u || offset == 16u * 4u ||
             offset == 50u * 4u || offset == 51u * 4u || offset == 52u * 4u;

    case NOX_EUD_POINTER_THINGDB_VALUE:
      return size == 4 && offset == 0;

    case NOX_EUD_POINTER_SPRITE_RECORD:
      if (size != 4)
        return 0;
      return offset == 1u * 4u || offset == 2u * 4u || offset == 8u * 4u ||
             offset == 11u * 4u || offset == 12u * 4u || offset == 13u * 4u ||
             offset == 14u * 4u || offset == 16u * 4u || offset == 18u * 4u;

    case NOX_EUD_POINTER_GAMEDATA_ROOT:
      record_offset = offset % NOX_EUD_GAMEDATA_TABLE_RECORD_SIZE;
      return size == 4 && (record_offset == 0 || record_offset == 4);

    case NOX_EUD_POINTER_GAMEDATA_SUBTABLE:
      return size == 4 && (offset & 3u) == 0;

    case NOX_EUD_POINTER_GAMEDATA_VALUE_DESC:
      return size == 4 && (offset == 0 || offset == 4);

    case NOX_EUD_POINTER_GAMEDATA_VALUES:
      return size == 4 && (offset & 3u) == 0;

    default:
      return 0;
  }
}

static int nox_eud_dynamic_write_allowed(const nox_eud_pointer_entry *entry, uint32_t offset, size_t size)
{
  uint64_t end = (uint64_t)offset + size;

  switch (entry->kind)
  {
    case NOX_EUD_POINTER_OBJECT:
      if (offset >= 16 && end <= 20)
        return 1;
      return size == 4 && (offset == 0x0C || offset == 0x1C || offset == 0x34 ||
                           offset == 0x78 || offset == 0x220 || offset == 0x224);

    case NOX_EUD_POINTER_UNIT_EXT:
      if (nox_eud_unit_ext_has_class(entry, 4))
      {
        if (offset >= 4 && end <= 10)
          return 1;
        if (offset >= 0x58 && end <= 0x5C)
          return 1;
      }
      return nox_eud_monster_ext_field_allowed(entry, offset, size);

    case NOX_EUD_POINTER_MONSTER_ACTION:
      return size == 4 && (offset == 4 || offset == 12 || offset == 16);

    case NOX_EUD_POINTER_WALL:
      return size == 1 && (offset == 4 || offset == 7);

    case NOX_EUD_POINTER_SCRIPT_LOCALS:
    case NOX_EUD_POINTER_GAMEDATA_VALUES:
      return 1;

    case NOX_EUD_POINTER_ALLOC:
      if ((entry->flags & NOX_EUD_ALLOC_MAGIC_MISSILE) && offset < 8u && (uint64_t)offset + size > 0)
        return size == 4 && (offset == 0 || offset == 4);
      return 1;

    case NOX_EUD_POINTER_THINGDB_VALUE:
      return size == 4 && offset == 0;

    case NOX_EUD_POINTER_THINGDB_RECORD:
      if (size != 4)
        return 0;
      return offset == 7u * 4u || offset == 12u * 4u || offset == 13u * 4u ||
             offset == 14u * 4u || offset == 16u * 4u || offset == 50u * 4u ||
             offset == 51u * 4u || offset == 52u * 4u;

    case NOX_EUD_POINTER_SPRITE_RECORD:
      if (size != 4)
        return 0;
      return offset == 1u * 4u || offset == 2u * 4u || offset == 8u * 4u ||
             offset == 11u * 4u || offset == 12u * 4u || offset == 13u * 4u ||
             offset == 14u * 4u || offset == 16u * 4u || offset == 18u * 4u;

    default:
      return 0;
  }
}

static int nox_eud_dynamic_read_bytes(uint32_t address, unsigned char *bytes, size_t size,
                                      nox_eud_pointer_entry **entry_out, uint32_t *offset_out)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;
  size_t i;

  if (!bytes || !nox_eud_pointer_decode(address, size, &entry, &offset) ||
      !nox_eud_dynamic_read_allowed(entry, offset, size))
    return 0;

  for (i = 0; i < size; ++i)
    bytes[i] = entry->host[offset + i];

  if (entry_out)
    *entry_out = entry;
  if (offset_out)
    *offset_out = offset;
  return 1;
}

static int nox_eud_dynamic_write_bytes(uint32_t address, const unsigned char *bytes, size_t size)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;
  size_t i;

  if (!bytes || !nox_eud_pointer_decode(address, size, &entry, &offset) ||
      !nox_eud_dynamic_write_allowed(entry, offset, size))
    return 0;

  for (i = 0; i < size; ++i)
    entry->host[offset + i] = bytes[i];
  return 1;
}

static uint32_t nox_eud_translate_dynamic_pointer(nox_eud_pointer_entry *entry, uint32_t offset, uint32_t value)
{
  uint32_t legacy;
  uint32_t locals_count;
  uint64_t locals_span;
  uint32_t token;
  int object_id;

  if (!value)
    return 0;

  switch (entry->kind)
  {
    case NOX_EUD_POINTER_OBJECT:
      if (offset == NOX_EUD_OBJECT_OWNER_OFFSET)
        return nox_eud_register_object_pointer(value);
      if (offset == NOX_EUD_OBJECT_PLAYER_EXT_OFFSET)
      {
        object_id = (int)nox_eud_host_read_u32(entry->host + NOX_EUD_OBJECT_SCRIPT_ID_OFFSET);
        token = nox_eud_register_unit_ext(value, object_id);
        if (token)
          return token;
        token = nox_eud_token_for_host_pointer((unsigned char *)(uintptr_t)value);
        return token ? token : 0;
      }
      break;

    case NOX_EUD_POINTER_UNIT_EXT:
      if (offset == NOX_EUD_PLAYER_EXT_WEAPON_OFFSET || offset == NOX_EUD_PLAYER_EXT_NEXT_WEAPON_OFFSET)
        return nox_eud_register_object_pointer(value);
      if (offset == NOX_EUD_PLAYER_EXT_INFO_OFFSET)
      {
        if (nox_eud_host_to_legacy_data((unsigned char *)(uintptr_t)value, &legacy))
          return legacy;
        return 0;
      }
      break;

    case NOX_EUD_POINTER_WALL:
      if (offset == 0x1C)
        return nox_eud_register_wall_details(value, entry->host, entry->object_id);
      break;

    case NOX_EUD_POINTER_SCRIPT_TABLE:
      if (offset % NOX_EUD_SCRIPT_RECORD_SIZE == NOX_EUD_SCRIPT_LOCALS_OFFSET)
      {
        locals_count = nox_eud_host_read_u32(entry->host + offset - NOX_EUD_SCRIPT_LOCALS_OFFSET +
                                             NOX_EUD_SCRIPT_LOCALS_COUNT_OFFSET);
        locals_span = (uint64_t)locals_count * 4u;
        if (!locals_span)
          locals_span = 4;
        if (locals_span >= NOX_EUD_TOKEN_STRIDE)
          return 0;
        return nox_eud_pointer_register(
            NOX_EUD_POINTER_SCRIPT_LOCALS, (unsigned char *)(uintptr_t)value, (uint32_t)locals_span, 0,
            entry->host + offset - NOX_EUD_SCRIPT_LOCALS_OFFSET);
      }
      break;

    case NOX_EUD_POINTER_ALLOC:
      if ((entry->flags & NOX_EUD_ALLOC_MAGIC_MISSILE) && (offset == 0 || offset == 4))
        return nox_eud_register_object_pointer(value);
      break;

    case NOX_EUD_POINTER_THINGDB_TABLE:
      if ((offset & 3u) == 0)
        return nox_eud_register_thingdb_record(value, entry, (int)(offset / 4u), 0);
      break;

    case NOX_EUD_POINTER_SPRITE_TABLE:
      if ((offset & 3u) == 0)
        return nox_eud_register_thingdb_record(value, entry, (int)(offset / 4u), 1);
      break;

    case NOX_EUD_POINTER_THINGDB_RECORD:
      if (offset == 4)
        return nox_eud_register_cstring(value);
      if (offset == 51u * 4u)
      {
        token = nox_eud_token_for_host_pointer((unsigned char *)(uintptr_t)value);
        return token ? token : nox_eud_register_thingdb_value(value, entry->host + offset);
      }
      break;

    case NOX_EUD_POINTER_SPRITE_RECORD:
      if (offset == 4 || offset == 8)
      {
        token = nox_eud_token_for_host_pointer((unsigned char *)(uintptr_t)value);
        if (token)
          return token;
        return nox_eud_register_cstring(value);
      }
      break;

    case NOX_EUD_POINTER_GAMEDATA_ROOT:
      if (offset % NOX_EUD_GAMEDATA_TABLE_RECORD_SIZE == 0)
        return nox_eud_register_gamedata_subtable(value, entry->host + offset);
      break;

    case NOX_EUD_POINTER_GAMEDATA_SUBTABLE:
      if (offset % NOX_EUD_GAMEDATA_SUBTABLE_RECORD_SIZE == 0)
        return nox_eud_register_cstring(value);
      if (offset % NOX_EUD_GAMEDATA_SUBTABLE_RECORD_SIZE == 4)
        return nox_eud_register_gamedata_value_desc(value, entry->host + offset - 4);
      break;

    case NOX_EUD_POINTER_GAMEDATA_VALUE_DESC:
      if (offset == 0)
        return nox_eud_register_gamedata_values(value, entry->host);
      break;

    default:
      break;
  }

  return value;
}

static uint32_t nox_eud_translate_mapped_pointer(uint32_t address, uint32_t value)
{
  uint32_t relative;

  if (!value)
    return 0;

  if (nox_eud_is_wide_string_pointer_field(address))
  {
    uint32_t token = nox_eud_token_for_host_pointer((unsigned char *)(uintptr_t)value);
    if (token)
      return token;
    return nox_eud_register_wstring(value);
  }

  if (address >= NOX_EUD_PLAYER_OBJECT_TABLE)
  {
    relative = address - NOX_EUD_PLAYER_OBJECT_TABLE;
    if (relative < NOX_EUD_PLAYER_COUNT * NOX_EUD_PLAYER_RECORD_STRIDE &&
        relative % NOX_EUD_PLAYER_RECORD_STRIDE == 0)
      return nox_eud_register_object_pointer(value);
  }

  if (address == NOX_EUD_SCRIPT_TABLE_POINTER)
    return nox_eud_register_script_table(value);

  if (address == NOX_EUD_TIMER_FREE_LIST_POINTER)
    return nox_eud_register_timer_node(value);

  if (address == NOX_EUD_ACTIVE_OBJECT_HEAD)
    return nox_eud_register_object_pointer(value);

  if (address == NOX_EUD_THINGDB_TABLE_POINTER)
    return nox_eud_register_thingdb_table(value, 0);

  if (address == NOX_EUD_THINGDB_SPRITE_TABLE_POINTER)
    return nox_eud_register_thingdb_table(value, 1);

  if (address == NOX_EUD_GAMEDATA_POINTER)
    return nox_eud_register_gamedata_root(value);

  if (address >= NOX_EUD_SCRIPT_STRING_TABLE &&
      address < NOX_EUD_SCRIPT_STRING_TABLE + 4u * nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_SCRIPT_STRING_COUNT)) &&
      ((address - NOX_EUD_SCRIPT_STRING_TABLE) & 3u) == 0)
    return nox_eud_register_cstring(value);

  return value;
}

static int nox_eud_resolve_object(uint32_t token, int *object)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;

  if (!object || !nox_eud_pointer_decode(token, 1, &entry, &offset) ||
      entry->kind != NOX_EUD_POINTER_OBJECT || offset != 0)
    return 0;
  *object = (int)(uintptr_t)entry->host;
  return 1;
}

static int nox_eud_resolve_wall(uint32_t token, int *wall)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;

  if (!wall || !nox_eud_pointer_decode(token, 1, &entry, &offset) ||
      entry->kind != NOX_EUD_POINTER_WALL || offset != 0)
    return 0;
  *wall = (int)(uintptr_t)entry->host;
  return 1;
}

static int nox_eud_mark_magic_missile_extension(unsigned char *object)
{
  uint32_t extension;
  uint32_t token;
  uint32_t value;
  uint32_t value2;
  uint32_t offset;
  nox_eud_pointer_entry *entry;
  int resolved;
  int resolved2;

  if (!object)
    return 0;
  extension = nox_eud_host_read_u32(object + NOX_EUD_OBJECT_PLAYER_EXT_OFFSET);
  token = extension ? nox_eud_token_for_host_pointer((unsigned char *)(uintptr_t)extension) : 0;
  if (!token)
    token = nox_eud_alloc_for_object(object, &entry);
  if (!token || !nox_eud_pointer_decode(token, 8, &entry, &offset) ||
      entry->kind != NOX_EUD_POINTER_ALLOC || offset != 0 || entry->span < 8)
    return 0;

  value = nox_eud_host_read_u32(entry->host);
  value2 = nox_eud_host_read_u32(entry->host + 4);
  if (!nox_eud_resolve_object(value, &resolved) || !nox_eud_resolve_object(value2, &resolved2))
    return 0;

  if ((uintptr_t)entry->host > UINT32_MAX)
    return 0;
  nox_eud_host_write_u32(entry->host, (uint32_t)resolved);
  nox_eud_host_write_u32(entry->host + 4, (uint32_t)resolved2);
  nox_eud_host_write_u32(object + NOX_EUD_OBJECT_PLAYER_EXT_OFFSET, (uint32_t)(uintptr_t)entry->host);
  entry->flags |= NOX_EUD_ALLOC_MAGIC_MISSILE;
  return 1;
}

static int nox_eud_try_semantic_pointer_write(uint32_t address, uint32_t value)
{
  nox_eud_pointer_entry *entry;
  nox_eud_pointer_entry *value_entry;
  uint32_t offset;
  uint32_t value_offset;
  uint32_t current_alloc;
  uint32_t existing_extension;
  unsigned char *host;
  uintptr_t function_pointer;
  int object;

  if (!nox_eud_pointer_decode(address, 4, &entry, &offset))
    return 0;

  if (entry->kind == NOX_EUD_POINTER_OBJECT)
  {
    if (offset == NOX_EUD_OBJECT_PLAYER_EXT_OFFSET)
    {
      if (!nox_eud_resolve_alloc_pointer(value, &host, &value_entry, &value_offset))
        return 1;
      current_alloc = nox_eud_alloc_for_object(entry->host, 0);
      existing_extension = nox_eud_host_read_u32(entry->host + offset);
      if ((entry->host[8] & 6) != 0 || existing_extension != 0 || value_offset != 0 ||
          value_entry->span < 8 || value_entry->owner != 0 ||
          (current_alloc && current_alloc != value))
        return 1;
      /* Keep the allocation pending until the known MagicMissile update write commits it. */
      value_entry->owner = entry->host;
      value_entry->object_id = (int)nox_eud_host_read_u32(entry->host + NOX_EUD_OBJECT_SCRIPT_ID_OFFSET);
      return 1;
    }
    if (offset == 0x2E8u)
    {
      if (value != NOX_EUD_MAGIC_MISSILE_UPDATE)
        return 1;
      function_pointer = (uintptr_t)sub_53BDA0;
      if (function_pointer > UINT32_MAX || !nox_eud_mark_magic_missile_extension(entry->host))
        return 1;
      nox_eud_host_write_u32(entry->host + offset, (uint32_t)function_pointer);
      return 1;
    }
  }

  if (entry->kind == NOX_EUD_POINTER_ALLOC && (entry->flags & NOX_EUD_ALLOC_MAGIC_MISSILE) &&
      (offset == 0 || offset == 4))
  {
    if (!nox_eud_resolve_object(value, &object))
      return 1;
    nox_eud_host_write_u32(entry->host + offset, (uint32_t)object);
    return 1;
  }

  if ((entry->kind == NOX_EUD_POINTER_THINGDB_RECORD && offset == 51u * 4u) ||
      (entry->kind == NOX_EUD_POINTER_SPRITE_RECORD && (offset == 4 || offset == 8)))
  {
    if (!nox_eud_record_restore(entry->host + offset))
      return 1;
    if (!value)
    {
      nox_eud_host_write_u32(entry->host + offset, 0);
      return 1;
    }
    if (!nox_eud_resolve_alloc_pointer(value, &host, &value_entry, &value_offset) ||
        (uintptr_t)host > UINT32_MAX)
      return 1;
    nox_eud_host_write_u32(entry->host + offset, (uint32_t)(uintptr_t)host);
    return 1;
  }

  return 0;
}

static int nox_eud_try_semantic_pointer_read(uint32_t address, uint32_t *value)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;
  uint32_t raw;
  uint32_t token;
  uintptr_t function_pointer;

  if (!value || !nox_eud_pointer_decode(address, 4, &entry, &offset))
    return 0;
  if (entry->kind == NOX_EUD_POINTER_OBJECT && offset == 0x2E8u)
  {
    raw = nox_eud_host_read_u32(entry->host + offset);
    function_pointer = (uintptr_t)sub_53BDA0;
    if (function_pointer <= UINT32_MAX && raw == (uint32_t)function_pointer)
      *value = NOX_EUD_MAGIC_MISSILE_UPDATE;
    else
      *value = 0;
    return 1;
  }
  if (entry->kind == NOX_EUD_POINTER_OBJECT && offset == NOX_EUD_OBJECT_PLAYER_EXT_OFFSET)
  {
    raw = nox_eud_host_read_u32(entry->host + offset);
    token = nox_eud_token_for_host_pointer((unsigned char *)(uintptr_t)raw);
    if (!token)
      token = nox_eud_alloc_for_object(entry->host, 0);
    if (token)
    {
      *value = token;
      return 1;
    }
  }
  return 0;
}

static int nox_eud_try_mapped_pointer_write(uint32_t address, uint32_t value)
{
  nox_eud_pointer_entry *entry;
  uint32_t offset;
  unsigned char *host;

  if (!nox_eud_is_wide_string_pointer_field(address) || !nox_eud_address_is_token(value))
    return 0;
  if (!nox_eud_pointer_decode(value, 1, &entry, &offset) ||
      (entry->kind != NOX_EUD_POINTER_ALLOC && entry->kind != NOX_EUD_POINTER_WSTRING))
    return 1;
  host = entry->host + offset;
  if ((uintptr_t)host > UINT32_MAX)
    return 1;
  if (entry->kind == NOX_EUD_POINTER_ALLOC && !nox_eud_record_restore(nox_eud_data_byte(address)))
    return 1;
  nox_eud_host_write_u32(nox_eud_data_byte(address), (uint32_t)(uintptr_t)host);
  return 1;
}

static int nox_eud_range_overlaps_protected_slot(uint32_t address, size_t size)
{
  uint64_t begin;
  uint64_t end;
  uint64_t slot_begin;
  uint64_t slot_end;

  if (!size)
    return 0;
  begin = address;
  end = begin + size;
  slot_begin = NOX_EUD_SMART_DESTRUCTOR_SLOT;
  slot_end = slot_begin + 4u;
  if (begin < slot_end && end > slot_begin)
    return 1;
  slot_begin = NOX_EUD_RECOVERY_DESTRUCTOR_SLOT;
  slot_end = slot_begin + 4u;
  return begin < slot_end && end > slot_begin;
}

static int nox_eud_try_mapped_shadow_read(uint32_t address, uint32_t *value)
{
  if (!value)
    return 0;
  if (address == NOX_EUD_SMART_DESTRUCTOR_SLOT)
  {
    *value = nox_eud_smart_destructor_target ? nox_eud_smart_destructor_target
                                             : NOX_EUD_SMART_DESTRUCTOR_ORIGINAL;
    return 1;
  }
  if (address == NOX_EUD_RECOVERY_DESTRUCTOR_SLOT)
  {
    *value = nox_eud_recovery_destructor_target ? nox_eud_recovery_destructor_target
                                                : NOX_EUD_RECOVERY_DESTRUCTOR_ORIGINAL;
    return 1;
  }
  return 0;
}

static int nox_eud_try_protected_mapped_write(uint32_t address, uint32_t value)
{
  if (address == NOX_EUD_SMART_DESTRUCTOR_SLOT)
  {
    if (value == NOX_EUD_SMART_MEMORY_CODE)
      nox_eud_smart_destructor_target = value;
    else if (value == NOX_EUD_SMART_DESTRUCTOR_ORIGINAL)
      nox_eud_smart_destructor_target = 0;
    return 1;
  }
  if (address == NOX_EUD_RECOVERY_DESTRUCTOR_SLOT)
  {
    if (value == NOX_EUD_RECOVERY_DESTRUCTOR_ORIGINAL)
      nox_eud_recovery_destructor_target = 0;
    else if (nox_eud_address_is_token(value) && nox_eud_recovery_chain_is_valid(value))
      nox_eud_recovery_destructor_target = value;
    return 1;
  }
  return 0;
}

static const char *nox_eud_script_string(uint32_t index)
{
  uint32_t count;
  uint32_t pointer;
  uint64_t entry_address;

  if (!nox_eud_read_u32(NOX_EUD_SCRIPT_STRING_COUNT, &count) || index >= count)
    return 0;
  entry_address = (uint64_t)NOX_EUD_SCRIPT_STRING_TABLE + (uint64_t)index * 4u;
  if (entry_address > UINT32_MAX || !nox_eud_range_is_mapped((uint32_t)entry_address, 4))
    return 0;
  pointer = nox_eud_host_read_u32(nox_eud_data_byte((uint32_t)entry_address));
  if (!pointer)
    return 0;
  return (const char *)(uintptr_t)pointer;
}

static float nox_eud_u32_to_float(uint32_t value)
{
  float result;

  memcpy(&result, &value, sizeof(result));
  return result;
}

static int nox_eud_read_float2(uint32_t address, float value[2])
{
  uint32_t first;
  uint32_t second;

  if (!value || !nox_eud_read_u32(address, &first) || !nox_eud_read_u32(address + 4, &second))
    return 0;
  value[0] = nox_eud_u32_to_float(first);
  value[1] = nox_eud_u32_to_float(second);
  return 1;
}

void nox_eud_object_extension_released(int object, uint32_t extension)
{
  unsigned int i;
  nox_eud_pointer_entry *entry;

  if (!object || !extension || !nox_eud_live_alloc_count)
    return;
  for (i = 0; i < NOX_EUD_TOKEN_COUNT; ++i)
  {
    entry = &nox_eud_pointers[i];
    if (entry->kind != NOX_EUD_POINTER_ALLOC ||
        !(entry->flags & NOX_EUD_ALLOC_MAGIC_MISSILE) ||
        entry->host != (unsigned char *)(uintptr_t)extension ||
        entry->owner != (unsigned char *)(uintptr_t)(uint32_t)object)
      continue;

    /* The engine is about to free this extension with free(); invalidate its EUD token first. */
    nox_eud_restore_references_to_alloc(entry);
    memset(entry, 0, sizeof(*entry));
    nox_eud_pointer_quarantined[i] = 1;
    --nox_eud_live_alloc_count;
    return;
  }
}

static int nox_eud_restore_recovery_list(uint32_t head_holder)
{
  nox_eud_pointer_entry *node_entry;
  uint32_t node_offset;
  uint32_t node;
  uint32_t target;
  uint32_t original;
  uint32_t next;
  unsigned int guard;

  if (!nox_eud_read_u32(head_holder, &node))
    return 0;
  for (guard = 0; node && guard < NOX_EUD_TOKEN_COUNT; ++guard)
  {
    if (!nox_eud_pointer_decode(node, 12u, &node_entry, &node_offset) ||
        node_entry->kind != NOX_EUD_POINTER_ALLOC ||
        !nox_eud_read_u32(node, &target) ||
        !nox_eud_read_u32(node + 4u, &original) ||
        !nox_eud_read_u32(node + 8u, &next))
      return 0;
    if (!nox_eud_write_u32(target, original))
      return 0;
    if (next == node)
      return 0;
    node = next;
  }
  return node == 0;
}

static void nox_eud_restore_recovery_chain(void)
{
  uint32_t target = nox_eud_recovery_destructor_target;
  uint32_t head_holder;
  uint32_t previous;
  uint32_t guard_value;
  unsigned int depth;
  nox_eud_recovery_kind kind;

  for (depth = 0; target && target != NOX_EUD_RECOVERY_DESTRUCTOR_ORIGINAL && depth < 8u; ++depth)
  {
    if (!nox_eud_target_is_recovery_helper(target, &kind, &head_holder, &previous))
      break;
    if (kind == NOX_EUD_RECOVERY_MAP_CHANGED)
      nox_eud_restore_recovery_list(head_holder);
    else if (nox_eud_range_is_mapped(NOX_EUD_RECOVERY_NORMAL_GUARD, 4u))
    {
      guard_value = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_RECOVERY_NORMAL_GUARD));
      if (guard_value)
        nox_eud_restore_recovery_list(head_holder);
    }
    if (previous == target)
      break;
    target = previous;
  }
}

void nox_eud_reset(void)
{
  unsigned int i;
  unsigned int event;
  nox_eud_callback_entry *entry;
  nox_eud_pointer_entry *pointer;
  nox_eud_callback_type type;
  uint32_t bit;
  uint32_t offset;
  uint32_t wrapper;
  uint32_t current;
  uintptr_t bypass;

  for (i = 0; i < NOX_EUD_CALLBACK_ENTRY_COUNT; ++i)
  {
    entry = &nox_eud_callbacks[i];

    if (!nox_eud_callback_entry_is_valid(entry))
      continue;
    for (event = 0; event < NOX_EUD_CALLBACK_COUNT; ++event)
    {
      type = (nox_eud_callback_type)event;
      bit = 1u << event;

      if (!(entry->installed_mask & bit))
        continue;
      offset = nox_eud_callback_handler_offset(type);
      wrapper = nox_eud_callback_wrapper_address(type);
      if (offset != UINT32_MAX && wrapper)
      {
        current = nox_eud_host_read_u32(entry->object + offset);
        bypass = (uintptr_t)nox_eud_callback_discard_bypass;
        if (current == wrapper ||
            (type == NOX_EUD_CALLBACK_DISCARD && bypass <= UINT32_MAX && current == (uint32_t)bypass))
          nox_eud_host_write_u32(entry->object + offset, entry->original_handler[event]);
      }
    }
    if (entry->pickup_once_tracked &&
        nox_eud_host_read_u32(entry->object + NOX_EUD_OBJECT_PICKUP_ONCE_CALLBACK_OFFSET) ==
            (uint32_t)entry->pickup_once_callback)
      nox_eud_host_write_u32(entry->object + NOX_EUD_OBJECT_PICKUP_ONCE_CALLBACK_OFFSET, UINT32_MAX);
  }

  /* Replay Panic recovery lists while their EUD-owned nodes are still alive. */
  nox_eud_restore_recovery_chain();

  /* Restore engine fields before any EUD-owned backing allocations are freed. */
  nox_eud_restore_all_references();

  for (i = 0; i < NOX_EUD_TOKEN_COUNT; ++i)
  {
    pointer = &nox_eud_pointers[i];
    if (pointer->kind == NOX_EUD_POINTER_ALLOC && pointer->host)
    {
      nox_eud_detach_alloc(pointer);
      free(pointer->host);
    }
  }

  if (nox_eud_range_is_mapped(NOX_EUD_SMART_MEMORY_HEAD, 4))
  {
    *nox_eud_data_byte(NOX_EUD_SMART_MEMORY_HEAD) = 0;
    *nox_eud_data_byte(NOX_EUD_SMART_MEMORY_HEAD + 1) = 0;
    *nox_eud_data_byte(NOX_EUD_SMART_MEMORY_HEAD + 2) = 0;
    *nox_eud_data_byte(NOX_EUD_SMART_MEMORY_HEAD + 3) = 0;
  }
  memset(nox_eud_callbacks, 0, sizeof(nox_eud_callbacks));
  memset(nox_eud_pointers, 0, sizeof(nox_eud_pointers));
  memset(nox_eud_pointer_quarantined, 0, sizeof(nox_eud_pointer_quarantined));
  nox_eud_pointer_next_slot = 0;
  nox_eud_live_alloc_count = 0;
  nox_eud_smart_destructor_target = 0;
  nox_eud_recovery_destructor_target = 0;
}

int nox_eud_read_u8(uint32_t address, uint8_t *value)
{
  unsigned char byte;

  if (!value)
    return 0;

  if (nox_eud_range_is_mapped(address, 1))
  {
    *value = *nox_eud_data_byte(address);
    return 1;
  }

  if (!nox_eud_dynamic_read_bytes(address, &byte, 1, 0, 0))
    return 0;
  *value = byte;
  return 1;
}

int nox_eud_read_u16(uint32_t address, uint16_t *value)
{
  unsigned char bytes[2];
  uint16_t result;

  if (!value)
    return 0;

  if (nox_eud_range_is_mapped(address, 2))
  {
    result = (uint16_t)*nox_eud_data_byte(address);
    result |= (uint16_t)((uint16_t)*nox_eud_data_byte(address + 1) << 8);
    *value = result;
    return 1;
  }

  if (!nox_eud_dynamic_read_bytes(address, bytes, 2, 0, 0))
    return 0;
  *value = (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8);
  return 1;
}

int nox_eud_read_u32(uint32_t address, uint32_t *value)
{
  unsigned char bytes[4];
  nox_eud_pointer_entry *entry;
  uint32_t offset;
  uint32_t result;

  if (!value)
    return 0;

  if (nox_eud_try_mapped_shadow_read(address, value))
    return 1;

  if (nox_eud_range_is_mapped(address, 4))
  {
    result = (uint32_t)*nox_eud_data_byte(address);
    result |= (uint32_t)*nox_eud_data_byte(address + 1) << 8;
    result |= (uint32_t)*nox_eud_data_byte(address + 2) << 16;
    result |= (uint32_t)*nox_eud_data_byte(address + 3) << 24;
    *value = nox_eud_translate_mapped_pointer(address, result);
    return 1;
  }

  if (nox_eud_try_callback_read(address, value))
    return 1;
  if (nox_eud_try_semantic_pointer_read(address, value))
    return 1;

  if (!nox_eud_dynamic_read_bytes(address, bytes, 4, &entry, &offset))
    return 0;

  result = (uint32_t)bytes[0];
  result |= (uint32_t)bytes[1] << 8;
  result |= (uint32_t)bytes[2] << 16;
  result |= (uint32_t)bytes[3] << 24;
  *value = nox_eud_translate_dynamic_pointer(entry, offset, result);
  return 1;
}

int nox_eud_write_u8(uint32_t address, uint8_t value)
{
  unsigned char byte = value;

  if (nox_eud_range_overlaps_protected_slot(address, 1))
    return 1;
  if (nox_eud_range_is_mapped(address, 1))
  {
    *nox_eud_data_byte(address) = value;
    return 1;
  }

  return nox_eud_dynamic_write_bytes(address, &byte, 1);
}

int nox_eud_write_u16(uint32_t address, uint16_t value)
{
  unsigned char bytes[2];

  if (nox_eud_range_overlaps_protected_slot(address, 2))
    return 1;
  if (nox_eud_range_is_mapped(address, 2))
  {
    *nox_eud_data_byte(address) = (unsigned char)value;
    *nox_eud_data_byte(address + 1) = (unsigned char)(value >> 8);
    return 1;
  }

  bytes[0] = (unsigned char)value;
  bytes[1] = (unsigned char)(value >> 8);
  return nox_eud_dynamic_write_bytes(address, bytes, 2);
}

int nox_eud_write_u32(uint32_t address, uint32_t value)
{
  unsigned char bytes[4];

  if (nox_eud_try_protected_mapped_write(address, value))
    return 1;
  if (nox_eud_range_overlaps_protected_slot(address, 4))
    return 1;
  if (nox_eud_try_mapped_pointer_write(address, value))
    return 1;

  if (nox_eud_range_is_mapped(address, 4))
  {
    *nox_eud_data_byte(address) = (unsigned char)value;
    *nox_eud_data_byte(address + 1) = (unsigned char)(value >> 8);
    *nox_eud_data_byte(address + 2) = (unsigned char)(value >> 16);
    *nox_eud_data_byte(address + 3) = (unsigned char)(value >> 24);
    return 1;
  }

  if (nox_eud_try_callback_write(address, value))
    return 1;
  if (nox_eud_try_semantic_pointer_write(address, value))
    return 1;

  bytes[0] = (unsigned char)value;
  bytes[1] = (unsigned char)(value >> 8);
  bytes[2] = (unsigned char)(value >> 16);
  bytes[3] = (unsigned char)(value >> 24);
  return nox_eud_dynamic_write_bytes(address, bytes, 4);
}

int nox_eud_dispatch_builtin(int builtin_id, uint32_t target, int *result)
{
  const char *name;
  _DWORD *new_object;
  int equip_mode;
  int object;
  int object2;
  int wall;
  int *action;
  uint32_t address;
  uint32_t argument;
  uint32_t argument2;
  uint32_t argument3;
  uint32_t argument4;
  uint32_t argument5;
  uint32_t dword_value;
  uint32_t pointer;
  uint32_t token;
  uint32_t unit_ext;
  uint32_t x_bits;
  uint32_t y_bits;
  uint32_t copy_index;
  uint64_t source_address;
  uint64_t destination_address;
  uint8_t byte_value;
  uint16_t word_value;
  float xy[2];

  if (!result)
    return 0;

  if (builtin_id == NOX_EUD_BIND_BUILTIN && nox_eud_target_is_bind_helper(target))
  {
    nox_eud_pointer_entry *function_table;
    uint32_t function_offset;
    uint32_t args;
    uint32_t function_record;
    uint32_t arg_value;
    uint32_t function_count;
    uint32_t caller;
    uint32_t trigger;
    int function_id;
    unsigned int arg_count;
    unsigned int i;

    args = (uint32_t)script_pop();
    function_id = script_pop();
    function_record = (uint32_t)script_pop();
    function_count = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_SCRIPT_FUNCTION_COUNT));
    if (function_id < 0 || (uint32_t)function_id >= function_count ||
        !nox_eud_pointer_decode(function_record, 12u, &function_table, &function_offset) ||
        function_table->kind != NOX_EUD_POINTER_SCRIPT_TABLE ||
        function_offset != (uint32_t)function_id * NOX_EUD_SCRIPT_RECORD_SIZE)
    {
      *result = 0;
      return 1;
    }
    arg_count = function_table->host[function_offset + 8u];
    for (i = arg_count; i > 0; --i)
    {
      if (!nox_eud_read_u32(args + 4u * (i - 1u), &arg_value))
      {
        *result = 0;
        return 1;
      }
      sub_507230((int)arg_value);
    }
    caller = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_SCRIPT_CALLER));
    trigger = nox_eud_host_read_u32(nox_eud_data_byte(NOX_EUD_SCRIPT_TRIGGER));
    *result = sub_507310(function_id, (int)caller, (int)trigger);
    return 1;
  }

  if (builtin_id == NOX_EUD_MEM_ALLOC_BUILTIN && nox_eud_target_is_mem_alloc_helper(target))
  {
    argument = (uint32_t)script_pop();
    token = nox_eud_alloc(argument);
    sub_507230((int)token);
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_INVOKE_RAW_BUILTIN && nox_eud_target_is_mem_free_helper(target))
  {
    argument = (uint32_t)script_pop();
    nox_eud_free_alloc(argument);
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_INVOKE_RAW_BUILTIN && nox_eud_target_is_dword_copy_helper(target))
  {
    address = (uint32_t)script_pop();
    if (nox_eud_read_u32(address, &argument) && nox_eud_read_u32(address + 4, &argument2) &&
        nox_eud_read_u32(address + 8, &argument3) && argument3 <= 0x00100000u)
    {
      for (copy_index = 0; copy_index < argument3; ++copy_index)
      {
        source_address = (uint64_t)argument + (uint64_t)copy_index * 4u;
        destination_address = (uint64_t)argument2 + (uint64_t)copy_index * 4u;
        if (source_address > UINT32_MAX || destination_address > UINT32_MAX ||
            !nox_eud_read_u32((uint32_t)source_address, &dword_value) ||
            !nox_eud_write_u32((uint32_t)destination_address, dword_value))
          break;
      }
    }
    *result = 0;
    return 1;
  }

  /*
   * EUD map sources generated by Panic's toolchain expose SetMemory through
   * Unused59 (builtin 0x59) and GetMemory through Unknownb9 (builtin 0xB9).
   * Claim the call only when the address is a mapped legacy data address or
   * one of the compatibility pointer tokens introduced by Tier 2.
   */
  if (builtin_id == NOX_EUD_SET_DWORD_BUILTIN)
  {
    argument = (uint32_t)script_pop();
    address = (uint32_t)script_pop();
    if (!nox_eud_write_u32(address, argument))
    {
      if (nox_eud_address_is_token(address))
      {
        *result = 0;
        return 1;
      }
      sub_507230((int)address);
      sub_507230((int)argument);
      return 0;
    }
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_GET_DWORD_BUILTIN)
  {
    address = (uint32_t)script_pop();
    if (!nox_eud_read_u32(address, &dword_value))
    {
      if (nox_eud_address_is_token(address))
      {
        sub_507230(0);
        *result = 0;
        return 1;
      }
      sub_507230((int)address);
      return 0;
    }
    sub_507230((int)dword_value);
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_SPELL_LOOKUP_BUILTIN && nox_eud_target_is_spell_lookup_helper(target))
  {
    argument = (uint32_t)script_pop();
    name = nox_eud_script_string(argument);
    sub_507230(name ? sub_4243F0(name) : 0);
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_CREATE_OBJECT_AT_BUILTIN && nox_eud_target_is_create_object_at_helper(target))
  {
    y_bits = (uint32_t)script_pop();
    x_bits = (uint32_t)script_pop();
    argument = (uint32_t)script_pop();
    name = nox_eud_script_string(argument);
    new_object = name ? sub_4E3810((CHAR *)name) : 0;
    if (new_object)
      sub_4DAA50((int)new_object, 0, nox_eud_u32_to_float(x_bits), nox_eud_u32_to_float(y_bits));
    pointer = (uint32_t)(uintptr_t)new_object;
    token = pointer ? nox_eud_register_object_pointer(pointer) : 0;
    sub_507230((int)token);
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_MONSTER_ACTION_PUSH_BUILTIN &&
      nox_eud_target_is_monster_action_push_helper(target))
  {
    address = (uint32_t)script_pop();
    token = 0;
    if (nox_eud_read_u32(address, &argument) && nox_eud_read_u32(address + 4, &argument2) &&
        nox_eud_resolve_object(argument, &object))
    {
      action = sub_50A260(object, (int)argument2);
      if (action)
      {
        unit_ext = nox_eud_host_read_u32((unsigned char *)(uintptr_t)(uint32_t)object +
                                         NOX_EUD_OBJECT_PLAYER_EXT_OFFSET);
        token = nox_eud_register_monster_action(action,
                                                (int)nox_eud_host_read_u32(
                                                    (unsigned char *)(uintptr_t)(uint32_t)object +
                                                    NOX_EUD_OBJECT_SCRIPT_ID_OFFSET),
                                                (unsigned char *)(uintptr_t)unit_ext);
      }
    }
    sub_507230((int)token);
    *result = 0;
    return 1;
  }

  /*
   * Panic's UnitToPtr bootstrap points builtin 0xB8 at a known 10-DWORD x86
   * helper stored in the current script's locals buffer. Tier 2 represents
   * that buffer with a compatibility token and verifies the helper signature
   * before reproducing it semantically, so arbitrary local buffers are never
   * treated as executable code.
   */
  if (builtin_id == NOX_EUD_UNIT_TO_PTR_BUILTIN && nox_eud_target_is_unit_to_ptr_helper(target))
  {
    argument = (uint32_t)script_pop();
    pointer = (uint32_t)sub_511B60((int)argument);
    if (pointer)
      pointer = nox_eud_register_object_pointer(pointer);
    sub_507230((int)pointer);
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_UNIT_TO_PTR_BUILTIN && nox_eud_target_is_remove_sneak_helper(target))
  {
    argument = (uint32_t)script_pop();
    if (nox_eud_resolve_object(argument, &object))
      sub_4FC300((_DWORD *)(uintptr_t)(uint32_t)object, 4);
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_UNIT_TO_PTR_BUILTIN && nox_eud_target_is_xtra_object_spec_helper(target))
  {
    argument = (uint32_t)script_pop();
    new_object = sub_4E3450((int)argument);
    pointer = (uint32_t)(uintptr_t)new_object;
    token = pointer ? nox_eud_register_object_pointer(pointer) : 0;
    sub_507230((int)token);
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_SPELL_LOOKUP_BUILTIN && nox_eud_target_is_wall_lookup_helper(target))
  {
    address = (uint32_t)script_pop();
    token = 0;
    if (nox_eud_read_u32(address, &argument) && nox_eud_read_u32(address + 4, &argument2))
    {
      pointer = (uint32_t)sub_410580((int)argument, (int)argument2);
      token = pointer ? nox_eud_register_wall(pointer) : 0;
    }
    sub_507230((int)token);
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_INVOKE_RAW_BUILTIN && nox_eud_target_is_create_magic_wall_helper(target))
  {
    address = (uint32_t)script_pop();
    if (nox_eud_read_u32(address, &argument) && nox_eud_read_u32(address + 4, &argument2) &&
        nox_eud_read_u32(address + 8, &argument3) && nox_eud_read_u32(address + 12, &argument4))
      sub_4FFD00((int)argument, (int)argument2, (int)argument3, (unsigned char)argument4);
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_INVOKE_RAW_BUILTIN && nox_eud_target_is_add_breakable_wall_helper(target))
  {
    address = (uint32_t)script_pop();
    if (nox_eud_read_u32(address, &argument) && nox_eud_resolve_wall(argument, &wall))
      sub_410840(wall);
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_INVOKE_RAW_BUILTIN && nox_eud_target_is_net_load_fx_helper(target))
  {
    address = (uint32_t)script_pop();
    if (nox_eud_read_u32(address, &argument) && nox_eud_read_u32(address + 4, &argument2) &&
        nox_eud_read_u32(address + 8, &argument3) && nox_eud_read_float2(argument3, xy))
      sub_523150((char)argument, (char)argument2, xy);
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_INVOKE_RAW_BUILTIN && nox_eud_target_is_summon_fx_helper(target))
  {
    address = (uint32_t)script_pop();
    if (nox_eud_read_u32(address, &argument) && nox_eud_read_u32(address + 4, &argument2) &&
        nox_eud_read_u32(address + 8, &argument3) && nox_eud_read_u32(address + 12, &argument4) &&
        nox_eud_read_u32(address + 16, &argument5) && nox_eud_read_float2(argument4, xy))
      sub_5236F0((__int16)argument5, xy, (char)argument3, (__int16)argument2, (__int16)argument);
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_NPC_EQUIPMENT_BUILTIN &&
      nox_eud_target_is_npc_equipment_helper(target, &equip_mode))
  {
    argument = (uint32_t)script_pop();
    argument2 = (uint32_t)script_pop();
    if (nox_eud_resolve_object(argument, &object2) && nox_eud_resolve_object(argument2, &object))
    {
      if (equip_mode)
        sub_4F2F70((_DWORD *)(uintptr_t)(uint32_t)object, object2);
      else
        sub_4F2FB0((_DWORD *)(uintptr_t)(uint32_t)object, object2);
    }
    *result = 0;
    return 1;
  }

  if (builtin_id == NOX_EUD_PLAY_SOUND_BUILTIN && nox_eud_target_is_play_sound_helper(target))
  {
    argument = (uint32_t)script_pop();
    argument2 = (uint32_t)script_pop();
    if (nox_eud_resolve_object(argument2, &object))
      sub_501960((int)argument, object, 0, 0);
    *result = 0;
    return 1;
  }

  switch (target)
  {
    case NOX_EUD_SET_BYTE_TARGET:
      argument = (uint32_t)script_pop();
      address = (uint32_t)script_pop();
      *result = nox_eud_write_u8(address, (uint8_t)argument) ? 0 : 1;
      return 1;

    case NOX_EUD_GET_BYTE_TARGET:
      address = (uint32_t)script_pop();
      if (!nox_eud_read_u8(address, &byte_value))
      {
        *result = 1;
        return 1;
      }
      sub_507230((int)byte_value);
      *result = 0;
      return 1;

    case NOX_EUD_SET_WORD_TARGET:
      argument = (uint32_t)script_pop();
      address = (uint32_t)script_pop();
      *result = nox_eud_write_u16(address, (uint16_t)argument) ? 0 : 1;
      return 1;

    case NOX_EUD_GET_WORD_TARGET:
      address = (uint32_t)script_pop();
      if (!nox_eud_read_u16(address, &word_value))
      {
        *result = 1;
        return 1;
      }
      sub_507230((int)word_value);
      *result = 0;
      return 1;

    default:
      break;
  }

  /* Never let virtual compatibility memory or legacy data bytes become executable host code. */
  if (nox_eud_address_is_token(target) || nox_eud_range_is_mapped(target, 1))
  {
    *result = 0;
    return 1;
  }
  return 0;
}
