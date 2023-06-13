/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Contains management of #Main database itself.
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_mempool.h"
#include "BLI_threads.h"

#include "DNA_ID.h"

#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_main_idmap.h"
#include "BKE_main_namemap.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

Main *BKE_main_new(void)
{
  Main *bmain = MEM_callocN(sizeof(Main), "new main");
  bmain->lock = MEM_mallocN(sizeof(SpinLock), "main lock");
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

    for (id = lb->first; id != NULL; id = id_next) {
      id_next = id->next;
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

  BLI_spin_end((SpinLock *)mainvar->lock);
  MEM_freeN(mainvar->lock);
  MEM_freeN(mainvar);
}

bool BKE_main_is_empty(Main *bmain)
{
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    return false;
  }
  FOREACH_MAIN_ID_END;
  return true;
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
  MainIDRelations *bmain_relations = cb_data->user_data;
  ID *self_id = cb_data->self_id;
  ID **id_pointer = cb_data->id_pointer;
  const int cb_flag = cb_data->cb_flag;

  if (*id_pointer) {
    MainIDRelationsEntry **entry_p;

    /* Add `id_pointer` as child of `self_id`. */
    {
      if (!BLI_ghash_ensure_p(
              bmain_relations->relations_from_pointers, self_id, (void ***)&entry_p)) {
        *entry_p = MEM_callocN(sizeof(**entry_p), __func__);
        (*entry_p)->session_uuid = self_id->session_uuid;
      }
      else {
        BLI_assert((*entry_p)->session_uuid == self_id->session_uuid);
      }
      MainIDRelationsEntryItem *to_id_entry = BLI_mempool_alloc(bmain_relations->entry_items_pool);
      to_id_entry->next = (*entry_p)->to_ids;
      to_id_entry->id_pointer.to = id_pointer;
      to_id_entry->session_uuid = (*id_pointer != NULL) ? (*id_pointer)->session_uuid :
                                                          MAIN_ID_SESSION_UUID_UNSET;
      to_id_entry->usage_flag = cb_flag;
      (*entry_p)->to_ids = to_id_entry;
    }

    /* Add `self_id` as parent of `id_pointer`. */
    if (*id_pointer != NULL) {
      if (!BLI_ghash_ensure_p(
              bmain_relations->relations_from_pointers, *id_pointer, (void ***)&entry_p)) {
        *entry_p = MEM_callocN(sizeof(**entry_p), __func__);
        (*entry_p)->session_uuid = (*id_pointer)->session_uuid;
      }
      else {
        BLI_assert((*entry_p)->session_uuid == (*id_pointer)->session_uuid);
      }
      MainIDRelationsEntryItem *from_id_entry = BLI_mempool_alloc(
          bmain_relations->entry_items_pool);
      from_id_entry->next = (*entry_p)->from_ids;
      from_id_entry->id_pointer.from = self_id;
      from_id_entry->session_uuid = self_id->session_uuid;
      from_id_entry->usage_flag = cb_flag;
      (*entry_p)->from_ids = from_id_entry;
    }
  }

  return IDWALK_RET_NOP;
}

void BKE_main_relations_create(Main *bmain, const short flag)
{
  if (bmain->relations != NULL) {
    BKE_main_relations_free(bmain);
  }

  bmain->relations = MEM_mallocN(sizeof(*bmain->relations), __func__);
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
      *entry_p = MEM_callocN(sizeof(**entry_p), __func__);
      (*entry_p)->session_uuid = id->session_uuid;
    }
    else {
      BLI_assert((*entry_p)->session_uuid == id->session_uuid);
    }

    BKE_library_foreach_ID_link(
        NULL, id, main_relations_create_idlink_cb, bmain->relations, idwalk_flag);
  }
  FOREACH_MAIN_ID_END;
}

void BKE_main_relations_free(Main *bmain)
{
  if (bmain->relations != NULL) {
    if (bmain->relations->relations_from_pointers != NULL) {
      BLI_ghash_free(bmain->relations->relations_from_pointers, NULL, MEM_freeN);
    }
    BLI_mempool_destroy(bmain->relations->entry_items_pool);
    MEM_freeN(bmain->relations);
    bmain->relations = NULL;
  }
}

void BKE_main_relations_tag_set(Main *bmain, const eMainIDRelationsEntryTags tag, const bool value)
{
  if (bmain->relations == NULL) {
    return;
  }

  GHashIterator *gh_iter;
  for (gh_iter = BLI_ghashIterator_new(bmain->relations->relations_from_pointers);
       !BLI_ghashIterator_done(gh_iter);
       BLI_ghashIterator_step(gh_iter))
  {
    MainIDRelationsEntry *entry = BLI_ghashIterator_getValue(gh_iter);
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
  if (gset == NULL) {
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
typedef struct LibWeakRefKey {
  char filepath[FILE_MAX];
  char id_name[MAX_ID_NAME];
} LibWeakRefKey;

static LibWeakRefKey *lib_weak_key_create(LibWeakRefKey *key,
                                          const char *lib_path,
                                          const char *id_name)
{
  if (key == NULL) {
    key = MEM_mallocN(sizeof(*key), __func__);
  }
  STRNCPY(key->filepath, lib_path);
  STRNCPY(key->id_name, id_name);
  return key;
}

static uint lib_weak_key_hash(const void *ptr)
{
  const LibWeakRefKey *string_pair = ptr;
  uint hash = BLI_ghashutil_strhash_p_murmur(string_pair->filepath);
  return hash ^ BLI_ghashutil_strhash_p_murmur(string_pair->id_name);
}

static bool lib_weak_key_cmp(const void *a, const void *b)
{
  const LibWeakRefKey *string_pair_a = a;
  const LibWeakRefKey *string_pair_b = b;

  return !(STREQ(string_pair_a->filepath, string_pair_b->filepath) &&
           STREQ(string_pair_a->id_name, string_pair_b->id_name));
}

GHash *BKE_main_library_weak_reference_create(Main *bmain)
{
  GHash *library_weak_reference_mapping = BLI_ghash_new(
      lib_weak_key_hash, lib_weak_key_cmp, __func__);

  ListBase *lb;
  FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
    ID *id_iter = lb->first;
    if (id_iter == NULL) {
      continue;
    }
    if (!BKE_idtype_idcode_append_is_reusable(GS(id_iter->name))) {
      continue;
    }
    BLI_assert(BKE_idtype_idcode_is_linkable(GS(id_iter->name)));

    FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id_iter) {
      if (id_iter->library_weak_reference == NULL) {
        continue;
      }
      LibWeakRefKey *key = lib_weak_key_create(NULL,
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
  BLI_ghash_free(library_weak_reference_mapping, MEM_freeN, NULL);
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
  BLI_assert(new_id->library_weak_reference == NULL);
  BLI_assert(BKE_idtype_idcode_append_is_reusable(GS(new_id->name)));

  new_id->library_weak_reference = MEM_mallocN(sizeof(*(new_id->library_weak_reference)),
                                               __func__);

  LibWeakRefKey *key = lib_weak_key_create(NULL, library_filepath, library_id_name);
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
  BLI_assert(old_id->library_weak_reference != NULL);
  BLI_assert(new_id->library_weak_reference == NULL);
  BLI_assert(STREQ(old_id->library_weak_reference->library_filepath, library_filepath));
  BLI_assert(STREQ(old_id->library_weak_reference->library_id_name, library_id_name));

  LibWeakRefKey key;
  lib_weak_key_create(&key, library_filepath, library_id_name);
  void **id_p = BLI_ghash_lookup_p(library_weak_reference_mapping, &key);
  BLI_assert(id_p != NULL && *id_p == old_id);

  new_id->library_weak_reference = old_id->library_weak_reference;
  old_id->library_weak_reference = NULL;
  *id_p = new_id;
}

void BKE_main_library_weak_reference_remove_item(GHash *library_weak_reference_mapping,
                                                 const char *library_filepath,
                                                 const char *library_id_name,
                                                 ID *old_id)
{
  BLI_assert(GS(library_id_name) == GS(old_id->name));
  BLI_assert(old_id->library_weak_reference != NULL);

  LibWeakRefKey key;
  lib_weak_key_create(&key, library_filepath, library_id_name);

  BLI_assert(BLI_ghash_lookup(library_weak_reference_mapping, &key) == old_id);
  BLI_ghash_remove(library_weak_reference_mapping, &key, MEM_freeN, NULL);

  MEM_SAFE_FREE(old_id->library_weak_reference);
}

BlendThumbnail *BKE_main_thumbnail_from_imbuf(Main *bmain, ImBuf *img)
{
  BlendThumbnail *data = NULL;

  if (bmain) {
    MEM_SAFE_FREE(bmain->blen_thumb);
  }

  if (img) {
    const size_t data_size = BLEN_THUMB_MEMSIZE(img->x, img->y);
    data = MEM_mallocN(data_size, __func__);

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
  ImBuf *img = NULL;

  if (!data && bmain) {
    data = bmain->blen_thumb;
  }

  if (data) {
    img = IMB_allocFromBuffer(
        (const uint8_t *)data->rect, NULL, (uint)data->width, (uint)data->height, 4);
  }

  return img;
}

void BKE_main_thumbnail_create(Main *bmain)
{
  MEM_SAFE_FREE(bmain->blen_thumb);

  bmain->blen_thumb = MEM_callocN(BLEN_THUMB_MEMSIZE(BLEN_THUMB_SIZE, BLEN_THUMB_SIZE), __func__);
  bmain->blen_thumb->width = BLEN_THUMB_SIZE;
  bmain->blen_thumb->height = BLEN_THUMB_SIZE;
}

const char *BKE_main_blendfile_path(const Main *bmain)
{
  return bmain->filepath;
}

const char *BKE_main_blendfile_path_from_global(void)
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
    case ID_SIM:
      return &(bmain->simulations);
  }
  return NULL;
}

int set_listbasepointers(Main *bmain, ListBase *lb[/*INDEX_ID_MAX*/])
{
  /* Libraries may be accessed from pretty much any other ID. */
  lb[INDEX_ID_LI] = &(bmain->libraries);

  lb[INDEX_ID_IP] = &(bmain->ipo);

  /* Moved here to avoid problems when freeing with animato (aligorith). */
  lb[INDEX_ID_AC] = &(bmain->actions);

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
  lb[INDEX_ID_SIM] = &(bmain->simulations);

  lb[INDEX_ID_NULL] = NULL;

  return (INDEX_ID_MAX - 1);
}
