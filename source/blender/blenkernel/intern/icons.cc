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
 * The Original Code is Copyright (C) 2006-2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_collection_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_linklist_lockfree.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_global.h" /* only for G.background test */
#include "BKE_icons.h"
#include "BKE_studiolight.h"

#include "BLI_sys_types.h" /* for intptr_t support */

#include "GPU_texture.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "BLO_read_write.h"

#include "atomic_ops.h"

/**
 * Only allow non-managed icons to be removed (by Python for eg).
 * Previews & ID's have their own functions to remove icons.
 */
enum {
  ICON_FLAG_MANAGED = (1 << 0),
};

/* GLOBALS */

static CLG_LogRef LOG = {"bke.icons"};

/* Protected by gIconMutex. */
static GHash *gIcons = nullptr;

/* Protected by gIconMutex. */
static int gNextIconId = 1;

/* Protected by gIconMutex. */
static int gFirstIconId = 1;

static std::mutex gIconMutex;

/* Not mutex-protected! */
static GHash *gCachedPreviews = nullptr;

/* Queue of icons for deferred deletion. */
struct DeferredIconDeleteNode {
  struct DeferredIconDeleteNode *next;
  int icon_id;
};
/* Protected by gIconMutex. */
static LockfreeLinkList g_icon_delete_queue;

static void icon_free(void *val)
{
  Icon *icon = (Icon *)val;

  if (icon) {
    if (icon->obj_type == ICON_DATA_GEOM) {
      struct Icon_Geom *obj = (struct Icon_Geom *)icon->obj;
      if (obj->mem) {
        /* coords & colors are part of this memory. */
        MEM_freeN((void *)obj->mem);
      }
      else {
        MEM_freeN((void *)obj->coords);
        MEM_freeN((void *)obj->colors);
      }
      MEM_freeN(icon->obj);
    }

    if (icon->drawinfo_free) {
      icon->drawinfo_free(icon->drawinfo);
    }
    else if (icon->drawinfo) {
      MEM_freeN(icon->drawinfo);
    }
    MEM_freeN(icon);
  }
}

static void icon_free_data(int icon_id, Icon *icon)
{
  if (icon->obj_type == ICON_DATA_ID) {
    ((ID *)(icon->obj))->icon_id = 0;
  }
  else if (icon->obj_type == ICON_DATA_IMBUF) {
    ImBuf *imbuf = (ImBuf *)icon->obj;
    if (imbuf) {
      IMB_freeImBuf(imbuf);
    }
  }
  else if (icon->obj_type == ICON_DATA_PREVIEW) {
    ((PreviewImage *)(icon->obj))->icon_id = 0;
  }
  else if (icon->obj_type == ICON_DATA_GPLAYER) {
    ((bGPDlayer *)(icon->obj))->runtime.icon_id = 0;
  }
  else if (icon->obj_type == ICON_DATA_GEOM) {
    ((struct Icon_Geom *)(icon->obj))->icon_id = 0;
  }
  else if (icon->obj_type == ICON_DATA_STUDIOLIGHT) {
    StudioLight *sl = (StudioLight *)icon->obj;
    if (sl != nullptr) {
      BKE_studiolight_unset_icon_id(sl, icon_id);
    }
  }
  else {
    BLI_assert(0);
  }
}

static Icon *icon_ghash_lookup(int icon_id)
{
  std::scoped_lock lock(gIconMutex);
  return (Icon *)BLI_ghash_lookup(gIcons, POINTER_FROM_INT(icon_id));
}

/* create an id for a new icon and make sure that ids from deleted icons get reused
 * after the integer number range is used up */
static int get_next_free_id()
{
  std::scoped_lock lock(gIconMutex);
  int startId = gFirstIconId;

  /* if we haven't used up the int number range, we just return the next int */
  if (gNextIconId >= gFirstIconId) {
    int next_id = gNextIconId++;
    return next_id;
  }

  /* Now we try to find the smallest icon id not stored in the gIcons hash.
   * Don't use icon_ghash_lookup here, it would lock recursively (dead-lock). */
  while (BLI_ghash_lookup(gIcons, POINTER_FROM_INT(startId)) && startId >= gFirstIconId) {
    startId++;
  }

  /* if we found a suitable one that isn't used yet, return it */
  if (startId >= gFirstIconId) {
    return startId;
  }

  /* fail */
  return 0;
}

void BKE_icons_init(int first_dyn_id)
{
  BLI_assert(BLI_thread_is_main());

  gNextIconId = first_dyn_id;
  gFirstIconId = first_dyn_id;

  if (!gIcons) {
    gIcons = BLI_ghash_int_new(__func__);
    BLI_linklist_lockfree_init(&g_icon_delete_queue);
  }

  if (!gCachedPreviews) {
    gCachedPreviews = BLI_ghash_str_new(__func__);
  }
}

void BKE_icons_free(void)
{
  BLI_assert(BLI_thread_is_main());

  if (gIcons) {
    BLI_ghash_free(gIcons, nullptr, icon_free);
    gIcons = nullptr;
  }

  if (gCachedPreviews) {
    BLI_ghash_free(gCachedPreviews, MEM_freeN, BKE_previewimg_freefunc);
    gCachedPreviews = nullptr;
  }

  BLI_linklist_lockfree_free(&g_icon_delete_queue, MEM_freeN);
}

void BKE_icons_deferred_free(void)
{
  std::scoped_lock lock(gIconMutex);

  for (DeferredIconDeleteNode *node =
           (DeferredIconDeleteNode *)BLI_linklist_lockfree_begin(&g_icon_delete_queue);
       node != nullptr;
       node = node->next) {
    BLI_ghash_remove(gIcons, POINTER_FROM_INT(node->icon_id), nullptr, icon_free);
  }
  BLI_linklist_lockfree_clear(&g_icon_delete_queue, MEM_freeN);
}

static PreviewImage *previewimg_create_ex(size_t deferred_data_size)
{
  PreviewImage *prv_img = (PreviewImage *)MEM_mallocN(sizeof(PreviewImage) + deferred_data_size,
                                                      "img_prv");
  memset(prv_img, 0, sizeof(*prv_img)); /* leave deferred data dirty */

  if (deferred_data_size) {
    prv_img->tag |= PRV_TAG_DEFFERED;
  }

  for (int i = 0; i < NUM_ICON_SIZES; i++) {
    prv_img->flag[i] |= (PRV_CHANGED | PRV_UNFINISHED);
    prv_img->changed_timestamp[i] = 0;
  }
  return prv_img;
}

static PreviewImage *previewimg_deferred_create(const char *path, int source)
{
  /* We pack needed data for lazy loading (source type, in a single char, and path). */
  const size_t deferred_data_size = strlen(path) + 2;
  char *deferred_data;

  PreviewImage *prv = previewimg_create_ex(deferred_data_size);
  deferred_data = (char *)PRV_DEFERRED_DATA(prv);
  deferred_data[0] = source;
  memcpy(&deferred_data[1], path, deferred_data_size - 1);

  return prv;
}

PreviewImage *BKE_previewimg_create(void)
{
  return previewimg_create_ex(0);
}

void BKE_previewimg_freefunc(void *link)
{
  PreviewImage *prv = (PreviewImage *)link;
  if (prv) {
    for (int i = 0; i < NUM_ICON_SIZES; i++) {
      if (prv->rect[i]) {
        MEM_freeN(prv->rect[i]);
      }
      if (prv->gputexture[i]) {
        GPU_texture_free(prv->gputexture[i]);
      }
    }

    MEM_freeN(prv);
  }
}

void BKE_previewimg_free(PreviewImage **prv)
{
  if (prv && (*prv)) {
    BKE_previewimg_freefunc(*prv);
    *prv = nullptr;
  }
}

void BKE_previewimg_clear_single(struct PreviewImage *prv, enum eIconSizes size)
{
  MEM_SAFE_FREE(prv->rect[size]);
  if (prv->gputexture[size]) {
    GPU_texture_free(prv->gputexture[size]);
  }
  prv->h[size] = prv->w[size] = 0;
  prv->flag[size] |= (PRV_CHANGED | PRV_UNFINISHED);
  prv->flag[size] &= ~PRV_USER_EDITED;
  prv->changed_timestamp[size] = 0;
}

void BKE_previewimg_clear(struct PreviewImage *prv)
{
  for (int i = 0; i < NUM_ICON_SIZES; i++) {
    BKE_previewimg_clear_single(prv, (eIconSizes)i);
  }
}

PreviewImage *BKE_previewimg_copy(const PreviewImage *prv)
{
  PreviewImage *prv_img = nullptr;

  if (prv) {
    prv_img = (PreviewImage *)MEM_dupallocN(prv);
    for (int i = 0; i < NUM_ICON_SIZES; i++) {
      if (prv->rect[i]) {
        prv_img->rect[i] = (uint *)MEM_dupallocN(prv->rect[i]);
      }
      prv_img->gputexture[i] = nullptr;
    }
  }
  return prv_img;
}

/**
 * Duplicate preview image from \a id and clear icon_id,
 * to be used by datablock copy functions.
 */
void BKE_previewimg_id_copy(ID *new_id, const ID *old_id)
{
  PreviewImage **old_prv_p = BKE_previewimg_id_get_p(old_id);
  PreviewImage **new_prv_p = BKE_previewimg_id_get_p(new_id);

  if (old_prv_p && *old_prv_p) {
    BLI_assert(new_prv_p != nullptr && ELEM(*new_prv_p, nullptr, *old_prv_p));
    //      const int new_icon_id = get_next_free_id();

    //      if (new_icon_id == 0) {
    //          return;  /* Failure. */
    //      }
    *new_prv_p = BKE_previewimg_copy(*old_prv_p);
    new_id->icon_id = (*new_prv_p)->icon_id = 0;
  }
}

PreviewImage **BKE_previewimg_id_get_p(const ID *id)
{
  switch (GS(id->name)) {
    case ID_OB: {
      Object *ob = (Object *)id;
      /* Currently, only object types with real geometry can be rendered as preview. */
      if (!OB_TYPE_IS_GEOMETRY(ob->type)) {
        return nullptr;
      }
      return &ob->preview;
    }

#define ID_PRV_CASE(id_code, id_struct) \
  case id_code: { \
    return &((id_struct *)id)->preview; \
  } \
    ((void)0)
      ID_PRV_CASE(ID_MA, Material);
      ID_PRV_CASE(ID_TE, Tex);
      ID_PRV_CASE(ID_WO, World);
      ID_PRV_CASE(ID_LA, Light);
      ID_PRV_CASE(ID_IM, Image);
      ID_PRV_CASE(ID_BR, Brush);
      ID_PRV_CASE(ID_GR, Collection);
      ID_PRV_CASE(ID_SCE, Scene);
      ID_PRV_CASE(ID_SCR, bScreen);
      ID_PRV_CASE(ID_AC, bAction);
#undef ID_PRV_CASE
    default:
      break;
  }

  return nullptr;
}

PreviewImage *BKE_previewimg_id_get(const ID *id)
{
  PreviewImage **prv_p = BKE_previewimg_id_get_p(id);
  return prv_p ? *prv_p : nullptr;
}

void BKE_previewimg_id_free(ID *id)
{
  PreviewImage **prv_p = BKE_previewimg_id_get_p(id);
  if (prv_p) {
    BKE_previewimg_free(prv_p);
  }
}

PreviewImage *BKE_previewimg_id_ensure(ID *id)
{
  PreviewImage **prv_p = BKE_previewimg_id_get_p(id);

  if (prv_p) {
    if (*prv_p == nullptr) {
      *prv_p = BKE_previewimg_create();
    }
    return *prv_p;
  }

  return nullptr;
}

void BKE_previewimg_id_custom_set(ID *id, const char *path)
{
  PreviewImage **prv = BKE_previewimg_id_get_p(id);

  /* Thumbnail previews must use the deferred pipeline. But we force them to be immediately
   * generated here still. */

  if (*prv) {
    BKE_previewimg_deferred_release(*prv);
  }
  *prv = previewimg_deferred_create(path, THB_SOURCE_IMAGE);

  /* Can't lazy-render the preview on access. ID previews are saved to files and we want them to be
   * there in time. Not only if something happened to have accessed it meanwhile. */
  for (int i = 0; i < NUM_ICON_SIZES; i++) {
    BKE_previewimg_ensure(*prv, i);
    /* Prevent auto-updates. */
    (*prv)->flag[i] |= PRV_USER_EDITED;
  }
}

bool BKE_previewimg_id_supports_jobs(const ID *id)
{
  return ELEM(GS(id->name), ID_OB, ID_MA, ID_TE, ID_LA, ID_WO, ID_IM, ID_BR);
}

void BKE_previewimg_deferred_release(PreviewImage *prv)
{
  if (prv) {
    if (prv->tag & PRV_TAG_DEFFERED_RENDERING) {
      /* We cannot delete the preview while it is being loaded in another thread... */
      prv->tag |= PRV_TAG_DEFFERED_DELETE;
      return;
    }
    if (prv->icon_id) {
      BKE_icon_delete(prv->icon_id);
    }
    BKE_previewimg_freefunc(prv);
  }
}

PreviewImage *BKE_previewimg_cached_get(const char *name)
{
  BLI_assert(BLI_thread_is_main());
  return (PreviewImage *)BLI_ghash_lookup(gCachedPreviews, name);
}

/**
 * Generate an empty PreviewImage, if not yet existing.
 */
PreviewImage *BKE_previewimg_cached_ensure(const char *name)
{
  BLI_assert(BLI_thread_is_main());

  PreviewImage *prv = nullptr;
  void **key_p, **prv_p;

  if (!BLI_ghash_ensure_p_ex(gCachedPreviews, name, &key_p, &prv_p)) {
    *key_p = BLI_strdup(name);
    *prv_p = BKE_previewimg_create();
  }
  prv = *(PreviewImage **)prv_p;
  BLI_assert(prv);

  return prv;
}

/**
 * Generate a PreviewImage from given file path, using thumbnails management, if not yet existing.
 * Does not actually generate the preview, #BKE_previewimg_ensure() must be called for that.
 */
PreviewImage *BKE_previewimg_cached_thumbnail_read(const char *name,
                                                   const char *path,
                                                   const int source,
                                                   bool force_update)
{
  BLI_assert(BLI_thread_is_main());

  PreviewImage *prv = nullptr;
  void **prv_p;

  prv_p = BLI_ghash_lookup_p(gCachedPreviews, name);

  if (prv_p) {
    prv = *(PreviewImage **)prv_p;
    BLI_assert(prv);
  }

  if (prv && force_update) {
    const char *prv_deferred_data = (char *)PRV_DEFERRED_DATA(prv);
    if (((int)prv_deferred_data[0] == source) && STREQ(&prv_deferred_data[1], path)) {
      /* If same path, no need to re-allocate preview, just clear it up. */
      BKE_previewimg_clear(prv);
    }
    else {
      BKE_previewimg_free(&prv);
    }
  }

  if (!prv) {
    prv = previewimg_deferred_create(path, source);
    force_update = true;
  }

  if (force_update) {
    if (prv_p) {
      *prv_p = prv;
    }
    else {
      BLI_ghash_insert(gCachedPreviews, BLI_strdup(name), prv);
    }
  }

  return prv;
}

void BKE_previewimg_cached_release(const char *name)
{
  BLI_assert(BLI_thread_is_main());

  PreviewImage *prv = (PreviewImage *)BLI_ghash_popkey(gCachedPreviews, name, MEM_freeN);

  BKE_previewimg_deferred_release(prv);
}

/**
 * Handle deferred (lazy) loading/generation of preview image, if needed.
 * For now, only used with file thumbnails.
 */
void BKE_previewimg_ensure(PreviewImage *prv, const int size)
{
  if ((prv->tag & PRV_TAG_DEFFERED) != 0) {
    const bool do_icon = ((size == ICON_SIZE_ICON) && !prv->rect[ICON_SIZE_ICON]);
    const bool do_preview = ((size == ICON_SIZE_PREVIEW) && !prv->rect[ICON_SIZE_PREVIEW]);

    if (do_icon || do_preview) {
      ImBuf *thumb;
      char *prv_deferred_data = (char *)PRV_DEFERRED_DATA(prv);
      int source = prv_deferred_data[0];
      char *path = &prv_deferred_data[1];
      int icon_w, icon_h;

      thumb = IMB_thumb_manage(path, THB_LARGE, (ThumbSource)source);

      if (thumb) {
        /* PreviewImage assumes premultiplied alhpa... */
        IMB_premultiply_alpha(thumb);

        if (do_preview) {
          prv->w[ICON_SIZE_PREVIEW] = thumb->x;
          prv->h[ICON_SIZE_PREVIEW] = thumb->y;
          prv->rect[ICON_SIZE_PREVIEW] = (uint *)MEM_dupallocN(thumb->rect);
          prv->flag[ICON_SIZE_PREVIEW] &= ~(PRV_CHANGED | PRV_USER_EDITED | PRV_UNFINISHED);
        }
        if (do_icon) {
          if (thumb->x > thumb->y) {
            icon_w = ICON_RENDER_DEFAULT_HEIGHT;
            icon_h = (thumb->y * icon_w) / thumb->x + 1;
          }
          else if (thumb->x < thumb->y) {
            icon_h = ICON_RENDER_DEFAULT_HEIGHT;
            icon_w = (thumb->x * icon_h) / thumb->y + 1;
          }
          else {
            icon_w = icon_h = ICON_RENDER_DEFAULT_HEIGHT;
          }

          IMB_scaleImBuf(thumb, icon_w, icon_h);
          prv->w[ICON_SIZE_ICON] = icon_w;
          prv->h[ICON_SIZE_ICON] = icon_h;
          prv->rect[ICON_SIZE_ICON] = (uint *)MEM_dupallocN(thumb->rect);
          prv->flag[ICON_SIZE_ICON] &= ~(PRV_CHANGED | PRV_USER_EDITED | PRV_UNFINISHED);
        }
        IMB_freeImBuf(thumb);
      }
    }
  }
}

/**
 * Create an #ImBuf holding a copy of the preview image buffer in \a prv.
 * \note The returned image buffer has to be free'd (#IMB_freeImBuf()).
 */
ImBuf *BKE_previewimg_to_imbuf(PreviewImage *prv, const int size)
{
  const unsigned int w = prv->w[size];
  const unsigned int h = prv->h[size];
  const unsigned int *rect = prv->rect[size];

  ImBuf *ima = nullptr;

  if (w > 0 && h > 0 && rect) {
    /* first allocate imbuf for copying preview into it */
    ima = IMB_allocImBuf(w, h, 32, IB_rect);
    memcpy(ima->rect, rect, w * h * sizeof(*ima->rect));
  }

  return ima;
}

void BKE_previewimg_finish(PreviewImage *prv, const int size)
{
  /* Previews may be calculated on a thread. */
  atomic_fetch_and_and_int16(&prv->flag[size], ~PRV_UNFINISHED);
}

bool BKE_previewimg_is_finished(const PreviewImage *prv, const int size)
{
  return (prv->flag[size] & PRV_UNFINISHED) == 0;
}

void BKE_previewimg_blend_write(BlendWriter *writer, const PreviewImage *prv)
{
  /* Note we write previews also for undo steps. It takes up some memory,
   * but not doing so would causes all previews to be re-rendered after
   * undo which is too expensive. */

  if (prv == nullptr) {
    return;
  }

  PreviewImage prv_copy = *prv;
  BLO_write_struct_at_address(writer, PreviewImage, prv, &prv_copy);
  if (prv_copy.rect[0]) {
    BLO_write_uint32_array(writer, prv_copy.w[0] * prv_copy.h[0], prv_copy.rect[0]);
  }
  if (prv_copy.rect[1]) {
    BLO_write_uint32_array(writer, prv_copy.w[1] * prv_copy.h[1], prv_copy.rect[1]);
  }
}

void BKE_previewimg_blend_read(BlendDataReader *reader, PreviewImage *prv)
{
  if (prv == nullptr) {
    return;
  }

  for (int i = 0; i < NUM_ICON_SIZES; i++) {
    if (prv->rect[i]) {
      BLO_read_data_address(reader, &prv->rect[i]);
    }
    prv->gputexture[i] = nullptr;
    /* For now consider previews read from file as finished to not confuse File Browser preview
     * loading. That could be smarter and check if there's a preview job running instead.
     * If the preview is tagged as changed, it needs to be updated anyway, so don't remove the tag.
     */
    if ((prv->flag[i] & PRV_CHANGED) == 0) {
      BKE_previewimg_finish(prv, i);
    }
    else {
      /* Only for old files that didn't write the flag. */
      prv->flag[i] |= PRV_UNFINISHED;
    }
  }
  prv->icon_id = 0;
  prv->tag = 0;
}

void BKE_icon_changed(const int icon_id)
{
  Icon *icon = nullptr;

  if (!icon_id || G.background) {
    return;
  }

  icon = icon_ghash_lookup(icon_id);

  if (icon) {
    /* We *only* expect ID-tied icons here, not non-ID icon/preview! */
    BLI_assert(icon->id_type != 0);
    BLI_assert(icon->obj_type == ICON_DATA_ID);

    /* Do not enforce creation of previews for valid ID types using BKE_previewimg_id_ensure()
     * here, we only want to ensure *existing* preview images are properly tagged as
     * changed/invalid, that's all. */
    PreviewImage **p_prv = BKE_previewimg_id_get_p((ID *)icon->obj);

    /* If we have previews, they all are now invalid changed. */
    if (p_prv && *p_prv) {
      for (int i = 0; i < NUM_ICON_SIZES; i++) {
        (*p_prv)->flag[i] |= (PRV_CHANGED | PRV_UNFINISHED);
        (*p_prv)->changed_timestamp[i]++;
      }
    }
  }
}

static Icon *icon_create(int icon_id, int obj_type, void *obj)
{
  Icon *new_icon = (Icon *)MEM_mallocN(sizeof(Icon), __func__);

  new_icon->obj_type = obj_type;
  new_icon->obj = obj;
  new_icon->id_type = 0;
  new_icon->flag = 0;

  /* next two lines make sure image gets created */
  new_icon->drawinfo = nullptr;
  new_icon->drawinfo_free = nullptr;

  {
    std::scoped_lock lock(gIconMutex);
    BLI_ghash_insert(gIcons, POINTER_FROM_INT(icon_id), new_icon);
  }

  return new_icon;
}

static int icon_id_ensure_create_icon(struct ID *id)
{
  BLI_assert(BLI_thread_is_main());

  Icon *icon = icon_create(id->icon_id, ICON_DATA_ID, id);
  icon->id_type = GS(id->name);
  icon->flag = ICON_FLAG_MANAGED;

  return id->icon_id;
}

int BKE_icon_id_ensure(struct ID *id)
{
  /* Never handle icons in non-main thread! */
  BLI_assert(BLI_thread_is_main());

  if (!id || G.background) {
    return 0;
  }

  if (id->icon_id) {
    return id->icon_id;
  }

  id->icon_id = get_next_free_id();

  if (!id->icon_id) {
    CLOG_ERROR(&LOG, "not enough IDs");
    return 0;
  }

  /* Ensure we synchronize ID icon_id with its previewimage if it has one. */
  PreviewImage **p_prv = BKE_previewimg_id_get_p(id);
  if (p_prv && *p_prv) {
    BLI_assert(ELEM((*p_prv)->icon_id, 0, id->icon_id));
    (*p_prv)->icon_id = id->icon_id;
  }

  return icon_id_ensure_create_icon(id);
}

static int icon_gplayer_color_ensure_create_icon(bGPDlayer *gpl)
{
  BLI_assert(BLI_thread_is_main());

  /* NOTE: The color previews for GP Layers don't really need
   * to be "rendered" to image per se (as it will just be a plain
   * colored rectangle), we need to define icon data here so that
   * we can store a pointer to the layer data in icon->obj.
   */
  Icon *icon = icon_create(gpl->runtime.icon_id, ICON_DATA_GPLAYER, gpl);
  icon->flag = ICON_FLAG_MANAGED;

  return gpl->runtime.icon_id;
}

int BKE_icon_gplayer_color_ensure(bGPDlayer *gpl)
{
  /* Never handle icons in non-main thread! */
  BLI_assert(BLI_thread_is_main());

  if (!gpl || G.background) {
    return 0;
  }

  if (gpl->runtime.icon_id) {
    return gpl->runtime.icon_id;
  }

  gpl->runtime.icon_id = get_next_free_id();

  if (!gpl->runtime.icon_id) {
    CLOG_ERROR(&LOG, "not enough IDs");
    return 0;
  }

  return icon_gplayer_color_ensure_create_icon(gpl);
}

/**
 * Return icon id of given preview, or create new icon if not found.
 */
int BKE_icon_preview_ensure(ID *id, PreviewImage *preview)
{
  if (!preview || G.background) {
    return 0;
  }

  if (id) {
    BLI_assert(BKE_previewimg_id_ensure(id) == preview);
  }

  if (preview->icon_id) {
    BLI_assert(!id || !id->icon_id || id->icon_id == preview->icon_id);
    return preview->icon_id;
  }

  if (id && id->icon_id) {
    preview->icon_id = id->icon_id;
    return preview->icon_id;
  }

  preview->icon_id = get_next_free_id();

  if (!preview->icon_id) {
    CLOG_ERROR(&LOG, "not enough IDs");
    return 0;
  }

  /* Ensure we synchronize ID icon_id with its previewimage if available,
   * and generate suitable 'ID' icon. */
  if (id) {
    id->icon_id = preview->icon_id;
    return icon_id_ensure_create_icon(id);
  }

  Icon *icon = icon_create(preview->icon_id, ICON_DATA_PREVIEW, preview);
  icon->flag = ICON_FLAG_MANAGED;

  return preview->icon_id;
}

/**
 * Create an icon as owner or \a ibuf. The icon-ID is not stored in \a ibuf, it needs to be stored
 * separately.
 * \note Transforms ownership of \a ibuf to the newly created icon.
 */
int BKE_icon_imbuf_create(ImBuf *ibuf)
{
  int icon_id = get_next_free_id();

  Icon *icon = icon_create(icon_id, ICON_DATA_IMBUF, ibuf);
  icon->flag = ICON_FLAG_MANAGED;

  return icon_id;
}

ImBuf *BKE_icon_imbuf_get_buffer(int icon_id)
{
  Icon *icon = icon_ghash_lookup(icon_id);
  if (!icon) {
    CLOG_ERROR(&LOG, "no icon for icon ID: %d", icon_id);
    return nullptr;
  }
  if (icon->obj_type != ICON_DATA_IMBUF) {
    CLOG_ERROR(&LOG, "icon ID does not refer to an imbuf icon: %d", icon_id);
    return nullptr;
  }

  return (ImBuf *)icon->obj;
}

Icon *BKE_icon_get(const int icon_id)
{
  BLI_assert(BLI_thread_is_main());

  Icon *icon = nullptr;

  icon = icon_ghash_lookup(icon_id);

  if (!icon) {
    CLOG_ERROR(&LOG, "no icon for icon ID: %d", icon_id);
    return nullptr;
  }

  return icon;
}

void BKE_icon_set(const int icon_id, struct Icon *icon)
{
  void **val_p;

  std::scoped_lock lock(gIconMutex);
  if (BLI_ghash_ensure_p(gIcons, POINTER_FROM_INT(icon_id), &val_p)) {
    CLOG_ERROR(&LOG, "icon already set: %d", icon_id);
    return;
  }

  *val_p = icon;
}

static void icon_add_to_deferred_delete_queue(int icon_id)
{
  DeferredIconDeleteNode *node = (DeferredIconDeleteNode *)MEM_mallocN(
      sizeof(DeferredIconDeleteNode), __func__);
  node->icon_id = icon_id;
  /* Doesn't need lock. */
  BLI_linklist_lockfree_insert(&g_icon_delete_queue, (LockfreeLinkNode *)node);
}

void BKE_icon_id_delete(struct ID *id)
{
  const int icon_id = id->icon_id;
  if (!icon_id) {
    return; /* no icon defined for library object */
  }
  id->icon_id = 0;

  if (!BLI_thread_is_main()) {
    icon_add_to_deferred_delete_queue(icon_id);
    return;
  }

  BKE_icons_deferred_free();
  std::scoped_lock lock(gIconMutex);
  BLI_ghash_remove(gIcons, POINTER_FROM_INT(icon_id), nullptr, icon_free);
}

/**
 * Remove icon and free data.
 */
bool BKE_icon_delete(const int icon_id)
{
  if (icon_id == 0) {
    /* no icon defined for library object */
    return false;
  }

  std::scoped_lock lock(gIconMutex);
  if (Icon *icon = (Icon *)BLI_ghash_popkey(gIcons, POINTER_FROM_INT(icon_id), nullptr)) {
    icon_free_data(icon_id, icon);
    icon_free(icon);
    return true;
  }

  return false;
}

bool BKE_icon_delete_unmanaged(const int icon_id)
{
  if (icon_id == 0) {
    /* no icon defined for library object */
    return false;
  }

  std::scoped_lock lock(gIconMutex);

  Icon *icon = (Icon *)BLI_ghash_popkey(gIcons, POINTER_FROM_INT(icon_id), nullptr);
  if (icon) {
    if (UNLIKELY(icon->flag & ICON_FLAG_MANAGED)) {
      BLI_ghash_insert(gIcons, POINTER_FROM_INT(icon_id), icon);
      return false;
    }

    icon_free_data(icon_id, icon);
    icon_free(icon);
    return true;
  }

  return false;
}

/* -------------------------------------------------------------------- */
/** \name Geometry Icon
 * \{ */

int BKE_icon_geom_ensure(struct Icon_Geom *geom)
{
  BLI_assert(BLI_thread_is_main());

  if (geom->icon_id) {
    return geom->icon_id;
  }

  geom->icon_id = get_next_free_id();

  icon_create(geom->icon_id, ICON_DATA_GEOM, geom);
  /* Not managed for now, we may want this to be configurable per icon). */

  return geom->icon_id;
}

struct Icon_Geom *BKE_icon_geom_from_memory(uchar *data, size_t data_len)
{
  BLI_assert(BLI_thread_is_main());
  if (data_len <= 8) {
    return nullptr;
  }
  /* Wrapper for RAII early exit cleanups. */
  std::unique_ptr<uchar> data_wrapper(std::move(data));

  /* Skip the header. */
  data_len -= 8;
  const int div = 3 * 2 * 3;
  const int coords_len = data_len / div;
  if (coords_len * div != data_len) {
    return nullptr;
  }

  const uchar header[4] = {'V', 'C', 'O', 0};
  uchar *p = data_wrapper.get();
  if (memcmp(p, header, ARRAY_SIZE(header)) != 0) {
    return nullptr;
  }
  p += 4;

  struct Icon_Geom *geom = (struct Icon_Geom *)MEM_mallocN(sizeof(*geom), __func__);
  geom->coords_range[0] = (int)*p++;
  geom->coords_range[1] = (int)*p++;
  /* x, y ignored for now */
  p += 2;

  geom->coords_len = coords_len;
  geom->coords = reinterpret_cast<decltype(geom->coords)>(p);
  geom->colors = reinterpret_cast<decltype(geom->colors)>(p + (data_len / 3));
  geom->icon_id = 0;
  /* Move buffer ownership to C buffer. */
  geom->mem = data_wrapper.release();
  return geom;
}

struct Icon_Geom *BKE_icon_geom_from_file(const char *filename)
{
  BLI_assert(BLI_thread_is_main());
  size_t data_len;
  uchar *data = (uchar *)BLI_file_read_binary_as_mem(filename, 0, &data_len);
  if (data == nullptr) {
    return nullptr;
  }
  return BKE_icon_geom_from_memory(data, data_len);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Studio Light Icon
 * \{ */

int BKE_icon_ensure_studio_light(struct StudioLight *sl, int id_type)
{
  int icon_id = get_next_free_id();
  Icon *icon = icon_create(icon_id, ICON_DATA_STUDIOLIGHT, sl);
  icon->id_type = id_type;
  return icon_id;
}
/** \} */
