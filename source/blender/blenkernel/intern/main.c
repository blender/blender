/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

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
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_main.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

Main *BKE_main_new(void)
{
  Main *bmain = MEM_callocN(sizeof(Main), "new main");
  bmain->lock = MEM_mallocN(sizeof(SpinLock), "main lock");
  BLI_spin_init((SpinLock *)bmain->lock);
  return bmain;
}

void BKE_main_free(Main *mainvar)
{
  /* also call when reading a file, erase all, etc */
  ListBase *lbarray[MAX_LIBARRAY];
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
      /* errors freeing ID's can be hard to track down,
       * enable this so valgrind will give the line number in its error log */
      switch (a) {
        case 0:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 1:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 2:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 3:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 4:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 5:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 6:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 7:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 8:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 9:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 10:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 11:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 12:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 13:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 14:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 15:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 16:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 17:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 18:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 19:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 20:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 21:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 22:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 23:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 24:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 25:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 26:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 27:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 28:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 29:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 30:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 31:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 32:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 33:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        case 34:
          BKE_id_free_ex(mainvar, id, free_flag, false);
          break;
        default:
          BLI_assert(0);
          break;
      }
#endif
    }
    BLI_listbase_clear(lb);
  }

  if (mainvar->relations) {
    BKE_main_relations_free(mainvar);
  }

  BLI_spin_end((SpinLock *)mainvar->lock);
  MEM_freeN(mainvar->lock);
  MEM_freeN(mainvar);
}

void BKE_main_lock(struct Main *bmain)
{
  BLI_spin_lock((SpinLock *)bmain->lock);
}

void BKE_main_unlock(struct Main *bmain)
{
  BLI_spin_unlock((SpinLock *)bmain->lock);
}

static int main_relations_create_idlink_cb(void *user_data,
                                           ID *id_self,
                                           ID **id_pointer,
                                           int cb_flag)
{
  MainIDRelations *rel = user_data;

  if (*id_pointer) {
    MainIDRelationsEntry *entry, **entry_p;

    entry = BLI_mempool_alloc(rel->entry_pool);
    if (BLI_ghash_ensure_p(rel->id_user_to_used, id_self, (void ***)&entry_p)) {
      entry->next = *entry_p;
    }
    else {
      entry->next = NULL;
    }
    entry->id_pointer = id_pointer;
    entry->usage_flag = cb_flag;
    *entry_p = entry;

    entry = BLI_mempool_alloc(rel->entry_pool);
    if (BLI_ghash_ensure_p(rel->id_used_to_user, *id_pointer, (void ***)&entry_p)) {
      entry->next = *entry_p;
    }
    else {
      entry->next = NULL;
    }
    entry->id_pointer = (ID **)id_self;
    entry->usage_flag = cb_flag;
    *entry_p = entry;
  }

  return IDWALK_RET_NOP;
}

/** Generate the mappings between used IDs and their users, and vice-versa. */
void BKE_main_relations_create(Main *bmain)
{
  if (bmain->relations != NULL) {
    BKE_main_relations_free(bmain);
  }

  bmain->relations = MEM_mallocN(sizeof(*bmain->relations), __func__);
  bmain->relations->id_used_to_user = BLI_ghash_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
  bmain->relations->id_user_to_used = BLI_ghash_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
  bmain->relations->entry_pool = BLI_mempool_create(
      sizeof(MainIDRelationsEntry), 128, 128, BLI_MEMPOOL_NOP);

  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    BKE_library_foreach_ID_link(
        NULL, id, main_relations_create_idlink_cb, bmain->relations, IDWALK_READONLY);
  }
  FOREACH_MAIN_ID_END;
}

void BKE_main_relations_free(Main *bmain)
{
  if (bmain->relations) {
    if (bmain->relations->id_used_to_user) {
      BLI_ghash_free(bmain->relations->id_used_to_user, NULL, NULL);
    }
    if (bmain->relations->id_user_to_used) {
      BLI_ghash_free(bmain->relations->id_user_to_used, NULL, NULL);
    }
    BLI_mempool_destroy(bmain->relations->entry_pool);
    MEM_freeN(bmain->relations);
    bmain->relations = NULL;
  }
}

/**
 * Create a GSet storing all IDs present in given \a bmain, by their pointers.
 *
 * \param gset: If not NULL, given GSet will be extended with IDs from given \a bmain,
 * instead of creating a new one.
 */
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

/**
 * Generates a raw .blend file thumbnail data from given image.
 *
 * \param bmain: If not NULL, also store generated data in this Main.
 * \param img: ImBuf image to generate thumbnail data from.
 * \return The generated .blend file raw thumbnail data.
 */
BlendThumbnail *BKE_main_thumbnail_from_imbuf(Main *bmain, ImBuf *img)
{
  BlendThumbnail *data = NULL;

  if (bmain) {
    MEM_SAFE_FREE(bmain->blen_thumb);
  }

  if (img) {
    const size_t sz = BLEN_THUMB_MEMSIZE(img->x, img->y);
    data = MEM_mallocN(sz, __func__);

    IMB_rect_from_float(img); /* Just in case... */
    data->width = img->x;
    data->height = img->y;
    memcpy(data->rect, img->rect, sz - sizeof(*data));
  }

  if (bmain) {
    bmain->blen_thumb = data;
  }
  return data;
}

/**
 * Generates an image from raw .blend file thumbnail \a data.
 *
 * \param bmain: Use this bmain->blen_thumb data if given \a data is NULL.
 * \param data: Raw .blend file thumbnail data.
 * \return An ImBuf from given data, or NULL if invalid.
 */
ImBuf *BKE_main_thumbnail_to_imbuf(Main *bmain, BlendThumbnail *data)
{
  ImBuf *img = NULL;

  if (!data && bmain) {
    data = bmain->blen_thumb;
  }

  if (data) {
    /* Note: we cannot use IMB_allocFromBuffer(), since it tries to dupalloc passed buffer,
     *       which will fail here (we do not want to pass the first two ints!). */
    img = IMB_allocImBuf(
        (unsigned int)data->width, (unsigned int)data->height, 32, IB_rect | IB_metadata);
    memcpy(img->rect, data->rect, BLEN_THUMB_MEMSIZE(data->width, data->height) - sizeof(*data));
  }

  return img;
}

/**
 * Generates an empty (black) thumbnail for given Main.
 */
void BKE_main_thumbnail_create(struct Main *bmain)
{
  MEM_SAFE_FREE(bmain->blen_thumb);

  bmain->blen_thumb = MEM_callocN(BLEN_THUMB_MEMSIZE(BLEN_THUMB_SIZE, BLEN_THUMB_SIZE), __func__);
  bmain->blen_thumb->width = BLEN_THUMB_SIZE;
  bmain->blen_thumb->height = BLEN_THUMB_SIZE;
}

/**
 * Return filepath of given \a main.
 */
const char *BKE_main_blendfile_path(const Main *bmain)
{
  return bmain->name;
}

/**
 * Return filepath of global main #G_MAIN.
 *
 * \warning Usage is not recommended,
 * you should always try to get a valid Main pointer from context...
 */
const char *BKE_main_blendfile_path_from_global(void)
{
  return BKE_main_blendfile_path(G_MAIN);
}

/**
 * \return A pointer to the \a ListBase of given \a bmain for requested \a type ID type.
 */
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
    case ID_CU:
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
    case ID_GD:
      return &(bmain->gpencils);
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
  }
  return NULL;
}

/**
 * puts into array *lb pointers to all the #ListBase structs in main,
 * and returns the number of them as the function result. This is useful for
 * generic traversal of all the blocks in a Main (by traversing all the
 * lists in turn), without worrying about block types.
 *
 * \note #MAX_LIBARRAY define should match this code */
int set_listbasepointers(Main *bmain, ListBase **lb)
{
  /* BACKWARDS! also watch order of free-ing! (mesh<->mat), first items freed last.
   * This is important because freeing data decreases usercounts of other datablocks,
   * if this data is its self freed it can crash. */
  lb[INDEX_ID_LI] = &(
      bmain->libraries); /* Libraries may be accessed from pretty much any other ID... */
  lb[INDEX_ID_IP] = &(bmain->ipo);
  lb[INDEX_ID_AC] = &(
      bmain->actions); /* moved here to avoid problems when freeing with animato (aligorith) */
  lb[INDEX_ID_KE] = &(bmain->shapekeys);
  lb[INDEX_ID_PAL] = &(
      bmain->palettes); /* referenced by gpencil, so needs to be before that to avoid crashes */
  lb[INDEX_ID_GD] = &(
      bmain->gpencils); /* referenced by nodes, objects, view, scene etc, before to free after. */
  lb[INDEX_ID_NT] = &(bmain->nodetrees);
  lb[INDEX_ID_IM] = &(bmain->images);
  lb[INDEX_ID_TE] = &(bmain->textures);
  lb[INDEX_ID_MA] = &(bmain->materials);
  lb[INDEX_ID_VF] = &(bmain->fonts);

  /* Important!: When adding a new object type,
   * the specific data should be inserted here
   */

  lb[INDEX_ID_AR] = &(bmain->armatures);

  lb[INDEX_ID_CF] = &(bmain->cachefiles);
  lb[INDEX_ID_ME] = &(bmain->meshes);
  lb[INDEX_ID_CU] = &(bmain->curves);
  lb[INDEX_ID_MB] = &(bmain->metaballs);

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

  lb[INDEX_ID_NULL] = NULL;

  return (MAX_LIBARRAY - 1);
}
