/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_main_namemap.h"

#include "BLI_assert.h"
#include "BLI_bitmap.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_base.hh"
#include "BLI_set.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"

#include "DNA_ID.h"

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.main_namemap"};

//#define DEBUG_PRINT_MEMORY_USAGE

using namespace blender;

/* Assumes and ensure that the suffix number can never go beyond 1 billion. */
#define MAX_NUMBER 1000000000
/* We do not want to get "name.000", so minimal number is 1. */
#define MIN_NUMBER 1

/**
 * Helper building final ID name from given base_name and number.
 *
 * If everything goes well and we do generate a valid final ID name in given name, we return
 * true. In case the final name would overflow the allowed ID name length, or given number is
 * bigger than maximum allowed value, we truncate further the base_name (and given name, which is
 * assumed to have the same 'base_name' part), and return false.
 */
static bool id_name_final_build(char *name, char *base_name, size_t base_name_len, int number)
{
  char number_str[11]; /* Dot + nine digits + null terminator. */
  size_t number_str_len = SNPRINTF_RLEN(number_str, ".%.3d", number);

  /* If the number would lead to an overflow of the maximum ID name length, we need to truncate
   * the base name part and do all the number checks again. */
  if (base_name_len + number_str_len >= MAX_NAME || number >= MAX_NUMBER) {
    if (base_name_len + number_str_len >= MAX_NAME) {
      base_name_len = MAX_NAME - number_str_len - 1;
    }
    else {
      base_name_len--;
    }
    base_name[base_name_len] = '\0';

    /* Code above may have generated invalid utf-8 string, due to raw truncation.
     * Ensure we get a valid one now. */
    base_name_len -= size_t(BLI_str_utf8_invalid_strip(base_name, base_name_len));

    /* Also truncate orig name, and start the whole check again. */
    name[base_name_len] = '\0';
    return false;
  }

  /* We have our final number, we can put it in name and exit the function. */
  BLI_strncpy(name + base_name_len, number_str, number_str_len + 1);
  return true;
}

/* Key used in set/map lookups: just a string name. */
struct UniqueName_Key {
  char name[MAX_NAME];
  uint64_t hash() const
  {
    return BLI_ghashutil_strhash_n(name, MAX_NAME);
  }
  bool operator==(const UniqueName_Key &o) const
  {
    return !BLI_ghashutil_strcmp(name, o.name);
  }
};

/* Tracking of used numeric suffixes. For each base name:
 *
 * - Exactly track which of the lowest 1024 suffixes are in use,
 *   whenever there is a name collision we pick the lowest "unused"
 *   one. This is done with a bit map.
 * - Above 1024, do not track them exactly, just track the maximum
 *   suffix value seen so far. Upon collision, assign number that is
 *   one larger.
 */
struct UniqueName_Value {
  static constexpr uint max_exact_tracking = 1024;
  BLI_BITMAP_DECLARE(mask, max_exact_tracking);
  int max_value = 0;

  void mark_used(int number)
  {
    if (number >= 0 && number < max_exact_tracking) {
      BLI_BITMAP_ENABLE(mask, number);
    }
    if (number < MAX_NUMBER) {
      math::max_inplace(max_value, number);
    }
  }

  void mark_unused(int number)
  {
    if (number >= 0 && number < max_exact_tracking) {
      BLI_BITMAP_DISABLE(mask, number);
    }
    if (number > 0 && number == max_value) {
      --max_value;
    }
  }

  bool use_if_unused(int number)
  {
    if (number >= 0 && number < max_exact_tracking) {
      if (!BLI_BITMAP_TEST_BOOL(mask, number)) {
        BLI_BITMAP_ENABLE(mask, number);
        math::max_inplace(max_value, number);
        return true;
      }
    }
    return false;
  }

  int use_smallest_unused()
  {
    /* Find the smallest available one <1k.
     * However we never want to pick zero ("none") suffix, even if it is
     * available, e.g. if Foo.001 was used and we want to create another
     * Foo.001, we should return Foo.002 and not Foo.
     * So while searching, mark #0 as "used" to make sure we don't find it,
     * and restore the value afterwards. */

    BLI_bitmap prev_first = mask[0];
    mask[0] |= 1;
    int result = BLI_bitmap_find_first_unset(mask, max_exact_tracking);
    if (result >= 0) {
      BLI_BITMAP_ENABLE(mask, result);
      math::max_inplace(max_value, result);
    }
    mask[0] |= prev_first & 1; /* Restore previous value of #0 bit. */
    return result;
  }
};

/* Tracking of names for a single ID type. */
struct UniqueName_TypeMap {
  /* Set of full names that are in use. */
  Set<UniqueName_Key> full_names;
  /* For each base name (i.e. without numeric suffix), track the
   * numeric suffixes that are in use. */
  Map<UniqueName_Key, UniqueName_Value> base_name_to_num_suffix;
};

struct UniqueName_Map {
  UniqueName_TypeMap type_maps[INDEX_ID_MAX];

  UniqueName_TypeMap *find_by_type(short id_type)
  {
    int index = BKE_idtype_idcode_to_index(id_type);
    return index >= 0 ? &type_maps[index] : nullptr;
  }
};

UniqueName_Map *BKE_main_namemap_create()
{
  UniqueName_Map *map = MEM_new<UniqueName_Map>(__func__);
  return map;
}

void BKE_main_namemap_destroy(UniqueName_Map **r_name_map)
{
#ifdef DEBUG_PRINT_MEMORY_USAGE
  int64_t size_sets = 0;
  int64_t size_maps = 0;
  for (const UniqueName_TypeMap &type_map : (*r_name_map)->type_maps) {
    size_sets += type_map.full_names.size_in_bytes();
    size_maps += type_map.base_name_to_num_suffix.size_in_bytes();
  }
  printf(
      "NameMap memory usage: sets %.1fKB, maps %.1fKB\n", size_sets / 1024.0, size_maps / 1024.0);
#endif
  MEM_delete<UniqueName_Map>(*r_name_map);
  *r_name_map = nullptr;
}

void BKE_main_namemap_clear(Main *bmain)
{
  for (Main *bmain_iter = bmain; bmain_iter != nullptr; bmain_iter = bmain_iter->next) {
    if (bmain_iter->name_map != nullptr) {
      BKE_main_namemap_destroy(&bmain_iter->name_map);
    }
    if (bmain_iter->name_map_global != nullptr) {
      BKE_main_namemap_destroy(&bmain_iter->name_map_global);
    }
    for (Library *lib_iter = static_cast<Library *>(bmain_iter->libraries.first);
         lib_iter != nullptr;
         lib_iter = static_cast<Library *>(lib_iter->id.next))
    {
      if (lib_iter->runtime.name_map != nullptr) {
        BKE_main_namemap_destroy(&lib_iter->runtime.name_map);
      }
    }
  }
}

/* `do_global` will generate a namemap for all IDs in current Main, regardless of their library.
 * Note that duplicates (e.g.local ID and linked ID with same name) will only generate a single
 * entry in the map then. */
static void main_namemap_populate(
    UniqueName_Map *name_map, Main *bmain, Library *library, ID *ignore_id, const bool do_global)
{
  BLI_assert_msg(name_map != nullptr, "name_map should not be null");
  for (UniqueName_TypeMap &type_map : name_map->type_maps) {
    type_map.base_name_to_num_suffix.clear();
  }
  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if ((id == ignore_id) || (!do_global && (id->lib != library))) {
      continue;
    }
    UniqueName_TypeMap *type_map = name_map->find_by_type(GS(id->name));
    BLI_assert(type_map != nullptr);

    /* Insert the full name into the set. */
    UniqueName_Key key;
    STRNCPY(key.name, id->name + 2);
    if (!type_map->full_names.add(key)) {
      /* Do not assert, this code is also used by #BKE_main_namemap_validate_and_fix, where
       * duplicates are expected. */
#if 0
      BLI_assert_msg(do_global,
                     "The key (name) already exists in the namemap, should only happen when "
                     "`do_global` is true.");
#endif
      continue;
    }

    /* Get the name and number parts ("name.number"). */
    int number = MIN_NUMBER;
    BLI_string_split_name_number(id->name + 2, '.', key.name, &number);

    /* Get and update the entry for this base name. */
    UniqueName_Value &val = type_map->base_name_to_num_suffix.lookup_or_add_default(key);
    val.mark_used(number);
  }
  FOREACH_MAIN_ID_END;
}

/* Get the name map object used for the given Main/ID.
 * Lazily creates and populates the contents of the name map, if ensure_created is true.
 * NOTE: if the contents are populated, the name of the given ID itself is not added. */
static UniqueName_Map *get_namemap_for(Main *bmain,
                                       ID *id,
                                       const bool ensure_created,
                                       const bool do_global)
{
  if (do_global) {
    if (ensure_created && bmain->name_map_global == nullptr) {
      bmain->name_map_global = BKE_main_namemap_create();
      main_namemap_populate(bmain->name_map_global, bmain, id->lib, id, true);
    }
    return bmain->name_map_global;
  }

  if (id->lib != nullptr) {
    if (ensure_created && id->lib->runtime.name_map == nullptr) {
      id->lib->runtime.name_map = BKE_main_namemap_create();
      main_namemap_populate(id->lib->runtime.name_map, bmain, id->lib, id, false);
    }
    return id->lib->runtime.name_map;
  }
  if (ensure_created && bmain->name_map == nullptr) {
    bmain->name_map = BKE_main_namemap_create();
    main_namemap_populate(bmain->name_map, bmain, id->lib, id, false);
  }
  return bmain->name_map;
}

/* Tries to add given name to the given name_map, returns `true` if added, `false` if it was
 * already in the namemap. */
static bool namemap_add_name(UniqueName_Map *name_map, ID *id, const char *name, const int number)
{
  BLI_assert(strlen(name) < MAX_NAME);
  UniqueName_TypeMap *type_map = name_map->find_by_type(GS(id->name));
  BLI_assert(type_map != nullptr);

  UniqueName_Key key;
  /* Remove full name from the set. */
  STRNCPY(key.name, name);
  if (!type_map->full_names.add(key)) {
    /* Name already in this namemap, nothing else to do. */
    return false;
  }

  UniqueName_Value &val = type_map->base_name_to_num_suffix.lookup_or_add(key, {});
  val.mark_used(number);
  return true;
}

bool BKE_main_namemap_get_name(Main *bmain, ID *id, char *name, const bool do_unique_in_bmain)
{
#ifndef __GNUC__ /* GCC warns with `nonull-compare`. */
  BLI_assert(bmain != nullptr);
  BLI_assert(id != nullptr);
#endif
  UniqueName_Map *name_map = get_namemap_for(bmain, id, true, do_unique_in_bmain);
  UniqueName_Map *name_map_other = get_namemap_for(bmain, id, false, !do_unique_in_bmain);
  BLI_assert(name_map != nullptr);
  BLI_assert(strlen(name) < MAX_NAME);
  UniqueName_TypeMap *type_map = name_map->find_by_type(GS(id->name));
  BLI_assert(type_map != nullptr);

  bool is_name_changed = false;

  UniqueName_Key key;
  while (true) {
    /* Check if the full original name has a duplicate. */
    STRNCPY(key.name, name);
    const bool has_dup = type_map->full_names.contains(key);

    /* Get the name and number parts ("name.number"). */
    int number = MIN_NUMBER;
    size_t base_name_len = BLI_string_split_name_number(name, '.', key.name, &number);

    bool added_new = false;
    UniqueName_Value &val = type_map->base_name_to_num_suffix.lookup_or_add_cb(key, [&]() {
      added_new = true;
      return UniqueName_Value();
    });
    if (added_new || !has_dup) {
      /* This base name is not used at all yet, or the full original
       * name has no duplicates. The latter could happen if splitting
       * by number would produce the same values, for different name
       * strings (e.g. Foo.001 and Foo.1). */
      val.mark_used(number);

      if (!has_dup) {
        STRNCPY(key.name, name);
        type_map->full_names.add(key);
      }
      if (name_map_other != nullptr) {
        namemap_add_name(name_map_other, id, name, number);
      }
      return is_name_changed;
    }

    /* The base name is already used. But our number suffix might not be used yet. */
    int number_to_use = -1;
    if (val.use_if_unused(number)) {
      /* Our particular number suffix is not used yet: use it. */
      number_to_use = number;
    }
    else {
      /* Find lowest free under 1k and use it. */
      number_to_use = val.use_smallest_unused();

      /* Did not find one under 1k. */
      if (number_to_use == -1) {
        if (number >= MIN_NUMBER && number > val.max_value) {
          val.max_value = number;
          number_to_use = number;
        }
        else {
          val.max_value++;
          number_to_use = val.max_value;
        }
      }
    }

    /* Try to build final name from the current base name and the number.
     * Note that this can fail due to too long base name, or a too large number,
     * in which case it will shorten the base name, and we'll start again. */
    BLI_assert(number_to_use >= MIN_NUMBER);
    if (id_name_final_build(name, key.name, base_name_len, number_to_use)) {
      /* All good, add final name to the set. */
      STRNCPY(key.name, name);
      type_map->full_names.add(key);
      if (name_map_other != nullptr) {
        namemap_add_name(name_map_other, id, name, number);
      }
      break;
    }

    /* Name had to be truncated, or number too large: mark
     * the output name as definitely changed, and proceed with the
     * truncated name again. */
    is_name_changed = true;
  }
  return is_name_changed;
}

static void namemap_remove_name(UniqueName_Map *name_map, ID *id, const char *name)
{
  BLI_assert(strlen(name) < MAX_NAME);
  UniqueName_TypeMap *type_map = name_map->find_by_type(GS(id->name));
  BLI_assert(type_map != nullptr);

  UniqueName_Key key;
  /* Remove full name from the set. */
  STRNCPY(key.name, name);
  type_map->full_names.remove(key);

  int number = MIN_NUMBER;
  BLI_string_split_name_number(name, '.', key.name, &number);
  UniqueName_Value *val = type_map->base_name_to_num_suffix.lookup_ptr(key);
  if (val == nullptr) {
    return;
  }
  if (number == 0 && val->max_value == 0) {
    /* This was the only base name usage, remove whole key. */
    type_map->base_name_to_num_suffix.remove(key);
    return;
  }
  val->mark_unused(number);
}

void BKE_main_namemap_remove_name(Main *bmain, ID *id, const char *name)
{
#ifndef __GNUC__ /* GCC warns with `nonull-compare`. */
  BLI_assert(bmain != nullptr);
  BLI_assert(id != nullptr);
  BLI_assert(name != nullptr);
#endif
  /* Name is empty or not initialized yet, nothing to remove. */
  if (name[0] == '\0') {
    return;
  }

  UniqueName_Map *name_map_local = get_namemap_for(bmain, id, false, false);
  if (name_map_local != nullptr) {
    namemap_remove_name(name_map_local, id, name);
  }

  UniqueName_Map *name_map_global = get_namemap_for(bmain, id, false, true);
  if (name_map_global != nullptr) {
    namemap_remove_name(name_map_global, id, name);
  }
}

struct Uniqueness_Key {
  char name[MAX_ID_NAME];
  Library *lib;
  uint64_t hash() const
  {
    return BLI_ghashutil_combine_hash(BLI_ghashutil_strhash_n(name, MAX_ID_NAME),
                                      BLI_ghashutil_ptrhash(lib));
  }
  bool operator==(const Uniqueness_Key &o) const
  {
    return lib == o.lib && !BLI_ghashutil_strcmp(name, o.name);
  }
};

static bool main_namemap_validate_and_fix(Main *bmain, const bool do_fix)
{
  Set<Uniqueness_Key> id_names_libs;
  Set<ID *> id_validated;
  bool is_valid = true;
  ListBase *lb_iter;
  FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb_iter) {
    LISTBASE_FOREACH_MUTABLE (ID *, id_iter, lb_iter) {
      if (id_validated.contains(id_iter)) {
        /* Do not re-check an already validated ID. */
        continue;
      }

      Uniqueness_Key key;
      STRNCPY(key.name, id_iter->name);
      key.lib = id_iter->lib;
      if (!id_names_libs.add(key)) {
        is_valid = false;
        if (do_fix) {
          CLOG_WARN(&LOG,
                    "ID name '%s' (from library '%s') is found more than once",
                    id_iter->name,
                    id_iter->lib != nullptr ? id_iter->lib->filepath : "<None>");
          /* NOTE: this may imply moving this ID in its listbase. The logic below will add the ID
           * to the validated set if it can now be added to `id_names_libs`, and will prevent
           * further checking (which would fail again, since the new ID name/lib key has already
           * been added to `id_names_libs`). */
          BKE_id_new_name_validate(
              bmain, which_libbase(bmain, GS(id_iter->name)), id_iter, nullptr, true);
          STRNCPY(key.name, id_iter->name);
          if (!id_names_libs.add(key)) {
            /* This is a serious error, very likely a bug, keep it as CLOG_ERROR even when doing
             * fixes. */
            CLOG_ERROR(&LOG,
                       "\tID has been renamed to '%s', but it still seems to be already in use",
                       id_iter->name);
          }
          else {
            CLOG_WARN(&LOG, "\tID has been renamed to '%s'", id_iter->name);
            id_validated.add(id_iter);
          }
        }
        else {
          CLOG_ERROR(&LOG,
                     "ID name '%s' (from library '%s') is found more than once",
                     id_iter->name,
                     id_iter->lib != nullptr ? id_iter->lib->filepath : "<None>");
        }
      }

      UniqueName_Map *name_map = get_namemap_for(bmain, id_iter, false, false);
      if (name_map == nullptr) {
        continue;
      }
      UniqueName_TypeMap *type_map = name_map->find_by_type(GS(id_iter->name));
      BLI_assert(type_map != nullptr);

      UniqueName_Key key_namemap;
      /* Remove full name from the set. */
      STRNCPY(key_namemap.name, id_iter->name + 2);
      if (!type_map->full_names.contains(key_namemap)) {
        is_valid = false;
        if (do_fix) {
          CLOG_INFO(
              &LOG,
              3,
              "ID name '%s' (from library '%s') exists in current Main, but is not listed in "
              "the namemap",
              id_iter->name,
              id_iter->lib != nullptr ? id_iter->lib->filepath : "<None>");
        }
        else {
          CLOG_ERROR(
              &LOG,
              "ID name '%s' (from library '%s') exists in current Main, but is not listed in "
              "the namemap",
              id_iter->name,
              id_iter->lib != nullptr ? id_iter->lib->filepath : "<None>");
        }
      }
    }
  }
  FOREACH_MAIN_LISTBASE_END;

  Library *lib = nullptr;
  UniqueName_Map *name_map = bmain->name_map;
  do {
    if (name_map != nullptr) {
      int i = 0;
      for (short idcode = BKE_idtype_idcode_iter_step(&i); idcode != 0;
           idcode = BKE_idtype_idcode_iter_step(&i))
      {
        UniqueName_TypeMap *type_map = name_map->find_by_type(idcode);
        if (type_map != nullptr) {
          for (const UniqueName_Key &id_name : type_map->full_names) {
            Uniqueness_Key key;
            *(reinterpret_cast<short *>(key.name)) = idcode;
            BLI_strncpy(key.name + 2, id_name.name, MAX_NAME);
            key.lib = lib;
            if (!id_names_libs.contains(key)) {
              is_valid = false;
              if (do_fix) {
                CLOG_INFO(
                    &LOG,
                    3,
                    "ID name '%s' (from library '%s') is listed in the namemap, but does not "
                    "exists in current Main",
                    key.name,
                    lib != nullptr ? lib->filepath : "<None>");
              }
              else {
                CLOG_ERROR(
                    &LOG,
                    "ID name '%s' (from library '%s') is listed in the namemap, but does not "
                    "exists in current Main",
                    key.name,
                    lib != nullptr ? lib->filepath : "<None>");
              }
            }
          }
        }
      }
    }
    lib = static_cast<Library *>((lib == nullptr) ? bmain->libraries.first : lib->id.next);
    name_map = (lib != nullptr) ? lib->runtime.name_map : nullptr;
  } while (lib != nullptr);

  if (is_valid || !do_fix) {
    return is_valid;
  }

  /* Clear all existing namemaps. */
  BKE_main_namemap_clear(bmain);

  return is_valid;
}

bool BKE_main_namemap_validate_and_fix(Main *bmain)
{
  const bool is_valid = main_namemap_validate_and_fix(bmain, true);
  BLI_assert(main_namemap_validate_and_fix(bmain, false));
  return is_valid;
}

bool BKE_main_namemap_validate(Main *bmain)
{
  return main_namemap_validate_and_fix(bmain, false);
}
