/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Contains management of #Main database itself.
 */

#include <cstring>
#include <iostream>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_map.hh"
#include "BLI_mempool.h"
#include "BLI_threads.h"
#include "BLI_vector.hh"

#include "DNA_ID.h"

#include "BKE_asset.hh"
#include "BKE_bpath.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_main.hh"
#include "BKE_main_idmap.hh"
#include "BKE_main_namemap.hh"
#include "BKE_report.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

using namespace blender::bke;

static CLG_LogRef LOG = {"bke.main"};

Main *BKE_main_new()
{
  Main *bmain = static_cast<Main *>(MEM_callocN(sizeof(Main), "new main"));
  bmain->lock = static_cast<MainLock *>(MEM_mallocN(sizeof(SpinLock), "main lock"));
  BLI_spin_init((SpinLock *)bmain->lock);
  bmain->is_global_main = false;
  return bmain;
}

void BKE_main_free(Main *mainvar)
{
  /* In case this is called on a 'split-by-libraries' list of mains.
   *
   * Should not happen in typical usages, but can occur e.g. if a file reading is aborted. */
  if (mainvar->next) {
    BKE_main_free(mainvar->next);
  }

  /* Include this check here as the path may be manipulated after creation. */
  BLI_assert_msg(!(mainvar->filepath[0] == '/' && mainvar->filepath[1] == '/'),
                 "'.blend' relative \"//\" must not be used in Main!");

  /* also call when reading a file, erase all, etc */
  ListBase *lbarray[INDEX_ID_MAX];
  int a;

  /* Since we are removing whole main, no need to bother 'properly'
   * (and slowly) removing each ID from it. */
  const int free_flag = (LIB_ID_FREE_NO_MAIN | LIB_ID_FREE_NO_UI_USER |
                         LIB_ID_FREE_NO_USER_REFCOUNT | LIB_ID_FREE_NO_DEG_TAG);

  MEM_SAFE_FREE(mainvar->blen_thumb);

  a = set_listbasepointers(mainvar, lbarray);
  while (a--) {
    ListBase *lb = lbarray[a];
    ID *id, *id_next;

    for (id = static_cast<ID *>(lb->first); id != nullptr; id = id_next) {
      id_next = static_cast<ID *>(id->next);
#if 1
      BKE_id_free_ex(mainvar, id, free_flag, false);
#else
      /* Errors freeing ID's can be hard to track down,
       * enable this so VALGRIND or ASAN will give the line number in its error log. */

#  define CASE_ID_INDEX(id_index) \
    case id_index: \
      BKE_id_free_ex(mainvar, id, free_flag, false); \
      break

      switch ((eID_Index)a) {
        CASE_ID_INDEX(INDEX_ID_LI);
        CASE_ID_INDEX(INDEX_ID_IP);
        CASE_ID_INDEX(INDEX_ID_AC);
        CASE_ID_INDEX(INDEX_ID_GD_LEGACY);
        CASE_ID_INDEX(INDEX_ID_NT);
        CASE_ID_INDEX(INDEX_ID_VF);
        CASE_ID_INDEX(INDEX_ID_TXT);
        CASE_ID_INDEX(INDEX_ID_SO);
        CASE_ID_INDEX(INDEX_ID_MSK);
        CASE_ID_INDEX(INDEX_ID_IM);
        CASE_ID_INDEX(INDEX_ID_MC);
        CASE_ID_INDEX(INDEX_ID_TE);
        CASE_ID_INDEX(INDEX_ID_MA);
        CASE_ID_INDEX(INDEX_ID_LS);
        CASE_ID_INDEX(INDEX_ID_WO);
        CASE_ID_INDEX(INDEX_ID_CF);
        CASE_ID_INDEX(INDEX_ID_SIM);
        CASE_ID_INDEX(INDEX_ID_PA);
        CASE_ID_INDEX(INDEX_ID_KE);
        CASE_ID_INDEX(INDEX_ID_AR);
        CASE_ID_INDEX(INDEX_ID_ME);
        CASE_ID_INDEX(INDEX_ID_CU_LEGACY);
        CASE_ID_INDEX(INDEX_ID_MB);
        CASE_ID_INDEX(INDEX_ID_CV);
        CASE_ID_INDEX(INDEX_ID_PT);
        CASE_ID_INDEX(INDEX_ID_VO);
        CASE_ID_INDEX(INDEX_ID_LT);
        CASE_ID_INDEX(INDEX_ID_LA);
        CASE_ID_INDEX(INDEX_ID_CA);
        CASE_ID_INDEX(INDEX_ID_SPK);
        CASE_ID_INDEX(INDEX_ID_LP);
        CASE_ID_INDEX(INDEX_ID_OB);
        CASE_ID_INDEX(INDEX_ID_GR);
        CASE_ID_INDEX(INDEX_ID_PAL);
        CASE_ID_INDEX(INDEX_ID_PC);
        CASE_ID_INDEX(INDEX_ID_BR);
        CASE_ID_INDEX(INDEX_ID_SCE);
        CASE_ID_INDEX(INDEX_ID_SCR);
        CASE_ID_INDEX(INDEX_ID_WS);
        CASE_ID_INDEX(INDEX_ID_WM);
        case INDEX_ID_NULL: {
          BLI_assert_unreachable();
          break;
        }
      }

#  undef CASE_ID_INDEX

#endif
    }
    BLI_listbase_clear(lb);
  }

  if (mainvar->relations) {
    BKE_main_relations_free(mainvar);
  }

  if (mainvar->id_map) {
    BKE_main_idmap_destroy(mainvar->id_map);
  }

  /* NOTE: `name_map` in libraries are freed together with the library IDs above. */
  if (mainvar->name_map) {
    BKE_main_namemap_destroy(&mainvar->name_map);
  }
  if (mainvar->name_map_global) {
    BKE_main_namemap_destroy(&mainvar->name_map_global);
  }

  BLI_spin_end((SpinLock *)mainvar->lock);
  MEM_freeN(mainvar->lock);
  MEM_freeN(mainvar);
}

static bool are_ids_from_different_mains_matching(Main *bmain_1, ID *id_1, Main *bmain_2, ID *id_2)
{
  /* Both IDs should not be null at the same time.
   *
   * NOTE: E.g. `id_1` may be null, in case `id_2` is a Library ID which path is the filepath of
   * `bmain_1`. */
  BLI_assert(id_1 || id_2);

  /* Special handling for libraries, since their filepaths is used then, not their ID names.
   *
   * NOTE: In library case, this call should always return true, since given data should always
   * match. The asserts below merely ensure that expected conditions are always met:
   *   - A given library absolute filepath should never match its own bmain filepath.
   *   - If both given libraries are non-null:
   *     - Their absolute filepath should match.
   *     - Neither of their absolute filepaths should match any of the bmain filepaths.
   *   - If one of the library is null:
   *      - The other library should match the bmain filepath of the null library. */
  if ((!id_1 && GS(id_2->name) == ID_LI) || GS(id_1->name) == ID_LI) {
    BLI_assert(!id_1 || !ID_IS_LINKED(id_1));
    BLI_assert(!id_2 || !ID_IS_LINKED(id_2));

    Library *lib_1 = reinterpret_cast<Library *>(id_1);
    Library *lib_2 = reinterpret_cast<Library *>(id_2);

    if (lib_1 && lib_2) {
      BLI_assert(STREQ(lib_1->filepath_abs, lib_2->filepath_abs));
    }
    if (lib_1) {
      BLI_assert(!STREQ(lib_1->filepath_abs, bmain_1->filepath));
      if (lib_2) {
        BLI_assert(!STREQ(lib_1->filepath_abs, bmain_2->filepath));
      }
      else {
        BLI_assert(STREQ(lib_1->filepath_abs, bmain_2->filepath));
      }
    }
    if (lib_2) {
      BLI_assert(!STREQ(lib_2->filepath_abs, bmain_2->filepath));
      if (lib_1) {
        BLI_assert(!STREQ(lib_2->filepath_abs, bmain_1->filepath));
      }
      else {
        BLI_assert(STREQ(lib_2->filepath_abs, bmain_1->filepath));
      }
    }

    return true;
  }

  /* Now both IDs are expected to be valid data, and caller is expected to have ensured already
   * that they have the same name. */
  BLI_assert(id_1 && id_2);
  BLI_assert(STREQ(id_1->name, id_2->name));

  if (!id_1->lib && !id_2->lib) {
    return true;
  }

  if (id_1->lib && id_2->lib) {
    if (id_1->lib == id_2->lib) {
      return true;
    }
    if (STREQ(id_1->lib->filepath_abs, id_2->lib->filepath_abs)) {
      return true;
    }
    return false;
  }

  /* In case one Main is the library of the ID from the other Main. */

  if (id_1->lib) {
    if (STREQ(id_1->lib->filepath_abs, bmain_2->filepath)) {
      return true;
    }
    return false;
  }

  if (id_2->lib) {
    if (STREQ(id_2->lib->filepath_abs, bmain_1->filepath)) {
      return true;
    }
    return false;
  }

  BLI_assert_unreachable();
  return false;
}

static void main_merge_add_id_to_move(Main *bmain_dst,
                                      blender::Map<std::string, blender::Vector<ID *>> &id_map_dst,
                                      ID *id_src,
                                      id::IDRemapper &id_remapper,
                                      blender::Vector<ID *> &ids_to_move,
                                      const bool is_library,
                                      MainMergeReport &reports)
{
  const bool is_id_src_linked(id_src->lib);
  bool is_id_src_from_bmain_dst = false;
  if (is_id_src_linked) {
    BLI_assert(!is_library);
    UNUSED_VARS_NDEBUG(is_library);
    blender::Vector<ID *> id_src_lib_dst = id_map_dst.lookup_default(id_src->lib->filepath_abs,
                                                                     {});
    /* The current library of the source ID would be remapped to null, which means that it comes
     * from the destination Main. */
    is_id_src_from_bmain_dst = !id_src_lib_dst.is_empty() && !id_src_lib_dst[0];
  }
  std::cout << id_src->name << " is linked from dst Main: " << is_id_src_from_bmain_dst << "\n";
  std::cout.flush();

  if (is_id_src_from_bmain_dst) {
    /* Do not move an ID supposed to be from `bmain_dst` (used as library in `bmain_src`) into
     * `bmain_src`. Fact that no match was found is worth a warning, although it could happen
     * e.g. in case `bmain_dst` has been updated since it file was loaded as library in
     * `bmain_src`. */
    CLOG_WARN(&LOG,
              "ID '%s' defined in source Main as linked from destination Main (file '%s') not "
              "found in given destination Main",
              id_src->name,
              bmain_dst->filepath);
    id_remapper.add(id_src, nullptr);
    reports.num_unknown_ids++;
  }
  else {
    ids_to_move.append(id_src);
  }
}

void BKE_main_merge(Main *bmain_dst, Main **r_bmain_src, MainMergeReport &reports)
{
  Main *bmain_src = *r_bmain_src;
  /* NOTE: Dedicated mapping type is needed here, to handle properly the library cases. */
  blender::Map<std::string, blender::Vector<ID *>> id_map_dst;
  ID *id_iter_dst, *id_iter_src;
  FOREACH_MAIN_ID_BEGIN (bmain_dst, id_iter_dst) {
    if (GS(id_iter_dst->name) == ID_LI) {
      /* Libraries need specific handling, as we want to check them by their filepath, not the IDs
       * themselves. */
      Library *lib_dst = reinterpret_cast<Library *>(id_iter_dst);
      BLI_assert(!id_map_dst.contains(lib_dst->filepath_abs));
      id_map_dst.add(lib_dst->filepath_abs, {id_iter_dst});
    }
    else {
      id_map_dst.lookup_or_add(id_iter_dst->name, {}).append(id_iter_dst);
    }
  }
  FOREACH_MAIN_ID_END;
  /* Add the current `bmain_dst` filepath in the mapping as well, as it may be a library of the
   * `bmain_src` Main. */
  id_map_dst.add(bmain_dst->filepath, {nullptr});

  /* A dedicated remapper for libraries is needed because these need to be remapped _before_ IDs
   * are moved from `bmain_src` to `bmain_dst`, to avoid having to fix naming and ordering of IDs
   * afterwards (especially in case some source linked IDs become local in `bmain_dst`). */
  id::IDRemapper id_remapper;
  id::IDRemapper id_remapper_libraries;
  blender::Vector<ID *> ids_to_move;

  FOREACH_MAIN_ID_BEGIN (bmain_src, id_iter_src) {
    const bool is_library = GS(id_iter_src->name) == ID_LI;

    blender::Vector<ID *> ids_dst = id_map_dst.lookup_default(
        is_library ? reinterpret_cast<Library *>(id_iter_src)->filepath_abs : id_iter_src->name,
        {});
    if (is_library) {
      BLI_assert(ids_dst.size() <= 1);
    }
    if (ids_dst.is_empty()) {
      main_merge_add_id_to_move(
          bmain_dst, id_map_dst, id_iter_src, id_remapper, ids_to_move, is_library, reports);
      continue;
    }

    bool src_has_match_in_dst = false;
    for (ID *id_iter_dst : ids_dst) {
      if (are_ids_from_different_mains_matching(bmain_dst, id_iter_dst, bmain_src, id_iter_src)) {
        /* There should only ever be one potential match, never more. */
        BLI_assert(!src_has_match_in_dst);
        if (!src_has_match_in_dst) {
          if (is_library) {
            id_remapper_libraries.add(id_iter_src, id_iter_dst);
            reports.num_remapped_libraries++;
          }
          else {
            id_remapper.add(id_iter_src, id_iter_dst);
            reports.num_remapped_ids++;
          }
          src_has_match_in_dst = true;
        }
#ifdef NDEBUG /* In DEBUG builds, keep looping to ensure there is only one match. */
        break;
#endif
      }
    }
    if (!src_has_match_in_dst) {
      main_merge_add_id_to_move(
          bmain_dst, id_map_dst, id_iter_src, id_remapper, ids_to_move, is_library, reports);
    }
  }
  FOREACH_MAIN_ID_END;

  reports.num_merged_ids = int(ids_to_move.size());

  /* Rebase relative filepaths in `bmain_src` using `bmain_dst` path as new reference, or make them
   * absolute if destination bmain has no filepath. */
  if (bmain_src->filepath[0] != '\0') {
    char dir_src[FILE_MAXDIR];
    BLI_path_split_dir_part(bmain_src->filepath, dir_src, sizeof(dir_src));
    BLI_path_normalize_native(dir_src);

    if (bmain_dst->filepath[0] != '\0') {
      char dir_dst[FILE_MAXDIR];
      BLI_path_split_dir_part(bmain_dst->filepath, dir_dst, sizeof(dir_dst));
      BLI_path_normalize_native(dir_dst);
      BKE_bpath_relative_rebase(bmain_src, dir_src, dir_dst, reports.reports);
    }
    else {
      BKE_bpath_absolute_convert(bmain_src, dir_src, reports.reports);
    }
  }

  /* Libraries need to be remapped before moving IDs into `bmain_dst`, to ensure that the sorting
   * of inserted IDs is correct. Note that no bmain is given here, so this is only a 'raw'
   * remapping. */
  BKE_libblock_relink_multiple(nullptr,
                               ids_to_move,
                               ID_REMAP_TYPE_REMAP,
                               id_remapper_libraries,
                               ID_REMAP_DO_LIBRARY_POINTERS);

  for (ID *id_iter_src : ids_to_move) {
    BKE_libblock_management_main_remove(bmain_src, id_iter_src);
    BKE_libblock_management_main_add(bmain_dst, id_iter_src);
  }

  /* The other data has to be remapped once all IDs are in `bmain_dst`, to ensure that additional
   * update process (e.g. collection hierarchy handling) happens as expected with the correct set
   * of data. */
  BKE_libblock_relink_multiple(bmain_dst, ids_to_move, ID_REMAP_TYPE_REMAP, id_remapper, 0);

  BKE_reportf(
      reports.reports,
      RPT_INFO,
      "Merged %d IDs from '%s' Main into '%s' Main; %d IDs and %d Libraries already existed as "
      "part of the destination Main, and %d IDs missing from destination Main, were freed "
      "together with the source Main",
      reports.num_merged_ids,
      bmain_src->filepath,
      bmain_dst->filepath,
      reports.num_remapped_ids,
      reports.num_remapped_libraries,
      reports.num_unknown_ids);

  /* Remapping above may have made some IDs local. So namemap needs to be cleared, and moved IDs
   * need to be re-sorted. */
  BKE_main_namemap_clear(bmain_dst);

  BLI_assert(BKE_main_namemap_validate(bmain_dst));

  BKE_main_free(bmain_src);
  *r_bmain_src = nullptr;
}

bool BKE_main_is_empty(Main *bmain)
{
  bool result = true;
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    result = false;
    break;
  }
  FOREACH_MAIN_ID_END;
  return result;
}

bool BKE_main_has_issues(const Main *bmain)
{
  return bmain->has_forward_compatibility_issues || bmain->is_asset_repository;
}

bool BKE_main_needs_overwrite_confirm(const Main *bmain)
{
  return bmain->has_forward_compatibility_issues || bmain->is_asset_repository;
}

void BKE_main_lock(Main *bmain)
{
  BLI_spin_lock((SpinLock *)bmain->lock);
}

void BKE_main_unlock(Main *bmain)
{
  BLI_spin_unlock((SpinLock *)bmain->lock);
}

static int main_relations_create_idlink_cb(LibraryIDLinkCallbackData *cb_data)
{
  MainIDRelations *bmain_relations = static_cast<MainIDRelations *>(cb_data->user_data);
  ID *self_id = cb_data->self_id;
  ID **id_pointer = cb_data->id_pointer;
  const int cb_flag = cb_data->cb_flag;

  if (*id_pointer) {
    MainIDRelationsEntry **entry_p;

    /* Add `id_pointer` as child of `self_id`. */
    {
      if (!BLI_ghash_ensure_p(
              bmain_relations->relations_from_pointers, self_id, (void ***)&entry_p))
      {
        *entry_p = static_cast<MainIDRelationsEntry *>(MEM_callocN(sizeof(**entry_p), __func__));
        (*entry_p)->session_uid = self_id->session_uid;
      }
      else {
        BLI_assert((*entry_p)->session_uid == self_id->session_uid);
      }
      MainIDRelationsEntryItem *to_id_entry = static_cast<MainIDRelationsEntryItem *>(
          BLI_mempool_alloc(bmain_relations->entry_items_pool));
      to_id_entry->next = (*entry_p)->to_ids;
      to_id_entry->id_pointer.to = id_pointer;
      to_id_entry->session_uid = (*id_pointer != nullptr) ? (*id_pointer)->session_uid :
                                                            MAIN_ID_SESSION_UID_UNSET;
      to_id_entry->usage_flag = cb_flag;
      (*entry_p)->to_ids = to_id_entry;
    }

    /* Add `self_id` as parent of `id_pointer`. */
    if (*id_pointer != nullptr) {
      if (!BLI_ghash_ensure_p(
              bmain_relations->relations_from_pointers, *id_pointer, (void ***)&entry_p))
      {
        *entry_p = static_cast<MainIDRelationsEntry *>(MEM_callocN(sizeof(**entry_p), __func__));
        (*entry_p)->session_uid = (*id_pointer)->session_uid;
      }
      else {
        BLI_assert((*entry_p)->session_uid == (*id_pointer)->session_uid);
      }
      MainIDRelationsEntryItem *from_id_entry = static_cast<MainIDRelationsEntryItem *>(
          BLI_mempool_alloc(bmain_relations->entry_items_pool));
      from_id_entry->next = (*entry_p)->from_ids;
      from_id_entry->id_pointer.from = self_id;
      from_id_entry->session_uid = self_id->session_uid;
      from_id_entry->usage_flag = cb_flag;
      (*entry_p)->from_ids = from_id_entry;
    }
  }

  return IDWALK_RET_NOP;
}

void BKE_main_relations_create(Main *bmain, const short flag)
{
  if (bmain->relations != nullptr) {
    BKE_main_relations_free(bmain);
  }

  bmain->relations = static_cast<MainIDRelations *>(
      MEM_mallocN(sizeof(*bmain->relations), __func__));
  bmain->relations->relations_from_pointers = BLI_ghash_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
  bmain->relations->entry_items_pool = BLI_mempool_create(
      sizeof(MainIDRelationsEntryItem), 128, 128, BLI_MEMPOOL_NOP);

  bmain->relations->flag = flag;

  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    const int idwalk_flag = IDWALK_READONLY |
                            ((flag & MAINIDRELATIONS_INCLUDE_UI) != 0 ? IDWALK_INCLUDE_UI : 0);

    /* Ensure all IDs do have an entry, even if they are not connected to any other. */
    MainIDRelationsEntry **entry_p;
    if (!BLI_ghash_ensure_p(bmain->relations->relations_from_pointers, id, (void ***)&entry_p)) {
      *entry_p = static_cast<MainIDRelationsEntry *>(MEM_callocN(sizeof(**entry_p), __func__));
      (*entry_p)->session_uid = id->session_uid;
    }
    else {
      BLI_assert((*entry_p)->session_uid == id->session_uid);
    }

    BKE_library_foreach_ID_link(
        nullptr, id, main_relations_create_idlink_cb, bmain->relations, idwalk_flag);
  }
  FOREACH_MAIN_ID_END;
}

void BKE_main_relations_free(Main *bmain)
{
  if (bmain->relations != nullptr) {
    if (bmain->relations->relations_from_pointers != nullptr) {
      BLI_ghash_free(bmain->relations->relations_from_pointers, nullptr, MEM_freeN);
    }
    BLI_mempool_destroy(bmain->relations->entry_items_pool);
    MEM_freeN(bmain->relations);
    bmain->relations = nullptr;
  }
}

void BKE_main_relations_tag_set(Main *bmain, const eMainIDRelationsEntryTags tag, const bool value)
{
  if (bmain->relations == nullptr) {
    return;
  }

  GHashIterator *gh_iter;
  for (gh_iter = BLI_ghashIterator_new(bmain->relations->relations_from_pointers);
       !BLI_ghashIterator_done(gh_iter);
       BLI_ghashIterator_step(gh_iter))
  {
    MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
        BLI_ghashIterator_getValue(gh_iter));
    if (value) {
      entry->tags |= tag;
    }
    else {
      entry->tags &= ~tag;
    }
  }
  BLI_ghashIterator_free(gh_iter);
}

GSet *BKE_main_gset_create(Main *bmain, GSet *gset)
{
  if (gset == nullptr) {
    gset = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
  }

  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    BLI_gset_add(gset, id);
  }
  FOREACH_MAIN_ID_END;
  return gset;
}

/* Utils for ID's library weak reference API. */
struct LibWeakRefKey {
  char filepath[FILE_MAX];
  char id_name[MAX_ID_NAME];
};

static LibWeakRefKey *lib_weak_key_create(LibWeakRefKey *key,
                                          const char *lib_path,
                                          const char *id_name)
{
  if (key == nullptr) {
    key = static_cast<LibWeakRefKey *>(MEM_mallocN(sizeof(*key), __func__));
  }
  STRNCPY(key->filepath, lib_path);
  STRNCPY(key->id_name, id_name);
  return key;
}

static uint lib_weak_key_hash(const void *ptr)
{
  const LibWeakRefKey *string_pair = static_cast<const LibWeakRefKey *>(ptr);
  uint hash = BLI_ghashutil_strhash_p_murmur(string_pair->filepath);
  return hash ^ BLI_ghashutil_strhash_p_murmur(string_pair->id_name);
}

static bool lib_weak_key_cmp(const void *a, const void *b)
{
  const LibWeakRefKey *string_pair_a = static_cast<const LibWeakRefKey *>(a);
  const LibWeakRefKey *string_pair_b = static_cast<const LibWeakRefKey *>(b);

  return !(STREQ(string_pair_a->filepath, string_pair_b->filepath) &&
           STREQ(string_pair_a->id_name, string_pair_b->id_name));
}

GHash *BKE_main_library_weak_reference_create(Main *bmain)
{
  GHash *library_weak_reference_mapping = BLI_ghash_new(
      lib_weak_key_hash, lib_weak_key_cmp, __func__);

  ListBase *lb;
  FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
    ID *id_iter = static_cast<ID *>(lb->first);
    if (id_iter == nullptr) {
      continue;
    }
    if (!BKE_idtype_idcode_append_is_reusable(GS(id_iter->name))) {
      continue;
    }
    BLI_assert(BKE_idtype_idcode_is_linkable(GS(id_iter->name)));

    FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id_iter) {
      if (id_iter->library_weak_reference == nullptr) {
        continue;
      }
      LibWeakRefKey *key = lib_weak_key_create(nullptr,
                                               id_iter->library_weak_reference->library_filepath,
                                               id_iter->library_weak_reference->library_id_name);
      BLI_ghash_insert(library_weak_reference_mapping, key, id_iter);
    }
    FOREACH_MAIN_LISTBASE_ID_END;
  }
  FOREACH_MAIN_LISTBASE_END;

  return library_weak_reference_mapping;
}

void BKE_main_library_weak_reference_destroy(GHash *library_weak_reference_mapping)
{
  BLI_ghash_free(library_weak_reference_mapping, MEM_freeN, nullptr);
}

ID *BKE_main_library_weak_reference_search_item(GHash *library_weak_reference_mapping,
                                                const char *library_filepath,
                                                const char *library_id_name)
{
  LibWeakRefKey key;
  lib_weak_key_create(&key, library_filepath, library_id_name);
  return (ID *)BLI_ghash_lookup(library_weak_reference_mapping, &key);
}

void BKE_main_library_weak_reference_add_item(GHash *library_weak_reference_mapping,
                                              const char *library_filepath,
                                              const char *library_id_name,
                                              ID *new_id)
{
  BLI_assert(GS(library_id_name) == GS(new_id->name));
  BLI_assert(new_id->library_weak_reference == nullptr);
  BLI_assert(BKE_idtype_idcode_append_is_reusable(GS(new_id->name)));

  new_id->library_weak_reference = static_cast<LibraryWeakReference *>(
      MEM_mallocN(sizeof(*(new_id->library_weak_reference)), __func__));

  LibWeakRefKey *key = lib_weak_key_create(nullptr, library_filepath, library_id_name);
  void **id_p;
  const bool already_exist_in_mapping = BLI_ghash_ensure_p(
      library_weak_reference_mapping, key, &id_p);
  BLI_assert(!already_exist_in_mapping);
  UNUSED_VARS_NDEBUG(already_exist_in_mapping);

  STRNCPY(new_id->library_weak_reference->library_filepath, library_filepath);
  STRNCPY(new_id->library_weak_reference->library_id_name, library_id_name);
  *id_p = new_id;
}

void BKE_main_library_weak_reference_update_item(GHash *library_weak_reference_mapping,
                                                 const char *library_filepath,
                                                 const char *library_id_name,
                                                 ID *old_id,
                                                 ID *new_id)
{
  BLI_assert(GS(library_id_name) == GS(old_id->name));
  BLI_assert(GS(library_id_name) == GS(new_id->name));
  BLI_assert(old_id->library_weak_reference != nullptr);
  BLI_assert(new_id->library_weak_reference == nullptr);
  BLI_assert(STREQ(old_id->library_weak_reference->library_filepath, library_filepath));
  BLI_assert(STREQ(old_id->library_weak_reference->library_id_name, library_id_name));

  LibWeakRefKey key;
  lib_weak_key_create(&key, library_filepath, library_id_name);
  void **id_p = BLI_ghash_lookup_p(library_weak_reference_mapping, &key);
  BLI_assert(id_p != nullptr && *id_p == old_id);

  new_id->library_weak_reference = old_id->library_weak_reference;
  old_id->library_weak_reference = nullptr;
  *id_p = new_id;
}

void BKE_main_library_weak_reference_remove_item(GHash *library_weak_reference_mapping,
                                                 const char *library_filepath,
                                                 const char *library_id_name,
                                                 ID *old_id)
{
  BLI_assert(GS(library_id_name) == GS(old_id->name));
  BLI_assert(old_id->library_weak_reference != nullptr);

  LibWeakRefKey key;
  lib_weak_key_create(&key, library_filepath, library_id_name);

  BLI_assert(BLI_ghash_lookup(library_weak_reference_mapping, &key) == old_id);
  BLI_ghash_remove(library_weak_reference_mapping, &key, MEM_freeN, nullptr);

  MEM_SAFE_FREE(old_id->library_weak_reference);
}

BlendThumbnail *BKE_main_thumbnail_from_imbuf(Main *bmain, ImBuf *img)
{
  BlendThumbnail *data = nullptr;

  if (bmain) {
    MEM_SAFE_FREE(bmain->blen_thumb);
  }

  if (img) {
    const size_t data_size = BLEN_THUMB_MEMSIZE(img->x, img->y);
    data = static_cast<BlendThumbnail *>(MEM_mallocN(data_size, __func__));

    IMB_rect_from_float(img); /* Just in case... */
    data->width = img->x;
    data->height = img->y;
    memcpy(data->rect, img->byte_buffer.data, data_size - sizeof(*data));
  }

  if (bmain) {
    bmain->blen_thumb = data;
  }
  return data;
}

ImBuf *BKE_main_thumbnail_to_imbuf(Main *bmain, BlendThumbnail *data)
{
  ImBuf *img = nullptr;

  if (!data && bmain) {
    data = bmain->blen_thumb;
  }

  if (data) {
    img = IMB_allocFromBuffer(
        (const uint8_t *)data->rect, nullptr, uint(data->width), uint(data->height), 4);
  }

  return img;
}

void BKE_main_thumbnail_create(Main *bmain)
{
  MEM_SAFE_FREE(bmain->blen_thumb);

  bmain->blen_thumb = static_cast<BlendThumbnail *>(
      MEM_callocN(BLEN_THUMB_MEMSIZE(BLEN_THUMB_SIZE, BLEN_THUMB_SIZE), __func__));
  bmain->blen_thumb->width = BLEN_THUMB_SIZE;
  bmain->blen_thumb->height = BLEN_THUMB_SIZE;
}

const char *BKE_main_blendfile_path(const Main *bmain)
{
  return bmain->filepath;
}

const char *BKE_main_blendfile_path_from_global()
{
  return BKE_main_blendfile_path(G_MAIN);
}

ListBase *which_libbase(Main *bmain, short type)
{
  switch ((ID_Type)type) {
    case ID_SCE:
      return &(bmain->scenes);
    case ID_LI:
      return &(bmain->libraries);
    case ID_OB:
      return &(bmain->objects);
    case ID_ME:
      return &(bmain->meshes);
    case ID_CU_LEGACY:
      return &(bmain->curves);
    case ID_MB:
      return &(bmain->metaballs);
    case ID_MA:
      return &(bmain->materials);
    case ID_TE:
      return &(bmain->textures);
    case ID_IM:
      return &(bmain->images);
    case ID_LT:
      return &(bmain->lattices);
    case ID_LA:
      return &(bmain->lights);
    case ID_CA:
      return &(bmain->cameras);
    case ID_IP:
      return &(bmain->ipo);
    case ID_KE:
      return &(bmain->shapekeys);
    case ID_WO:
      return &(bmain->worlds);
    case ID_SCR:
      return &(bmain->screens);
    case ID_VF:
      return &(bmain->fonts);
    case ID_TXT:
      return &(bmain->texts);
    case ID_SPK:
      return &(bmain->speakers);
    case ID_LP:
      return &(bmain->lightprobes);
    case ID_SO:
      return &(bmain->sounds);
    case ID_GR:
      return &(bmain->collections);
    case ID_AR:
      return &(bmain->armatures);
    case ID_AC:
      return &(bmain->actions);
    case ID_AN:
      return &(bmain->animations);
    case ID_NT:
      return &(bmain->nodetrees);
    case ID_BR:
      return &(bmain->brushes);
    case ID_PA:
      return &(bmain->particles);
    case ID_WM:
      return &(bmain->wm);
    case ID_GD_LEGACY:
      return &(bmain->gpencils);
    case ID_GP:
      return &(bmain->grease_pencils);
    case ID_MC:
      return &(bmain->movieclips);
    case ID_MSK:
      return &(bmain->masks);
    case ID_LS:
      return &(bmain->linestyles);
    case ID_PAL:
      return &(bmain->palettes);
    case ID_PC:
      return &(bmain->paintcurves);
    case ID_CF:
      return &(bmain->cachefiles);
    case ID_WS:
      return &(bmain->workspaces);
    case ID_CV:
      return &(bmain->hair_curves);
    case ID_PT:
      return &(bmain->pointclouds);
    case ID_VO:
      return &(bmain->volumes);
  }
  return nullptr;
}

int set_listbasepointers(Main *bmain, ListBase *lb[/*INDEX_ID_MAX*/])
{
  /* Libraries may be accessed from pretty much any other ID. */
  lb[INDEX_ID_LI] = &(bmain->libraries);

  lb[INDEX_ID_IP] = &(bmain->ipo);

  /* Moved here to avoid problems when freeing with animato (aligorith). */
  lb[INDEX_ID_AC] = &(bmain->actions);
  lb[INDEX_ID_AN] = &(bmain->animations);

  lb[INDEX_ID_KE] = &(bmain->shapekeys);

  /* Referenced by gpencil, so needs to be before that to avoid crashes. */
  lb[INDEX_ID_PAL] = &(bmain->palettes);

  /* Referenced by nodes, objects, view, scene etc, before to free after. */
  lb[INDEX_ID_GD_LEGACY] = &(bmain->gpencils);
  lb[INDEX_ID_GP] = &(bmain->grease_pencils);

  lb[INDEX_ID_NT] = &(bmain->nodetrees);
  lb[INDEX_ID_IM] = &(bmain->images);
  lb[INDEX_ID_TE] = &(bmain->textures);
  lb[INDEX_ID_MA] = &(bmain->materials);
  lb[INDEX_ID_VF] = &(bmain->fonts);

  /* Important!: When adding a new object type,
   * the specific data should be inserted here. */

  lb[INDEX_ID_AR] = &(bmain->armatures);

  lb[INDEX_ID_CF] = &(bmain->cachefiles);
  lb[INDEX_ID_ME] = &(bmain->meshes);
  lb[INDEX_ID_CU_LEGACY] = &(bmain->curves);
  lb[INDEX_ID_MB] = &(bmain->metaballs);
  lb[INDEX_ID_CV] = &(bmain->hair_curves);
  lb[INDEX_ID_PT] = &(bmain->pointclouds);
  lb[INDEX_ID_VO] = &(bmain->volumes);

  lb[INDEX_ID_LT] = &(bmain->lattices);
  lb[INDEX_ID_LA] = &(bmain->lights);
  lb[INDEX_ID_CA] = &(bmain->cameras);

  lb[INDEX_ID_TXT] = &(bmain->texts);
  lb[INDEX_ID_SO] = &(bmain->sounds);
  lb[INDEX_ID_GR] = &(bmain->collections);
  lb[INDEX_ID_PAL] = &(bmain->palettes);
  lb[INDEX_ID_PC] = &(bmain->paintcurves);
  lb[INDEX_ID_BR] = &(bmain->brushes);
  lb[INDEX_ID_PA] = &(bmain->particles);
  lb[INDEX_ID_SPK] = &(bmain->speakers);
  lb[INDEX_ID_LP] = &(bmain->lightprobes);

  lb[INDEX_ID_WO] = &(bmain->worlds);
  lb[INDEX_ID_MC] = &(bmain->movieclips);
  lb[INDEX_ID_SCR] = &(bmain->screens);
  lb[INDEX_ID_OB] = &(bmain->objects);
  lb[INDEX_ID_LS] = &(bmain->linestyles); /* referenced by scenes */
  lb[INDEX_ID_SCE] = &(bmain->scenes);
  lb[INDEX_ID_WS] = &(bmain->workspaces); /* before wm, so it's freed after it! */
  lb[INDEX_ID_WM] = &(bmain->wm);
  lb[INDEX_ID_MSK] = &(bmain->masks);

  lb[INDEX_ID_NULL] = nullptr;

  return (INDEX_ID_MAX - 1);
}

Main *BKE_main_from_id(Main *global_main, const ID *id, const bool verify)
{
  if (id == nullptr || (id->tag & LIB_TAG_NO_MAIN)) {
    return nullptr;
  }
  if (id->tag & LIB_TAG_ASSET_MAIN) {
    return BKE_asset_weak_reference_main(id);
  }

  if (verify) {
    /* This is rather expensive, so don't do by default and assume valid input. */
    if (BLI_findindex(which_libbase(global_main, GS(id->name)), id) == -1) {
      return nullptr;
    }
  }
  else {
    /* Debug assert, especially for places that pass in G_MAIN. */
    #ifndef NDEBUG
    if (id->flag & LIB_EMBEDDED_DATA) {
      const ID *id_owner = BKE_id_owner_get(const_cast<ID*>(id));
      BLI_assert(BLI_findindex(which_libbase(global_main, GS(id_owner->name)), id_owner) != -1);
    }
    else {
      BLI_assert(BLI_findindex(which_libbase(global_main, GS(id->name)), id) != -1);
    }
    #endif
  }

  return global_main;
}
