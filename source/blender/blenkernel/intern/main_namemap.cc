/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_namemap.hh"

#include "BLI_assert.h"
#include "BLI_bit_span_ops.hh"
#include "BLI_bit_vector.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_base.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"

#include "DNA_ID.h"

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include <fmt/format.h>

static CLG_LogRef LOG = {"lib.main_namemap"};

// #define DEBUG_PRINT_MEMORY_USAGE

using namespace blender;

/* Assumes and ensure that the suffix number can never go beyond 1 billion. */
constexpr int MAX_NUMBER = 999999999;
/* Value representing that there is no available number. Must be negative value. */
constexpr int NO_AVAILABLE_NUMBER = -1;

/* Tracking of used numeric suffixes. For each base name:
 *
 * - Exactly track which of the lowest 1023 suffixes are in use,
 *   whenever there is a name collision we pick the lowest "unused"
 *   one. This is done with a bit vector.
 * - Above 1023, do not track them exactly, just track the maximum
 *   suffix value seen so far. Upon collision, assign number that is
 *   one larger.
 */
struct UniqueName_Value {
  static constexpr int max_exact_tracking = 1023;
  std::optional<int> max_value_in_use = {};
  BitVector<> mask = {};
  /* Only created when required. Used to manage cases where the same numeric value is used by
   * several unique full names ('Foo.1' and 'Foo.001' e.g.).
   *
   * The key is the suffix numeric value.
   * The value is the number of how many different full names using that same base name and numeric
   * suffix value are currently registered.
   *
   * This code will never generate such cases for local maps, but it needs to support users doing
   * so explicitly.
   *
   * For global maps, this is a much more common case, as duplicates of ID names across libraries
   * are fairly common. */
  std::optional<Map<int, int>> numbers_multi_usages = std::nullopt;

  void mark_used(const int number)
  {
    BLI_assert(number >= 0);
    if (number >= 0 && number <= max_exact_tracking) {
      if (this->mask.size() <= number) {
        this->mask.resize(number + 1);
      }
      if (this->mask[number]) {
        if (!this->numbers_multi_usages.has_value()) {
          this->numbers_multi_usages.emplace();
        }
        int &multi_usages_num = this->numbers_multi_usages->lookup_or_add(number, 1);
        BLI_assert(multi_usages_num >= 1);
        multi_usages_num++;
      }
      else {
        this->mask[number].set(true);
      }
    }
    if (number <= MAX_NUMBER) {
      if (this->max_value_in_use) {
        math::max_inplace(this->max_value_in_use.value(), number);
      }
      else {
        this->max_value_in_use = number;
      }
    }
  }

  void mark_unused(const int number)
  {
    BLI_assert(number >= 0);
    if (number >= 0 && number <= max_exact_tracking) {
      BLI_assert_msg(number < this->mask.size(),
                     "Trying to unregister a number suffix higher than current size of the bit "
                     "vector, should never happen.");

      if (this->numbers_multi_usages.has_value() && this->numbers_multi_usages->contains(number)) {
        int &multi_usages_num = this->numbers_multi_usages->lookup(number);
        BLI_assert(multi_usages_num > 1);
        multi_usages_num--;
        if (multi_usages_num == 1) {
          this->numbers_multi_usages->remove_contained(number);
        }
        /* Do not unset the matching bit, nor handle #max_value_in_use, since there are more IDs
         * with the same base name and numeric suffix value. */
        return;
      }

      this->mask[number].set(false);
    }
    if (number == this->max_value_in_use.value_or(NO_AVAILABLE_NUMBER)) {
      if (number > 0) {
        this->max_value_in_use.value()--;
      }
      else {
        this->max_value_in_use.reset();
      }
    }
  }

  /**
   * Return the smallest non-null known free number.
   *
   * \note This is only returns the true smallest unused number for values <= #max_exact_tracking.
   * Otherwise, it simply returns the highest known value in use + 1.
   */
  int get_smallest_unused()
  {
    if (this->mask.size() < 2) {
      /* No numbered entry was ever registered for this name yet, so first non-null value is
       * unused. */
      return 1;
    }
    /* Never pick the `0` value (e.g. if 'Foo.001' is used and another 'Foo.001' is requested,
     * return 'Foo.002' and not 'Foo'). So only search on mask[1:] range. */
    BitSpan search_mask(this->mask.data(), IndexRange(1, this->mask.size() - 1));
    std::optional<int64_t> result = find_first_0_index(search_mask);
    if (result) {
      return int(*result + 1);
    }
    if (this->mask.size() <= max_exact_tracking) {
      /* No need to increase size of the mask here, this will be done by calls to #mark_used once
       * the final name with its final number has been defined. */
      return int(this->mask.size());
    }
    if (this->max_value_in_use) {
      if (this->max_value_in_use.value() + 1 <= MAX_NUMBER) {
        return this->max_value_in_use.value() + 1;
      }
      return NO_AVAILABLE_NUMBER;
    }
    return 1;
  }
};

/* Tracking of names for a single ID type. */
struct UniqueName_TypeMap {
  /* Map of full names that are in use, and the amount of times they are used.
   * For local maps, the number values must all always be `1`.
   * For global maps, a same name can be used by several IDs from different libraries. */
  Map<std::string, int> full_names;
  /* For each base name (i.e. without numeric suffix), track the
   * numeric suffixes that are in use. */
  Map<std::string, std::unique_ptr<UniqueName_Value>> base_name_to_num_suffix;
};

struct UniqueName_Map {
  std::array<UniqueName_TypeMap, INDEX_ID_MAX> type_maps;
  /* Whether this map covers all IDs in its Main, or only the local (or a specific library) ones.
   */
  bool is_global;

  UniqueName_Map(const bool is_global) : type_maps{}, is_global(is_global) {}

  UniqueName_TypeMap &find_by_type(const short id_type)
  {
    int index = BKE_idtype_idcode_to_index(id_type);
    return type_maps[size_t(index)];
  }

  void populate(Main &bmain, Library *library, ID *ignore_id)
  {
    for (UniqueName_TypeMap &type_map : this->type_maps) {
      type_map.full_names.clear();
      type_map.base_name_to_num_suffix.clear();
    }
    ID *id;
    FOREACH_MAIN_ID_BEGIN (&bmain, id) {
      if ((id == ignore_id) || (!this->is_global && (id->lib != library))) {
        continue;
      }
      UniqueName_TypeMap &type_map = this->find_by_type(GS(id->name));

      /* Insert the full name into the map. */
      if (this->is_global) {
        /* Global name-map is expected to have several IDs using the same name (from different
         * libraries). */
        int &count = type_map.full_names.lookup_or_add_as(blender::StringRef{BKE_id_name(*id)}, 0);
        count++;
        if (count > 1) {
          /* Name is already used at least once, just increase user-count. */
          continue;
        }
      }
      else {
        /* For non-global name-maps, there should only be one usage for each name, adding the
         * full-name as key should always return `true`. */
        if (!type_map.full_names.add(BKE_id_name(*id), 1)) {
          /* Do not assert, this code is also used by #BKE_main_namemap_validate_and_fix, where
           * duplicates are expected. */
#if 0
      BLI_assert_msg(false,
                     "The key (name) already exists in the namemap, should only happen when "
                     "`do_global` is true.");
#endif
          continue;
        }
      }

      /* Get the name and number parts ("name.number"). */
      int number = 0;
      const std::string name_base = BLI_string_split_name_number(BKE_id_name(*id), '.', number);

      /* Get and update the entry for this base name. */
      std::unique_ptr<UniqueName_Value> &val = type_map.base_name_to_num_suffix.lookup_or_add_as(
          name_base, std::make_unique<UniqueName_Value>(UniqueName_Value{}));
      val->mark_used(number);
    }
    FOREACH_MAIN_ID_END;
  }

  /* Add the given name to the given #type_map, returns `true` if added, `false` if it was already
   * in the map. */
  void add_name(UniqueName_TypeMap &type_map,
                StringRef name_full,
                StringRef name_base,
                const int number)
  {
    BLI_assert(name_full.size() < MAX_ID_NAME - 2);

    if (this->is_global) {
      /* By definition adding to global map is always successful. */
      int &count = type_map.full_names.lookup_or_add_as(name_full, 0);
      if (!count) {
        std::unique_ptr<UniqueName_Value> &val = type_map.base_name_to_num_suffix.lookup_or_add_as(
            name_base, std::make_unique<UniqueName_Value>(UniqueName_Value{}));
        val->mark_used(number);
      }
      count++;
      return;
    }

    if (!type_map.full_names.add(name_full, 1)) {
      BLI_assert_msg(
          false, "Adding a name to Main namemaps that was already in it, should never happen.");
      return;
    }

    std::unique_ptr<UniqueName_Value> &val = type_map.base_name_to_num_suffix.lookup_or_add_as(
        name_base, std::make_unique<UniqueName_Value>(UniqueName_Value{}));
    val->mark_used(number);
  }
  void add_name(const short id_type, StringRef name_full, StringRef name_base, const int number)
  {
    this->add_name(this->find_by_type(id_type), name_full, name_base, number);
  }

  /* Remove a full name_full from the specified #type_map. Trying to remove an unknown
   * (unregistered) name_full is an error. */
  void remove_full_name(UniqueName_TypeMap &type_map, blender::StringRef name_full)
  {
    BLI_assert(name_full.size() < MAX_ID_NAME - 2);

    if (this->is_global) {
      /* By definition adding to global map is always successful. */
      int *count = type_map.full_names.lookup_ptr(name_full);
      if (!count) {
        BLI_assert_msg(
            false, "Removing a name from Main namemaps that was not in it, should never happen.");
        return;
      }
      if (*count > 1) {
        (*count)--;
      }
      else {
        BLI_assert(*count == 1);
        type_map.full_names.remove_contained(name_full);
      }
    }
    else if (!type_map.full_names.remove(name_full)) {
      BLI_assert_msg(
          false, "Removing a name from Main namemaps that was not in it, should never happen.");
      return;
    }

    int number = 0;
    const std::string name_base = BLI_string_split_name_number(name_full, '.', number);
    std::unique_ptr<UniqueName_Value> *val = type_map.base_name_to_num_suffix.lookup_ptr(
        name_base);
    if (val == nullptr) {
      BLI_assert_unreachable();
      return;
    }
    val->get()->mark_unused(number);
    if (!val->get()->max_value_in_use) {
      /* This was the only base name usage, remove the whole key. */
      type_map.base_name_to_num_suffix.remove(name_base);
    }
  }
  void remove_full_name(const short id_type, blender::StringRef name_full)
  {
    this->remove_full_name(this->find_by_type(id_type), name_full);
  }
};

void BKE_main_namemap_destroy(UniqueName_Map **r_name_map)
{
  if (*r_name_map == nullptr) {
    return;
  }
#ifdef DEBUG_PRINT_MEMORY_USAGE
  int64_t size_full_names = 0;
  int64_t size_base_names = 0;
  for (const UniqueName_TypeMap &type_map : (*r_name_map)->type_maps) {
    size_full_names += type_map.full_names.size_in_bytes();
    size_base_names += type_map.base_name_to_num_suffix.size_in_bytes();
  }
  printf("NameMap memory usage: full_names %.1fKB, base_names %.1fKB\n",
         size_full_names / 1024.0,
         size_base_names / 1024.0);
#endif
  MEM_SAFE_DELETE(*r_name_map);
}

void BKE_main_namemap_clear(Main &bmain)
{
  auto bmain_namemap_clear = [](Main *bmain_iter) -> void {
    BKE_main_namemap_destroy(&bmain_iter->name_map);
    BKE_main_namemap_destroy(&bmain_iter->name_map_global);
    for (Library *lib_iter = static_cast<Library *>(bmain_iter->libraries.first);
         lib_iter != nullptr;
         lib_iter = static_cast<Library *>(lib_iter->id.next))
    {
      BKE_main_namemap_destroy(&lib_iter->runtime->name_map);
    }
  };

  if (bmain.split_mains) {
    BLI_assert_msg(bmain.split_mains->contains(&bmain),
                   "Main should always be part of its own `split_mains`");
    for (Main *bmain_iter : *bmain.split_mains) {
      bmain_namemap_clear(bmain_iter);
    }
  }
  else {
    bmain_namemap_clear(&bmain);
  }
}

/* Get the global (local and linked IDs) name map object used for the given Main/ID.
 * Lazily creates and populates the contents of the name map, if ensure_created is true.
 * NOTE: if the contents are populated, the name of the given #ignore_id ID (if any) is not added.
 */
static UniqueName_Map *get_global_namemap_for(Main &bmain,
                                              ID *ignore_id,
                                              const bool ensure_created)
{
  if (ensure_created && bmain.name_map_global == nullptr) {
    bmain.name_map_global = MEM_new<UniqueName_Map>(__func__, true);
    bmain.name_map_global->populate(bmain, nullptr, ignore_id);
  }
  return bmain.name_map_global;
}

/* Get the local or library-specific (when #lib is not null) name map object used for the given
 * Main/ID. Lazily creates and populates the contents of the name map, if ensure_created is true.
 * NOTE: if the contents are populated, the name of the given #ignore_id ID (if any) is not added.
 */
static UniqueName_Map *get_namemap_for(Main &bmain,
                                       Library *lib,
                                       ID *ignore_id,
                                       const bool ensure_created)
{
  if (lib != nullptr) {
    if (ensure_created && lib->runtime->name_map == nullptr) {
      lib->runtime->name_map = MEM_new<UniqueName_Map>(__func__, false);
      lib->runtime->name_map->populate(bmain, lib, ignore_id);
    }
    return lib->runtime->name_map;
  }
  if (ensure_created && bmain.name_map == nullptr) {
    bmain.name_map = MEM_new<UniqueName_Map>(__func__, false);
    bmain.name_map->populate(bmain, lib, ignore_id);
  }
  return bmain.name_map;
}

bool BKE_main_global_namemap_contain_name(Main &bmain, const short id_type, StringRef name)
{
  UniqueName_Map *name_map = get_global_namemap_for(bmain, nullptr, true);
  BLI_assert(name_map != nullptr);
  BLI_assert(name.size() < MAX_ID_NAME - 2);
  UniqueName_TypeMap &type_map = name_map->find_by_type(id_type);

  return type_map.full_names.contains(name);
}

bool BKE_main_namemap_contain_name(Main &bmain, Library *lib, const short id_type, StringRef name)
{
  UniqueName_Map *name_map = get_namemap_for(bmain, lib, nullptr, true);
  BLI_assert(name_map != nullptr);
  BLI_assert(name.size() < MAX_ID_NAME - 2);
  UniqueName_TypeMap &type_map = name_map->find_by_type(id_type);

  return type_map.full_names.contains(name);
}

/**
 * Helper building final ID name from given base_name and number.
 *
 * Return `true` in case of success (a valid final ID name can be generated from given #base_name
 * and #number parameters).
 *
 * Return `false` in case either the given number is invalid (#NO_AVAILABLE_NUMBER), or the
 * generated final name would be too long. #r_name_final is then set with a new, edited base name:
 *  - Shortened by one (UTF8) char in case of too long name.
 *  - Extended by a pseudo-random number in case the base name is short already (should only happen
 *    when #number is #NO_AVAILABLE_NUMBER).
 */
static bool id_name_final_build(UniqueName_TypeMap &type_map,
                                const StringRefNull base_name,
                                const int number,
                                std::string &r_name_final)
{
  /* In case no number value is available, current base name cannot be used to generate a final
   * full name. */
  if (number != NO_AVAILABLE_NUMBER) {
    BLI_assert(number >= 0 && number <= MAX_NUMBER);
    r_name_final = fmt::format("{}.{:03}", base_name, number);
    /* Most common case, there is a valid number suffix value and it fits in the #MAX_ID_NAME - 2
     * length limit.
     */
    if (r_name_final.size() < MAX_ID_NAME - 2) {
      return true;
    }
  }

  /* The base name cannot be used as-is, it needs to be modified, and a new number suffix must be
   * generated for it. */
  r_name_final = base_name;

  /* If the base name is long enough, shorten it by one (UTF8) char, until a base name with
   * available number suffixes is found. */
  while (r_name_final.size() > 8) {
    char base_name_modified[MAX_ID_NAME - 2];

    BLI_strncpy(base_name_modified, r_name_final.c_str(), r_name_final.size() + 1);
    base_name_modified[r_name_final.size() - 1] = '\0';
    /* Raw truncation of an UTF8 string may generate invalid UTF8 char-code at the end.
     * Ensure we get a valid one now. */
    BLI_str_utf8_invalid_strip(base_name_modified, r_name_final.size() - 1);

    r_name_final = base_name_modified;
    std::unique_ptr<UniqueName_Value> *val = type_map.base_name_to_num_suffix.lookup_ptr(
        r_name_final);
    if (!val || val->get()->max_value_in_use.value_or(0) < MAX_NUMBER) {
      return false;
    }
  }
  /* Else, extend the name with an increasing three-or-more-digits number, as it's better to add
   * gibberish at the end of a short name, rather than shorten it further. */
  uint64_t suffix = 1;
  const StringRef new_base_name = r_name_final;
  r_name_final = fmt::format("{}_{:03}", r_name_final, suffix);
  while (r_name_final.size() < MAX_ID_NAME - 2 - 12) {
    std::unique_ptr<UniqueName_Value> *val = type_map.base_name_to_num_suffix.lookup_ptr(
        r_name_final);
    if (!val || val->get()->max_value_in_use.value_or(0) < MAX_NUMBER) {
      return false;
    }
    suffix++;
    r_name_final = fmt::format("{}_{:03}", new_base_name, suffix);
  }

  /* WARNING: This code is absolute last defense, essentially theoretical case at this point.
   * It is not expected to ever be reached in practice. It is also virtually impossible to actually
   * test that case, given the enormous amount of IDs that would need to be created before it is
   * reached. */
  CLOG_ERROR(&LOG,
             "Impossible to find an available name for '%s' base name, even by editing that base "
             "name. This should never happen in real-life scenarii. Now trying to brute-force "
             "generate random names until a free one is found.",
             base_name.c_str());
  BLI_assert(new_base_name.size() <= 8);
  while (true) {
    r_name_final = fmt::format("{}_{}", new_base_name, uint32_t(get_default_hash(r_name_final)));
    std::unique_ptr<UniqueName_Value> *val = type_map.base_name_to_num_suffix.lookup_ptr(
        r_name_final);
    if (!val || val->get()->max_value_in_use.value_or(0) < MAX_NUMBER) {
      return false;
    }
  }
}

static bool namemap_get_name(Main &bmain,
                             ID &id,
                             std::string &r_name_full,
                             const bool do_unique_in_bmain)
{
  UniqueName_Map *name_map = do_unique_in_bmain ? get_global_namemap_for(bmain, &id, true) :
                                                  get_namemap_for(bmain, id.lib, &id, true);
  UniqueName_Map *name_map_other = do_unique_in_bmain ?
                                       get_namemap_for(bmain, id.lib, &id, false) :
                                       get_global_namemap_for(bmain, &id, false);
  BLI_assert(name_map != nullptr);
  BLI_assert(r_name_full.size() < MAX_ID_NAME - 2);
  UniqueName_TypeMap &type_map = name_map->find_by_type(GS(id.name));

  bool is_name_changed = false;

  while (true) {
    /* Get the name and number parts ("name.number"). */
    int number = 0;
    const std::string name_base = BLI_string_split_name_number(r_name_full, '.', number);
    std::unique_ptr<UniqueName_Value> &val = type_map.base_name_to_num_suffix.lookup_or_add_as(
        name_base, std::make_unique<UniqueName_Value>(UniqueName_Value{}));

    /* If the full original name is unused, and its number suffix is unused, or is above the max
     * managed value, the name can be used directly.
     *
     * NOTE: The second part of the check is designed to prevent issues with different names with
     * the same name base, and the same numeric value as suffix, but written differently.
     * E.g. `Mesh.001` and `Mesh.1` would both "use" the numeric suffix for base name `Mesh`.
     * Removing `Mesh.1` would then mark `001` suffix as available, which would be incorrect. */
    if (!type_map.full_names.contains(r_name_full)) {
      name_map->add_name(type_map, r_name_full, name_base, number);
      if (name_map_other != nullptr) {
        name_map_other->add_name(GS(id.name), r_name_full, name_base, number);
      }
      return is_name_changed;
    }

    /* At this point, if this is the first iteration, the initially given name is colliding with an
     * existing ID name, and has to be modified. If this is a later iteration, the given name has
     * already been modified one way or another. */
    is_name_changed = true;

    /* The base name and current number suffix are already used.
     * Request the lowest available valid number suffix (will return #NO_AVAILABLE_NUMBER if none
     * are available for the current base name). */
    const int number_to_use = val->get_smallest_unused();

    /* Try to build final name from the current base name and the number.
     * Note that this will fail if the suffix number is #NO_AVAILABLE_NUMBER, or if the base name
     * and suffix number would give a too long name. In such cases, this call will modify
     * the base name and put it into r_name_full, and a new iteration to find a suitable suffix
     * number and valid full name is needed. */
    if (!id_name_final_build(type_map, name_base, number_to_use, r_name_full)) {
      continue;
    }

    /* All good, add final name to the set. */
    name_map->add_name(type_map, r_name_full, name_base, number_to_use);
    if (name_map_other != nullptr) {
      name_map_other->add_name(GS(id.name), r_name_full, name_base, number_to_use);
    }
    return is_name_changed;
  }
}

bool BKE_main_namemap_get_unique_name(Main &bmain, ID &id, char *r_name)
{
  std::string r_name_full = r_name;
  BLI_assert(r_name_full.size() < MAX_ID_NAME - 2);
  const bool is_name_modified = namemap_get_name(bmain, id, r_name_full, false);
  BLI_assert(r_name_full.size() < MAX_ID_NAME - 2);
  BLI_strncpy(r_name, r_name_full.c_str(), MAX_ID_NAME - 2);
  return is_name_modified;
}
bool BKE_main_global_namemap_get_unique_name(Main &bmain, ID &id, char *r_name)
{
  std::string r_name_full = r_name;
  BLI_assert(r_name_full.size() < MAX_ID_NAME - 2);
  const bool is_name_modified = namemap_get_name(bmain, id, r_name_full, true);
  BLI_assert(r_name_full.size() < MAX_ID_NAME - 2);
  BLI_strncpy(r_name, r_name_full.c_str(), MAX_ID_NAME - 2);
  return is_name_modified;
}

void BKE_main_namemap_remove_id(Main &bmain, ID &id)
{
  StringRef name = BKE_id_name(id);
  if (name.is_empty()) {
    /* Name is empty or not initialized yet, nothing to remove. */
    return;
  }
  const short id_code = GS(id.name);

  UniqueName_Map *name_map_local = get_namemap_for(bmain, id.lib, nullptr, false);
  if (name_map_local) {
    name_map_local->remove_full_name(id_code, name);
  }

  UniqueName_Map *name_map_global = get_global_namemap_for(bmain, nullptr, false);
  if (name_map_global) {
    name_map_global->remove_full_name(id_code, name);
  }
}

struct Uniqueness_Key {
  std::string name;
  Library *lib;
  uint64_t hash() const
  {
    return blender::get_default_hash(name, lib);
  }
  friend bool operator==(const Uniqueness_Key &a, const Uniqueness_Key &b)
  {
    return a.lib == b.lib && a.name == b.name;
  }
};

static bool main_namemap_validate_and_fix(Main &bmain, const bool do_fix)
{
  Set<Uniqueness_Key> id_names_libs;
  Set<ID *> id_validated;
  bool is_valid = true;
  ListBase *lb_iter;
  FOREACH_MAIN_LISTBASE_BEGIN (&bmain, lb_iter) {
    LISTBASE_FOREACH_MUTABLE (ID *, id_iter, lb_iter) {
      if (id_validated.contains(id_iter)) {
        /* Do not re-check an already validated ID. */
        continue;
      }

      Uniqueness_Key key = {id_iter->name, id_iter->lib};
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
          BKE_id_new_name_validate(bmain,
                                   *which_libbase(&bmain, GS(id_iter->name)),
                                   *id_iter,
                                   nullptr,
                                   IDNewNameMode::RenameExistingNever,
                                   true);
          key.name = id_iter->name;
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

      UniqueName_Map *name_map = get_namemap_for(bmain, id_iter->lib, id_iter, false);
      if (name_map == nullptr) {
        continue;
      }
      UniqueName_TypeMap &type_map = name_map->find_by_type(GS(id_iter->name));

      /* Remove full name from the set. */
      const std::string id_name = BKE_id_name(*id_iter);
      if (!type_map.full_names.contains(id_name)) {
        is_valid = false;
        if (do_fix) {
          CLOG_WARN(
              &LOG,
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
  UniqueName_Map *name_map = bmain.name_map;
  do {
    if (name_map) {
      int i = 0;
      for (short idcode = BKE_idtype_idcode_iter_step(&i); idcode != 0;
           idcode = BKE_idtype_idcode_iter_step(&i))
      {
        UniqueName_TypeMap &type_map = name_map->find_by_type(idcode);
        for (const std::string &id_name : type_map.full_names.keys()) {
          Uniqueness_Key key = {
              fmt::format("{}{}", StringRef{reinterpret_cast<char *>(&idcode), 2}, id_name), lib};
          if (!id_names_libs.contains(key)) {
            is_valid = false;
            if (do_fix) {
              CLOG_WARN(&LOG,
                        "ID name '%s' (from library '%s') is listed in the namemap, but does not "
                        "exists in current Main",
                        key.name.c_str(),
                        lib != nullptr ? lib->filepath : "<None>");
            }
            else {
              CLOG_ERROR(&LOG,
                         "ID name '%s' (from library '%s') is listed in the namemap, but does not "
                         "exists in current Main",
                         key.name.c_str(),
                         lib != nullptr ? lib->filepath : "<None>");
            }
          }
        }
      }
    }

    lib = static_cast<Library *>((lib == nullptr) ? bmain.libraries.first : lib->id.next);
    name_map = (lib != nullptr) ? lib->runtime->name_map : nullptr;
  } while (lib != nullptr);

  if (is_valid || !do_fix) {
    return is_valid;
  }

  /* Clear all existing name-maps. */
  BKE_main_namemap_clear(bmain);

  return is_valid;
}

bool BKE_main_namemap_validate_and_fix(Main &bmain)
{
  const bool is_valid = main_namemap_validate_and_fix(bmain, true);
  BLI_assert(main_namemap_validate_and_fix(bmain, false));
  return is_valid;
}

bool BKE_main_namemap_validate(Main &bmain)
{
  return main_namemap_validate_and_fix(bmain, false);
}
