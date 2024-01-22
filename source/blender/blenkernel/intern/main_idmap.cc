/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_utildefines.h"

#include "DNA_ID.h"

#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_main_idmap.hh" /* own include */

/** \file
 * \ingroup bke
 *
 * Utility functions for faster ID lookups.
 */

/* -------------------------------------------------------------------- */
/** \name BKE_main_idmap API
 *
 * Cache ID (name, library lookups).
 * This doesn't account for adding/removing data-blocks,
 * and should only be used when performing many lookups.
 *
 * \note GHash's are initialized on demand,
 * since its likely some types will never have lookups run on them,
 * so its a waste to create and never use.
 * \{ */

struct IDNameLib_Key {
  /** `ID.name + 2`: without the ID type prefix, since each id type gets its own 'map'. */
  const char *name;
  /** `ID.lib`: */
  const Library *lib;
};

struct IDNameLib_TypeMap {
  GHash *map;
  short id_type;
};

/**
 * Opaque structure, external API users only see this.
 */
struct IDNameLib_Map {
  IDNameLib_TypeMap type_maps[INDEX_ID_MAX];
  GHash *uid_map;
  Main *bmain;
  GSet *valid_id_pointers;
  int idmap_types;

  /* For storage of keys for the #TypeMap #GHash, avoids many single allocations. */
  BLI_mempool *type_maps_keys_pool;
};

static IDNameLib_TypeMap *main_idmap_from_idcode(IDNameLib_Map *id_map, short id_type)
{
  if (id_map->idmap_types & MAIN_IDMAP_TYPE_NAME) {
    for (int i = 0; i < INDEX_ID_MAX; i++) {
      if (id_map->type_maps[i].id_type == id_type) {
        return &id_map->type_maps[i];
      }
    }
  }
  return nullptr;
}

IDNameLib_Map *BKE_main_idmap_create(Main *bmain,
                                     const bool create_valid_ids_set,
                                     Main *old_bmain,
                                     const int idmap_types)
{
  IDNameLib_Map *id_map = static_cast<IDNameLib_Map *>(MEM_mallocN(sizeof(*id_map), __func__));
  id_map->bmain = bmain;
  id_map->idmap_types = idmap_types;

  int index = 0;
  while (index < INDEX_ID_MAX) {
    IDNameLib_TypeMap *type_map = &id_map->type_maps[index];
    type_map->map = nullptr;
    type_map->id_type = BKE_idtype_idcode_iter_step(&index);
    BLI_assert(type_map->id_type != 0);
  }
  BLI_assert(index == INDEX_ID_MAX);
  id_map->type_maps_keys_pool = nullptr;

  if (idmap_types & MAIN_IDMAP_TYPE_UID) {
    ID *id;
    id_map->uid_map = BLI_ghash_int_new(__func__);
    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      BLI_assert(id->session_uid != MAIN_ID_SESSION_UID_UNSET);
      void **id_ptr_v;
      const bool existing_key = BLI_ghash_ensure_p(
          id_map->uid_map, POINTER_FROM_UINT(id->session_uid), &id_ptr_v);
      BLI_assert(existing_key == false);
      UNUSED_VARS_NDEBUG(existing_key);

      *id_ptr_v = id;
    }
    FOREACH_MAIN_ID_END;
  }
  else {
    id_map->uid_map = nullptr;
  }

  if (create_valid_ids_set) {
    id_map->valid_id_pointers = BKE_main_gset_create(bmain, nullptr);
    if (old_bmain != nullptr) {
      id_map->valid_id_pointers = BKE_main_gset_create(old_bmain, id_map->valid_id_pointers);
    }
  }
  else {
    id_map->valid_id_pointers = nullptr;
  }

  return id_map;
}

void BKE_main_idmap_insert_id(IDNameLib_Map *id_map, ID *id)
{
  if (id_map->idmap_types & MAIN_IDMAP_TYPE_NAME) {
    const short id_type = GS(id->name);
    IDNameLib_TypeMap *type_map = main_idmap_from_idcode(id_map, id_type);

    /* No need to do anything if map has not been lazily created yet. */
    if (LIKELY(type_map != nullptr) && type_map->map != nullptr) {
      BLI_assert(id_map->type_maps_keys_pool != nullptr);

      IDNameLib_Key *key = static_cast<IDNameLib_Key *>(
          BLI_mempool_alloc(id_map->type_maps_keys_pool));
      key->name = id->name + 2;
      key->lib = id->lib;
      BLI_ghash_insert(type_map->map, key, id);
    }
  }

  if (id_map->idmap_types & MAIN_IDMAP_TYPE_UID) {
    BLI_assert(id_map->uid_map != nullptr);
    BLI_assert(id->session_uid != MAIN_ID_SESSION_UID_UNSET);
    void **id_ptr_v;
    const bool existing_key = BLI_ghash_ensure_p(
        id_map->uid_map, POINTER_FROM_UINT(id->session_uid), &id_ptr_v);
    BLI_assert(existing_key == false);
    UNUSED_VARS_NDEBUG(existing_key);

    *id_ptr_v = id;
  }
}

void BKE_main_idmap_remove_id(IDNameLib_Map *id_map, ID *id)
{
  if (id_map->idmap_types & MAIN_IDMAP_TYPE_NAME) {
    const short id_type = GS(id->name);
    IDNameLib_TypeMap *type_map = main_idmap_from_idcode(id_map, id_type);

    /* No need to do anything if map has not been lazily created yet. */
    if (LIKELY(type_map != nullptr) && type_map->map != nullptr) {
      BLI_assert(id_map->type_maps_keys_pool != nullptr);

      /* NOTE: We cannot free the key from the MemPool here, would need new API from GHash to also
       * retrieve key pointer. Not a big deal for now */
      IDNameLib_Key key{id->name + 2, id->lib};
      BLI_ghash_remove(type_map->map, &key, nullptr, nullptr);
    }
  }

  if (id_map->idmap_types & MAIN_IDMAP_TYPE_UID) {
    BLI_assert(id_map->uid_map != nullptr);
    BLI_assert(id->session_uid != MAIN_ID_SESSION_UID_UNSET);

    BLI_ghash_remove(id_map->uid_map, POINTER_FROM_UINT(id->session_uid), nullptr, nullptr);
  }
}

Main *BKE_main_idmap_main_get(IDNameLib_Map *id_map)
{
  return id_map->bmain;
}

static uint idkey_hash(const void *ptr)
{
  const IDNameLib_Key *idkey = static_cast<const IDNameLib_Key *>(ptr);
  uint key = BLI_ghashutil_strhash(idkey->name);
  if (idkey->lib) {
    key ^= BLI_ghashutil_ptrhash(idkey->lib);
  }
  return key;
}

static bool idkey_cmp(const void *a, const void *b)
{
  const IDNameLib_Key *idkey_a = static_cast<const IDNameLib_Key *>(a);
  const IDNameLib_Key *idkey_b = static_cast<const IDNameLib_Key *>(b);
  return !STREQ(idkey_a->name, idkey_b->name) || (idkey_a->lib != idkey_b->lib);
}

ID *BKE_main_idmap_lookup_name(IDNameLib_Map *id_map,
                               short id_type,
                               const char *name,
                               const Library *lib)
{
  IDNameLib_TypeMap *type_map = main_idmap_from_idcode(id_map, id_type);

  if (UNLIKELY(type_map == nullptr)) {
    return nullptr;
  }

  /* Lazy init. */
  if (type_map->map == nullptr) {
    if (id_map->type_maps_keys_pool == nullptr) {
      id_map->type_maps_keys_pool = BLI_mempool_create(
          sizeof(IDNameLib_Key), 1024, 1024, BLI_MEMPOOL_NOP);
    }

    GHash *map = type_map->map = BLI_ghash_new(idkey_hash, idkey_cmp, __func__);
    ListBase *lb = which_libbase(id_map->bmain, id_type);
    for (ID *id = static_cast<ID *>(lb->first); id; id = static_cast<ID *>(id->next)) {
      IDNameLib_Key *key = static_cast<IDNameLib_Key *>(
          BLI_mempool_alloc(id_map->type_maps_keys_pool));
      key->name = id->name + 2;
      key->lib = id->lib;
      BLI_ghash_insert(map, key, id);
    }
  }

  const IDNameLib_Key key_lookup = {name, lib};
  return static_cast<ID *>(BLI_ghash_lookup(type_map->map, &key_lookup));
}

ID *BKE_main_idmap_lookup_id(IDNameLib_Map *id_map, const ID *id)
{
  /* When used during undo/redo, this function cannot assume that given id points to valid memory
   * (i.e. has not been freed),
   * so it has to check that it does exist in 'old' (aka current) Main database.
   * Otherwise, we cannot provide new ID pointer that way (would crash accessing freed memory
   * when trying to get ID name).
   */
  if (id_map->valid_id_pointers == nullptr || BLI_gset_haskey(id_map->valid_id_pointers, id)) {
    return BKE_main_idmap_lookup_name(id_map, GS(id->name), id->name + 2, id->lib);
  }
  return nullptr;
}

ID *BKE_main_idmap_lookup_uid(IDNameLib_Map *id_map, const uint session_uid)
{
  if (id_map->idmap_types & MAIN_IDMAP_TYPE_UID) {
    return static_cast<ID *>(BLI_ghash_lookup(id_map->uid_map, POINTER_FROM_UINT(session_uid)));
  }
  return nullptr;
}

void BKE_main_idmap_destroy(IDNameLib_Map *id_map)
{
  if (id_map->idmap_types & MAIN_IDMAP_TYPE_NAME) {
    IDNameLib_TypeMap *type_map = id_map->type_maps;
    for (int i = 0; i < INDEX_ID_MAX; i++, type_map++) {
      if (type_map->map) {
        BLI_ghash_free(type_map->map, nullptr, nullptr);
        type_map->map = nullptr;
      }
    }
    if (id_map->type_maps_keys_pool != nullptr) {
      BLI_mempool_destroy(id_map->type_maps_keys_pool);
      id_map->type_maps_keys_pool = nullptr;
    }
  }
  if (id_map->idmap_types & MAIN_IDMAP_TYPE_UID) {
    BLI_ghash_free(id_map->uid_map, nullptr, nullptr);
  }

  BLI_assert(id_map->type_maps_keys_pool == nullptr);

  if (id_map->valid_id_pointers != nullptr) {
    BLI_gset_free(id_map->valid_id_pointers, nullptr);
  }

  MEM_freeN(id_map);
}

/** \} */
