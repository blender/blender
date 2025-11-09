/* SPDX-FileCopyrightText: 2006-2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <string>

#include "DNA_ID.h"
#include "DNA_brush_types.h"
#include "DNA_collection_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BKE_icons.hh"

#include "BLI_ghash.h"
#include "BLI_string.h"
#ifndef NDEBUG
#  include "BLI_threads.h"
#endif

#include "BLO_read_write.hh"

#include "GPU_texture.hh"

#include "IMB_imbuf.hh"
#include "IMB_thumbs.hh"

#include "atomic_ops.h"

#include "BKE_preview_image.hh"

/* Not mutex-protected! */
static GHash *gCachedPreviews = nullptr;

namespace blender::bke {

struct PreviewDeferredLoadingData {
  std::string filepath;
  ThumbSource source;
};

PreviewImageRuntime::PreviewImageRuntime() = default;
PreviewImageRuntime::PreviewImageRuntime(const PreviewImageRuntime &other)
{
  if (other.deferred_loading_data) {
    this->deferred_loading_data = std::make_unique<PreviewDeferredLoadingData>(
        *other.deferred_loading_data);
  }
}
PreviewImageRuntime::~PreviewImageRuntime() = default;

}  // namespace blender::bke

static PreviewImage *previewimg_deferred_create(const char *filepath, ThumbSource source)
{
  PreviewImage *prv = BKE_previewimg_create();
  prv->runtime->deferred_loading_data =
      std::make_unique<blender::bke::PreviewDeferredLoadingData>();
  prv->runtime->deferred_loading_data->filepath = filepath;
  prv->runtime->deferred_loading_data->source = source;
  return prv;
}

PreviewImage *BKE_previewimg_create()
{
  PreviewImage *prv = MEM_callocN<PreviewImage>(__func__);

  for (int i = 0; i < NUM_ICON_SIZES; i++) {
    prv->flag[i] |= PRV_CHANGED;
    prv->changed_timestamp[i] = 0;
  }

  prv->runtime = MEM_new<blender::bke::PreviewImageRuntime>(__func__);
  return prv;
}

void BKE_previewimg_free(PreviewImage **prv)
{
  if (prv && (*prv)) {
    for (int i = 0; i < NUM_ICON_SIZES; i++) {
      if ((*prv)->rect[i]) {
        MEM_freeN((*prv)->rect[i]);
      }
      if ((*prv)->runtime->gputexture[i]) {
        GPU_texture_free((*prv)->runtime->gputexture[i]);
      }
    }

    MEM_delete((*prv)->runtime);
    MEM_freeN(*prv);
    *prv = nullptr;
  }
}

void BKE_preview_images_init()
{
  if (!gCachedPreviews) {
    gCachedPreviews = BLI_ghash_str_new(__func__);
  }
}

void BKE_preview_images_free()
{
  if (gCachedPreviews) {
    BLI_ghash_free(gCachedPreviews, MEM_freeN, BKE_previewimg_freefunc);
    gCachedPreviews = nullptr;
  }
}

void BKE_previewimg_freefunc(void *link)
{
  PreviewImage *prv = (PreviewImage *)link;
  if (!prv) {
    return;
  }
  BKE_previewimg_free(&prv);
}

void BKE_previewimg_clear_single(PreviewImage *prv, enum eIconSizes size)
{
  MEM_SAFE_FREE(prv->rect[size]);
  if (prv->runtime->gputexture[size]) {
    GPU_texture_free(prv->runtime->gputexture[size]);
  }
  prv->h[size] = prv->w[size] = 0;
  prv->flag[size] |= PRV_CHANGED;
  prv->flag[size] &= ~PRV_USER_EDITED;
  prv->changed_timestamp[size] = 0;
}

void BKE_previewimg_clear(PreviewImage *prv)
{
  for (int i = 0; i < NUM_ICON_SIZES; i++) {
    BKE_previewimg_clear_single(prv, (eIconSizes)i);
  }
}

PreviewImage *BKE_previewimg_copy(const PreviewImage *prv)
{
  if (!prv) {
    return nullptr;
  }

  PreviewImage *prv_img = MEM_mallocN<PreviewImage>(__func__);
  *prv_img = blender::dna::shallow_copy(*prv);
  prv_img->runtime = MEM_new<blender::bke::PreviewImageRuntime>(__func__, *prv->runtime);

  for (int i = 0; i < NUM_ICON_SIZES; i++) {
    if (prv->rect[i]) {
      prv_img->rect[i] = (uint *)MEM_dupallocN(prv->rect[i]);
    }
    prv_img->runtime->gputexture[i] = nullptr;
  }

  return prv_img;
}

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
    new_id->icon_id = (*new_prv_p)->runtime->icon_id = 0;
  }
}

PreviewImage **BKE_previewimg_id_get_p(const ID *id)
{
  switch (GS(id->name)) {
#define ID_PRV_CASE(id_code, id_struct) \
  case id_code: { \
    return &((id_struct *)id)->preview; \
  } \
    ((void)0)
    ID_PRV_CASE(ID_OB, Object);
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
    ID_PRV_CASE(ID_NT, bNodeTree);
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
  if (!prv_p) {
    return nullptr;
  }

  if (*prv_p == nullptr) {
    *prv_p = BKE_previewimg_create();
  }
  return *prv_p;
}

void BKE_previewimg_id_custom_set(ID *id, const char *filepath)
{
  PreviewImage **prv = BKE_previewimg_id_get_p(id);

  /* Thumbnail previews must use the deferred pipeline. But we force them to be immediately
   * generated here still. */

  if (*prv) {
    BKE_previewimg_deferred_release(*prv);
  }
  *prv = previewimg_deferred_create(filepath, THB_SOURCE_IMAGE);

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
  return ELEM(GS(id->name), ID_OB, ID_MA, ID_TE, ID_LA, ID_WO, ID_IM, ID_BR, ID_GR, ID_SCE);
}

void BKE_previewimg_deferred_release(PreviewImage *prv)
{
  if (!prv) {
    return;
  }

  if (prv->runtime->tag & PRV_TAG_DEFFERED_RENDERING) {
    /* We cannot delete the preview while it is being loaded in another thread... */
    prv->runtime->tag |= PRV_TAG_DEFFERED_DELETE;
    return;
  }
  if (prv->runtime->icon_id) {
    BKE_icon_delete(prv->runtime->icon_id);
  }
  BKE_previewimg_free(&prv);
}

PreviewImage *BKE_previewimg_cached_get(const char *name)
{
  BLI_assert(BLI_thread_is_main());
  return (PreviewImage *)BLI_ghash_lookup(gCachedPreviews, name);
}

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

PreviewImage *BKE_previewimg_cached_thumbnail_read(const char *name,
                                                   const char *filepath,
                                                   const int source,
                                                   bool force_update)
{
  BLI_assert(BLI_thread_is_main());

  PreviewImage *prv = nullptr;
  void **prv_p;

  prv_p = BLI_ghash_lookup_p(gCachedPreviews, name);

  if (prv_p) {
    prv = static_cast<PreviewImage *>(*prv_p);
    BLI_assert(prv);
    BLI_assert(prv->runtime->deferred_loading_data);
  }

  if (prv && force_update) {
    if (prv->runtime->deferred_loading_data &&
        (prv->runtime->deferred_loading_data->source == source) &&
        (prv->runtime->deferred_loading_data->filepath == filepath))
    {
      /* If same filepath, no need to re-allocate preview, just clear it up. */
      BKE_previewimg_clear(prv);
    }
    else {
      BKE_previewimg_free(&prv);
    }
  }

  if (!prv) {
    prv = previewimg_deferred_create(filepath, ThumbSource(source));
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
  if (!gCachedPreviews) {
    /* Static cache was already freed including all contained previews. Can happen on shutdown. */
    return;
  }

  PreviewImage *prv = (PreviewImage *)BLI_ghash_popkey(gCachedPreviews, name, MEM_freeN);

  BKE_previewimg_deferred_release(prv);
}

void BKE_previewimg_ensure(PreviewImage *prv, const int size)
{
  if (!prv->runtime->deferred_loading_data) {
    return;
  }

  const bool do_icon = ((size == ICON_SIZE_ICON) && !prv->rect[ICON_SIZE_ICON]);
  const bool do_preview = ((size == ICON_SIZE_PREVIEW) && !prv->rect[ICON_SIZE_PREVIEW]);

  if (!(do_icon || do_preview)) {
    /* Nothing to do. */
    return;
  }

  const blender::bke::PreviewDeferredLoadingData &prv_deferred =
      *prv->runtime->deferred_loading_data;
  int icon_w, icon_h;

  ImBuf *thumb = IMB_thumb_manage(prv_deferred.filepath.c_str(), THB_LARGE, prv_deferred.source);
  if (!thumb) {
    return;
  }

  /* #PreviewImage assumes pre-multiplied alpha. */
  IMB_premultiply_alpha(thumb);

  if (do_preview) {
    prv->w[ICON_SIZE_PREVIEW] = thumb->x;
    prv->h[ICON_SIZE_PREVIEW] = thumb->y;
    prv->rect[ICON_SIZE_PREVIEW] = (uint *)MEM_dupallocN(thumb->byte_buffer.data);
    prv->flag[ICON_SIZE_PREVIEW] &= ~(PRV_CHANGED | PRV_USER_EDITED | PRV_RENDERING);
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

    IMB_scale(thumb, icon_w, icon_h, IMBScaleFilter::Box, false);
    prv->w[ICON_SIZE_ICON] = icon_w;
    prv->h[ICON_SIZE_ICON] = icon_h;
    prv->rect[ICON_SIZE_ICON] = (uint *)MEM_dupallocN(thumb->byte_buffer.data);
    prv->flag[ICON_SIZE_ICON] &= ~(PRV_CHANGED | PRV_USER_EDITED | PRV_RENDERING);
  }
  IMB_freeImBuf(thumb);
}

const char *BKE_previewimg_deferred_filepath_get(const PreviewImage *prv)
{
  if (!prv->runtime->deferred_loading_data) {
    return nullptr;
  }

  return prv->runtime->deferred_loading_data->filepath.c_str();
}

std::optional<int> BKE_previewimg_deferred_thumb_source_get(const PreviewImage *prv)
{
  if (!prv->runtime->deferred_loading_data) {
    return std::nullopt;
  }

  return prv->runtime->deferred_loading_data->source;
}

ImBuf *BKE_previewimg_to_imbuf(const PreviewImage *prv, const int size)
{
  const uint w = prv->w[size];
  const uint h = prv->h[size];
  const uint *rect = prv->rect[size];

  ImBuf *ima = nullptr;

  if (w > 0 && h > 0 && rect) {
    /* first allocate imbuf for copying preview into it */
    ima = IMB_allocImBuf(w, h, 32, IB_byte_data);
    memcpy(ima->byte_buffer.data, rect, w * h * sizeof(uint8_t) * 4);
  }

  return ima;
}

void BKE_previewimg_finish(PreviewImage *prv, const int size)
{
  /* Previews may be calculated on a thread. */
  atomic_fetch_and_and_int16(&prv->flag[size], ~PRV_RENDERING);
}

bool BKE_previewimg_is_finished(const PreviewImage *prv, const int size)
{
  return (prv->flag[size] & PRV_RENDERING) == 0;
}

bool BKE_previewimg_is_invalid(const PreviewImage *prv)
{
  return (prv->runtime->tag & PRV_TAG_DEFFERED_INVALID) != 0;
}

void BKE_previewimg_blend_write(BlendWriter *writer, const PreviewImage *prv)
{
  /* Note we write previews also for undo steps. It takes up some memory,
   * but not doing so would causes all previews to be re-rendered after
   * undo which is too expensive. */

  if (prv == nullptr) {
    return;
  }

  PreviewImage prv_copy = blender::dna::shallow_copy(*prv);
  prv_copy.runtime = nullptr;
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

  prv->runtime = MEM_new<blender::bke::PreviewImageRuntime>(__func__);

  for (int i = 0; i < NUM_ICON_SIZES; i++) {
    if (prv->rect[i]) {
      BLO_read_uint32_array(reader, prv->w[i] * prv->h[i], &prv->rect[i]);
    }

    /* PRV_RENDERING is a runtime only flag currently, but don't mess with it on undo! It gets
     * special handling in #memfile_undosys_restart_unfinished_id_previews() then. */
    if (!BLO_read_data_is_undo(reader)) {
      prv->flag[i] &= ~PRV_RENDERING;
    }
  }
}
