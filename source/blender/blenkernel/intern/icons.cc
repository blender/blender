/* SPDX-FileCopyrightText: 2006-2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdlib>
#include <cstring>
#include <memory>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_gpencil_legacy_types.h"

#include "BLI_fileops.h"
#include "BLI_linklist_lockfree.h"
#include "BLI_map.hh"
#include "BLI_mutex.hh"
#include "BLI_span.hh"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_global.hh" /* only for G.background test */
#include "BKE_icons.hh"
#include "BKE_preview_image.hh"
#include "BKE_studiolight.h"

#include "BLI_sys_types.h" /* for intptr_t support */

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

namespace blender {

/**
 * Only allow non-managed icons to be removed (by Python for eg).
 * Previews & ID's have their own functions to remove icons.
 */
enum {
  ICON_FLAG_MANAGED = (1 << 0),
};

/* GLOBALS */

static CLG_LogRef LOG = {"lib.icons"};

/* Protected by gIconMutex. */
using GlobalIconsMap = Map<int, Icon *>;
static GlobalIconsMap &get_global_icons_map()
{
  static GlobalIconsMap gIcons;
  return gIcons;
}

/* Protected by gIconMutex. */
static int gNextIconId = 1;

/* Protected by gIconMutex. */
static int gFirstIconId = 1;

static Mutex gIconMutex;

/* Queue of icons for deferred deletion. */
struct DeferredIconDeleteNode {
  DeferredIconDeleteNode *next;
  int icon_id;
};
/* Protected by gIconMutex. */
static LockfreeLinkList g_icon_delete_queue;

static void icon_free(void *val)
{
  Icon *icon = static_cast<Icon *>(val);
  if (!icon) {
    return;
  }

  if (icon->obj_type == ICON_DATA_GEOM) {
    Icon_Geom *obj = static_cast<Icon_Geom *>(icon->obj);
    if (obj->mem) {
      /* coords & colors are part of this memory. */
      MEM_delete(obj->mem);
    }
    else {
      MEM_delete(obj->coords);
      MEM_delete(obj->colors);
    }
    MEM_delete(obj);
  }

  if (icon->drawinfo_free) {
    icon->drawinfo_free(icon->drawinfo);
  }
  else if (icon->drawinfo) {
    MEM_delete_void(icon->drawinfo);
  }
  MEM_delete(icon);
}

static void icon_free_data(int icon_id, Icon *icon)
{
  switch (icon->obj_type) {
    case ICON_DATA_ID:
      (static_cast<ID *>(icon->obj))->icon_id = 0;
      break;
    case ICON_DATA_IMBUF: {
      ImBuf *imbuf = static_cast<ImBuf *>(icon->obj);
      if (imbuf) {
        IMB_freeImBuf(imbuf);
      }
      break;
    }
    case ICON_DATA_PREVIEW:
      (static_cast<PreviewImage *>(icon->obj))->runtime->icon_id = 0;
      break;
    case ICON_DATA_GPLAYER:
      (static_cast<bGPDlayer *>(icon->obj))->runtime.icon_id = 0;
      break;
    case ICON_DATA_GEOM:
      (static_cast<Icon_Geom *>(icon->obj))->icon_id = 0;
      break;
    case ICON_DATA_STUDIOLIGHT: {
      StudioLight *sl = static_cast<StudioLight *>(icon->obj);
      if (sl != nullptr) {
        BKE_studiolight_unset_icon_id(sl, icon_id);
      }
      break;
    }
    default:
      BLI_assert_unreachable();
  }
}

static Icon *icon_ghash_lookup(int icon_id)
{
  std::scoped_lock lock(gIconMutex);
  const GlobalIconsMap &gIcons = get_global_icons_map();
  return gIcons.lookup_default(icon_id, nullptr);
}

/* create an id for a new icon and make sure that ids from deleted icons get reused
 * after the integer number range is used up */
static int get_next_free_id()
{
  std::scoped_lock lock(gIconMutex);
  const GlobalIconsMap &gIcons = get_global_icons_map();
  int startId = gFirstIconId;

  /* if we haven't used up the int number range, we just return the next int */
  if (gNextIconId >= gFirstIconId) {
    int next_id = gNextIconId++;
    return next_id;
  }

  /* Now we try to find the smallest icon id not stored in the gIcons hash.
   * Don't use icon_ghash_lookup here, it would lock recursively (dead-lock). */
  while (gIcons.contains(startId) && startId >= gFirstIconId) {
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

  BLI_linklist_lockfree_init(&g_icon_delete_queue);
}

void BKE_icons_free()
{
  BLI_assert(BLI_thread_is_main());

  GlobalIconsMap &gIcons = get_global_icons_map();
  for (Icon *icon : gIcons.values()) {
    icon_free(icon);
  }
  gIcons.clear();

  BLI_linklist_lockfree_free(&g_icon_delete_queue, MEM_delete_void);
}

void BKE_icons_deferred_free()
{
  std::scoped_lock lock(gIconMutex);
  GlobalIconsMap &gIcons = get_global_icons_map();

  for (DeferredIconDeleteNode *node = reinterpret_cast<DeferredIconDeleteNode *>(
           BLI_linklist_lockfree_begin(&g_icon_delete_queue));
       node != nullptr;
       node = node->next)
  {
    if (Icon *icon = gIcons.pop_default(node->icon_id, nullptr)) {
      icon_free(icon);
    }
  }
  BLI_linklist_lockfree_clear(&g_icon_delete_queue, MEM_delete_void);
}

void BKE_icon_changed(const int icon_id)
{
  Icon *icon = nullptr;

  if (!icon_id || G.background) {
    return;
  }

  icon = icon_ghash_lookup(icon_id);
  if (!icon) {
    return;
  }

  /* We *only* expect ID-tied icons here, not non-ID icon/preview! */
  BLI_assert(icon->id_type != 0);
  BLI_assert(icon->obj_type == ICON_DATA_ID);

  /* Do not enforce creation of previews for valid ID types using BKE_previewimg_id_ensure()
   * here, we only want to ensure *existing* preview images are properly tagged as
   * changed/invalid, that's all. */
  PreviewImage **p_prv = BKE_previewimg_id_get_p(static_cast<ID *>(icon->obj));

  /* If we have previews, they all are now invalid changed. */
  if (p_prv && *p_prv) {
    for (int i = 0; i < NUM_ICON_SIZES; i++) {
      (*p_prv)->flag[i] |= PRV_CHANGED;
      (*p_prv)->changed_timestamp[i]++;
    }
  }
}

static Icon *icon_create(int icon_id, int obj_type, void *obj)
{
  Icon *new_icon = MEM_new_uninitialized<Icon>(__func__);

  new_icon->obj_type = obj_type;
  new_icon->obj = obj;
  new_icon->id_type = 0;
  new_icon->flag = 0;

  /* next two lines make sure image gets created */
  new_icon->drawinfo = nullptr;
  new_icon->drawinfo_free = nullptr;

  {
    std::scoped_lock lock(gIconMutex);
    GlobalIconsMap &gIcons = get_global_icons_map();
    gIcons.add(icon_id, new_icon);
  }

  return new_icon;
}

static int icon_id_ensure_create_icon(ID *id)
{
  BLI_assert(BLI_thread_is_main());

  Icon *icon = icon_create(id->icon_id, ICON_DATA_ID, id);
  icon->id_type = GS(id->name);
  icon->flag = ICON_FLAG_MANAGED;

  return id->icon_id;
}

int BKE_icon_id_ensure(ID *id)
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
    BLI_assert(ELEM((*p_prv)->runtime->icon_id, 0, id->icon_id));
    (*p_prv)->runtime->icon_id = id->icon_id;
  }

  return icon_id_ensure_create_icon(id);
}

static int icon_gplayer_color_ensure_create_icon(bGPDlayer *gpl)
{
  BLI_assert(BLI_thread_is_main());

  /* NOTE: The color previews for GP Layers don't really need
   * to be "rendered" to image per-se (as it will just be a plain
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

int BKE_icon_preview_ensure(ID *id, PreviewImage *preview)
{
  if (!preview || G.background) {
    return 0;
  }

  if (id) {
    BLI_assert(BKE_previewimg_id_ensure(id) == preview);
  }

  if (preview->runtime->icon_id) {
    BLI_assert(!id || !id->icon_id || id->icon_id == preview->runtime->icon_id);
    return preview->runtime->icon_id;
  }

  if (id && id->icon_id) {
    preview->runtime->icon_id = id->icon_id;
    return preview->runtime->icon_id;
  }

  preview->runtime->icon_id = get_next_free_id();

  if (!preview->runtime->icon_id) {
    CLOG_ERROR(&LOG, "not enough IDs");
    return 0;
  }

  /* Ensure we synchronize ID icon_id with its previewimage if available,
   * and generate suitable 'ID' icon. */
  if (id) {
    id->icon_id = preview->runtime->icon_id;
    return icon_id_ensure_create_icon(id);
  }

  Icon *icon = icon_create(preview->runtime->icon_id, ICON_DATA_PREVIEW, preview);
  icon->flag = ICON_FLAG_MANAGED;

  return preview->runtime->icon_id;
}

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

  return static_cast<ImBuf *>(icon->obj);
}

static IconBufferRef construct_icon_buffer(const int width,
                                           const int height,
                                           const int channels,
                                           const uint8_t *buffer)
{
  BLI_assert(buffer != nullptr);
  BLI_assert(width >= 0);
  BLI_assert(height >= 0);
  BLI_assert(channels >= 0);

  return IconBufferRef{
      .width = width,
      .height = height,
      .channels = channels,
      .buffer = Span(buffer, width * height * channels),
  };
}

static std::optional<IconBufferRef> icon_buffer_from_preview(const PreviewImage *preview,
                                                             const eIconSizes size)
{
  if (!preview->rect[size]) {
    return std::nullopt;
  }

  const int num_channels = 4; /* #PreviewImage always has 4 color channels. */
  return construct_icon_buffer(preview->w[size],
                               preview->h[size],
                               num_channels,
                               reinterpret_cast<uint8_t *>(preview->rect[size]));
}

std::optional<IconBufferRef> BKE_icon_get_buffer(const int icon_id, const eIconSizes size)
{
  const Icon *icon = icon_ghash_lookup(icon_id);
  if (!icon) {
    CLOG_ERROR(&LOG, "no icon for icon ID: %d", icon_id);
    return std::nullopt;
  }

  switch (icon->obj_type) {
    case ICON_DATA_IMBUF: {
      const ImBuf *ibuf = static_cast<ImBuf *>(icon->obj);
      if (ibuf->byte_buffer.data == nullptr) {
        return std::nullopt;
      }
      return construct_icon_buffer(ibuf->x, ibuf->y, ibuf->channels, ibuf->byte_buffer.data);
    }
    case ICON_DATA_ID: {
      const ID *id = static_cast<ID *>(icon->obj);
      if (PreviewImage *preview = BKE_previewimg_id_get(id)) {
        return icon_buffer_from_preview(preview, size);
      }
      break;
    }
    case ICON_DATA_PREVIEW: {
      if (const PreviewImage *preview = static_cast<PreviewImage *>(icon->obj)) {
        if (!BKE_previewimg_is_finished(preview, size)) {
          return std::nullopt;
        }
        return icon_buffer_from_preview(preview, size);
      }
      break;
    }
  }

  return std::nullopt;
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

void BKE_icon_set(const int icon_id, Icon *icon)
{
  std::scoped_lock lock(gIconMutex);
  GlobalIconsMap &gIcons = get_global_icons_map();
  if (!gIcons.add(icon_id, icon)) {
    CLOG_ERROR(&LOG, "icon already set: %d", icon_id);
  }
}

static void icon_add_to_deferred_delete_queue(int icon_id)
{
  DeferredIconDeleteNode *node = MEM_new_uninitialized<DeferredIconDeleteNode>(__func__);
  node->icon_id = icon_id;
  /* Doesn't need lock. */
  BLI_linklist_lockfree_insert(&g_icon_delete_queue, reinterpret_cast<LockfreeLinkNode *>(node));
}

void BKE_icon_id_delete(ID *id)
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
  GlobalIconsMap &gIcons = get_global_icons_map();
  if (Icon *icon = gIcons.pop_default(icon_id, nullptr)) {
    icon_free(icon);
  }
}

bool BKE_icon_delete(const int icon_id)
{
  if (icon_id == 0) {
    /* no icon defined for library object */
    return false;
  }

  std::scoped_lock lock(gIconMutex);
  GlobalIconsMap &gIcons = get_global_icons_map();
  if (Icon *icon = gIcons.pop_default(icon_id, nullptr)) {
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
  GlobalIconsMap &gIcons = get_global_icons_map();

  Icon *icon = gIcons.pop_default(icon_id, nullptr);
  if (icon) {
    if (UNLIKELY(icon->flag & ICON_FLAG_MANAGED)) {
      gIcons.add(icon_id, icon);
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

int BKE_icon_geom_ensure(Icon_Geom *geom)
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

Icon_Geom *BKE_icon_geom_from_memory(uchar *data, size_t data_len)
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

  Icon_Geom *geom = MEM_new_uninitialized<Icon_Geom>(__func__);
  geom->coords_range[0] = int(*p++);
  geom->coords_range[1] = int(*p++);
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

Icon_Geom *BKE_icon_geom_from_file(const char *filename)
{
  BLI_assert(BLI_thread_is_main());
  size_t data_len;
  uchar *data = static_cast<uchar *>(BLI_file_read_binary_as_mem(filename, 0, &data_len));
  if (data == nullptr) {
    return nullptr;
  }
  return BKE_icon_geom_from_memory(data, data_len);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Studio Light Icon
 * \{ */

int BKE_icon_ensure_studio_light(StudioLight *sl, int id_type)
{
  int icon_id = get_next_free_id();
  Icon *icon = icon_create(icon_id, ICON_DATA_STUDIOLIGHT, sl);
  icon->id_type = id_type;
  return icon_id;
}

/** \} */

}  // namespace blender
