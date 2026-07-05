/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <mutex>

#include "MEM_guardedalloc.h"

#include "BLI_boxpack_2d.hh"
#include "BLI_listbase.hh"
#include "BLI_math_base.hh"
#include "BLI_math_base_c.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"
#include "BLI_time.hh"

#include "DNA_image_types.h"

#include "IMB_cache.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "BKE_image.hh"
#include "BKE_image_gpu.hh"
#include "BKE_main.hh"

#include "IMB_partial_update.hh"

#include "GPU_capabilities.hh"
#include "GPU_texture.hh"

#include "CLG_log.h"

#include "image_intern.hh"

namespace blender {

static CLG_LogRef LOG = {"image.gpu"};

using blender::imbuf::partial_update::Changes;

/* -------------------------------------------------------------------- */
/** \name UDIM image buffers for atlas and tile mapping
 * \{ */

static ImBuf *image_udim_gpu_cache_get(Image *image, ImageCacheKey key)
{
  if (image->runtime->cache == nullptr) {
    return nullptr;
  }
  ImBuf *ibuf = IMB_cache_get(image->runtime->cache, &key, nullptr);
  if (ibuf != nullptr) {
    ibuf->lastused = BLI_time_now_seconds_i();
  }
  return ibuf;
}

static ImBuf *image_udim_gpu_ibuf_get(Image *ima, const int udim_index)
{
  return image_udim_gpu_cache_get(ima, ImageCacheKey{.index = udim_index});
}

static ImBuf *image_udim_gpu_ibuf_ensure(Image *ima, const int udim_index)
{
  ImageCacheKey key = {.index = udim_index};
  ImBuf *ibuf = image_udim_gpu_cache_get(ima, key);
  if (ibuf == nullptr) {
    ibuf = IMB_allocImBuf(1, 1, ImBufFlags::Zero);
    imagecache_put(ima, key, ibuf);
  }
  return ibuf;
}

static void image_udim_gpu_ibuf_remove(Image *ima, const int udim_index)
{
  if (ima->runtime->cache == nullptr) {
    return;
  }
  ImageCacheKey key{.index = udim_index};
  IMB_cache_remove(ima->runtime->cache, &key);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Query State
 * \{ */

bool BKE_image_has_gpu_texture(Image *ima)
{
  if (ima->runtime->cache == nullptr) {
    return false;
  }

  std::scoped_lock lock(ima->runtime->cache_mutex);
  ImBufCacheIter *iter = IMB_cacheIter_new(ima->runtime->cache);
  bool found = false;
  while (!IMB_cacheIter_done(iter)) {
    ImBuf *ibuf = IMB_cacheIter_getImBuf(iter);
    if (ibuf != nullptr && ibuf->gpu.texture != nullptr) {
      found = true;
      break;
    }
    IMB_cacheIter_step(iter);
  }
  IMB_cacheIter_free(iter);
  return found;
}

bool BKE_image_has_gpu_texture_premultiplied_alpha(Image *image, ImBuf *ibuf)
{
  if (image) {
    /* Render result and compositor output are always premultiplied */
    if (ELEM(image->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE)) {
      return true;
    }
    /* Generated images use pre multiplied float buffer, but straight alpha for byte buffers. */
    if (image->type == IMA_TYPE_UV_TEST && ibuf) {
      return ibuf->float_data() != nullptr;
    }
  }
  if (ibuf) {
    if (ibuf->float_data()) {
      return image ? (image->alpha_mode != IMA_ALPHA_STRAIGHT) : false;
    }

    return image ? (image->alpha_mode == IMA_ALPHA_PREMUL) : true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UDIM GPU Texture
 * \{ */

static bool is_over_resolution_limit(int w, int h)
{
  return (w > GPU_texture_size_with_limit(w) || h > GPU_texture_size_with_limit(h));
}

static int smaller_power_of_2_limit(int num)
{
  return power_of_2_min_i(GPU_texture_size_with_limit(num));
}

static gpu::Texture *gpu_texture_create_tile_mapping(Image *ima, gpu::Texture *tilearray)
{
  if (tilearray == nullptr) {
    return nullptr;
  }

  float array_w = GPU_texture_width(tilearray);
  float array_h = GPU_texture_height(tilearray);

  /* Determine maximum tile number. */
  BKE_image_sort_tiles(ima);
  ImageTile *last_tile = static_cast<ImageTile *>(ima->tiles.last);
  int max_tile = last_tile->tile_number - 1001;

  /* create image */
  int width = max_tile + 1;
  float *data = MEM_new_array_zeroed<float>(size_t(width) * 8, __func__);
  for (int i = 0; i < width; i++) {
    data[4 * i] = -1.0f;
  }
  for (ImageTile &tile : ima->tiles) {
    int i = tile.tile_number - 1001;
    ImageTile_Runtime *tile_runtime = &tile.runtime;
    data[4 * i] = tile_runtime->tilearray_layer;

    float *tile_info = &data[4 * width + 4 * i];
    tile_info[0] = tile_runtime->tilearray_offset[0] / array_w;
    tile_info[1] = tile_runtime->tilearray_offset[1] / array_h;
    tile_info[2] = tile_runtime->tilearray_size[0] / array_w;
    tile_info[3] = tile_runtime->tilearray_size[1] / array_h;
  }

  gpu::Texture *tex = GPU_texture_create_1d_array(ima->id.name + 2,
                                                  width,
                                                  2,
                                                  1,
                                                  gpu::TextureFormat::SFLOAT_32_32_32_32,
                                                  GPU_TEXTURE_USAGE_SHADER_READ,
                                                  data);
  if (tex != nullptr) {
    GPU_texture_mipmap_mode(tex, false, false);
  }

  MEM_delete(data);

  return tex;
}

struct PackTile {
  FixedSizeBoxPack boxpack;
  ImageTile *tile;
  float pack_score;
};

static int compare_packtile(const void *a, const void *b)
{
  const PackTile *tile_a = static_cast<const PackTile *>(a);
  const PackTile *tile_b = static_cast<const PackTile *>(b);

  return tile_a->pack_score < tile_b->pack_score;
}

static gpu::Texture *gpu_texture_create_tile_array(Image *ima, ImBuf *main_ibuf)
{
  int arraywidth = 0, arrayheight = 0;
  ListBaseT<FixedSizeBoxPack> boxes = {nullptr};

  bool all_grayscale = true;

  for (ImageTile &tile : ima->tiles) {
    ImageUser iuser;
    BKE_imageuser_default(&iuser);
    iuser.tile = tile.tile_number;
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);

    if (ibuf) {
      PackTile *packtile = MEM_new_zeroed<PackTile>(__func__);
      packtile->tile = &tile;
      packtile->boxpack.w = ibuf->x;
      packtile->boxpack.h = ibuf->y;

      if (is_over_resolution_limit(packtile->boxpack.w, packtile->boxpack.h)) {
        packtile->boxpack.w = smaller_power_of_2_limit(packtile->boxpack.w);
        packtile->boxpack.h = smaller_power_of_2_limit(packtile->boxpack.h);
      }
      arraywidth = max_ii(arraywidth, packtile->boxpack.w);
      arrayheight = max_ii(arrayheight, packtile->boxpack.h);

      /* We sort the tiles by decreasing size, with an additional penalty term
       * for high aspect ratios. This improves packing efficiency. */
      float w = packtile->boxpack.w, h = packtile->boxpack.h;
      packtile->pack_score = max_ff(w, h) / min_ff(w, h) * w * h;

      BKE_image_release_ibuf(ima, ibuf, nullptr);
      BLI_addtail(&boxes, packtile);
      if (ibuf->color_mode != ImColorMode::BW) {
        all_grayscale = false;
      }
    }
  }

  BLI_assert(arraywidth > 0 && arrayheight > 0);

  BLI_listbase_sort(&boxes, compare_packtile);
  int arraylayers = 0;
  /* Keep adding layers until all tiles are packed. */
  while (boxes.first != nullptr) {
    ListBaseT<FixedSizeBoxPack> packed = {nullptr};
    BLI_box_pack_2d_fixedarea(&boxes, arraywidth, arrayheight, &packed);
    BLI_assert(packed.first != nullptr);

    for (const FixedSizeBoxPack &fixedpack : packed) {
      const PackTile *packtile = reinterpret_cast<const PackTile *>(&fixedpack);
      ImageTile *tile = packtile->tile;
      ImageTile_Runtime *tile_runtime = &tile->runtime;
      int *tileoffset = tile_runtime->tilearray_offset;
      int *tilesize = tile_runtime->tilearray_size;

      tileoffset[0] = packtile->boxpack.x;
      tileoffset[1] = packtile->boxpack.y;
      tilesize[0] = packtile->boxpack.w;
      tilesize[1] = packtile->boxpack.h;
      tile_runtime->tilearray_layer = arraylayers;
    }

    packed.free_no_destruct();
    arraylayers++;
  }

  const bool use_high_bitdepth = (ima->flag & IMA_HIGH_BITDEPTH);
  const bool use_grayscale = all_grayscale;
  /* Create Texture without content. */
  gpu::Texture *tex = IMB_touch_gpu_texture(ima->id.name + 2,
                                            main_ibuf,
                                            arraywidth,
                                            arrayheight,
                                            arraylayers,
                                            use_high_bitdepth,
                                            use_grayscale);

  if (!tex) {
    return nullptr;
  }

  /* Upload each tile one by one. */
  for (ImageTile &tile : ima->tiles) {
    const ImageTile_Runtime *tile_runtime = &tile.runtime;
    const int tilelayer = tile_runtime->tilearray_layer;
    const int *tileoffset = tile_runtime->tilearray_offset;
    const int *tilesize = tile_runtime->tilearray_size;

    if (tilesize[0] == 0 || tilesize[1] == 0) {
      continue;
    }

    ImageUser iuser;
    BKE_imageuser_default(&iuser);
    iuser.tile = tile.tile_number;
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);

    if (ibuf) {
      const bool store_premultiplied = BKE_image_has_gpu_texture_premultiplied_alpha(ima, ibuf);
      IMB_update_gpu_texture_sub(tex,
                                 ibuf,
                                 UNPACK2(tileoffset),
                                 tilelayer,
                                 UNPACK2(tilesize),
                                 use_grayscale,
                                 store_premultiplied);
    }

    BKE_image_release_ibuf(ima, ibuf, nullptr);
  }

  if (!(main_ibuf->gpu.flag & IMB_GPU_DISABLE_MIPMAP_UPDATE)) {
    GPU_texture_update_mipmap_chain(tex);
    GPU_texture_mipmap_mode(tex, true, true);
    main_ibuf->gpu.flag |= IMB_GPU_MIPMAP_COMPLETE;
  }
  else {
    GPU_texture_mipmap_mode(tex, false, true);
  }
  GPU_texture_original_size_set(tex, main_ibuf->x, main_ibuf->y);

  return tex;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Sequences
 * \{ */

/**
 * For a movie or image-sequence image, free the GPU textures of cached frames other than the
 * frame of #current_ibuf. This avoids excessive GPU memory usage during playback. This does
 * cause cache trashing when the same image sequence is used with different offsets.
 */
static void image_cache_free_inactive_frame_gpu_textures(Image *ima, const ImBuf *current_ibuf)
{
  if (!ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE) || ima->runtime->cache == nullptr) {
    return;
  }

  std::scoped_lock lock(ima->runtime->cache_mutex);

  /* The frame that backs the buffer currently in use. */
  const int current_frame = current_ibuf->fileframe;

  /* Free GPU textures of other frames. */
  ImBufCacheIter *iter = IMB_cacheIter_new(ima->runtime->cache);
  while (!IMB_cacheIter_done(iter)) {
    ImBuf *ibuf = IMB_cacheIter_getImBuf(iter);
    if (ibuf != nullptr && ibuf->gpu.texture != nullptr && ibuf->fileframe != current_frame &&
        ibuf->refcounter == 0)
    {
      IMB_free_gpu_textures(ibuf);
    }
    IMB_cacheIter_step(iter);
  }
  IMB_cacheIter_free(iter);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Error handling
 * \{ */

static ImBuf *g_error_imbuf = nullptr;

static ImBuf *image_gpu_error_imbuf_ensure()
{
  /* Create on demand so we have a GPU context available when creating. */
  if (g_error_imbuf == nullptr) {
    g_error_imbuf = IMB_allocImBuf(1, 1, ImBufFlags::Zero);
  }
  if (g_error_imbuf->gpu.texture == nullptr) {
    g_error_imbuf->gpu.texture = GPU_texture_create_error(2, false);
  }
  return g_error_imbuf;
}

static void image_gpu_log_load_error_once(Image *ima, ImageUser *iuser)
{
  if (ELEM(ima->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE)) {
    return;
  }
  if (ima->runtime->gpu_load_error_logged) {
    return;
  }
  ima->runtime->gpu_load_error_logged = true;

  char filepath[FILE_MAX];
  if (iuser != nullptr) {
    BKE_image_user_file_path(iuser, ima, filepath);
  }
  else {
    BLI_strncpy(filepath, ima->filepath, sizeof(filepath));
  }
  CLOG_ERROR(&LOG, "Failed to create texture for \"%s\"", filepath);
}

static void image_gpu_clear_load_error(Image *ima)
{
  ima->runtime->gpu_load_error_logged = false;
}

void BKE_image_free_gpu_fallback()
{
  if (g_error_imbuf == nullptr) {
    return;
  }
  if (g_error_imbuf->gpu.texture != nullptr) {
    GPU_texture_free(g_error_imbuf->gpu.texture);
    g_error_imbuf->gpu.texture = nullptr;
  }
  IMB_freeImBuf(g_error_imbuf);
  g_error_imbuf = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Get GPU texture from Image
 * \{ */

static void image_gpu_atlas_try_partial_update(Image *image, ImageUser *iuser)
{
  if (image->source != IMA_SRC_TILED) {
    return;
  }
  ImBuf *atlas_ibuf = image_udim_gpu_ibuf_get(image, IMA_INDEX_UDIM_ATLAS);
  if (atlas_ibuf == nullptr) {
    return;
  }
  gpu::Texture *atlas_tex = atlas_ibuf->gpu.texture;

  /* A resized tile invalidates the atlas packing and forces a rebuild of all tiles. */
  bool need_full_rebuild = false;

  bool need_mipmap_update = false;

  ImageUser tile_user = {};
  if (iuser != nullptr) {
    tile_user = *iuser;
  }
  else {
    tile_user.framenr = image->lastframe;
  }

  /* Get changeset ID that we will update to, and last changeset ID. */
  const int64_t new_changeset_id = BKE_image_partial_update_flush(image, &tile_user);
  const int64_t last_changeset_id = atlas_ibuf->gpu.partial_update_changeset;

  for (ImageTile &tile : image->tiles) {
    tile_user.tile = tile.tile_number;
    void *lock;
    ImBuf *ibuf = BKE_image_acquire_ibuf(image, &tile_user, &lock);
    if (ibuf != nullptr) {
      Changes changes = IMB_partial_update_collect(ibuf, last_changeset_id);
      switch (changes.kind) {
        case Changes::Kind::Resized:
          need_full_rebuild = true;
          break;
        case Changes::Kind::Full:
          changes.updated_regions.append({0, ibuf->x, 0, ibuf->y});
          [[fallthrough]];
        case Changes::Kind::Partial:
          if (atlas_tex != nullptr) {
            const bool store_premultiplied = BKE_image_has_gpu_texture_premultiplied_alpha(image,
                                                                                           ibuf);
            IMB_gpu_texture_apply_partial_update(atlas_tex,
                                                 ibuf,
                                                 store_premultiplied,
                                                 changes,
                                                 tile.runtime.tilearray_layer,
                                                 int2(tile.runtime.tilearray_offset),
                                                 int2(tile.runtime.tilearray_size));
            need_mipmap_update |= !(ibuf->gpu.flag & IMB_GPU_DISABLE_MIPMAP_UPDATE);
          }
          break;
        case Changes::Kind::None:
          break;
      }
    }
    BKE_image_release_ibuf(image, ibuf, lock);

    if (need_full_rebuild) {
      break;
    }
  }

  if (need_mipmap_update && !need_full_rebuild) {
    GPU_texture_update_mipmap_chain(atlas_tex);
    atlas_ibuf->gpu.flag |= IMB_GPU_MIPMAP_COMPLETE;
  }

  atlas_ibuf->gpu.partial_update_changeset = new_changeset_id;
  IMB_freeImBuf(atlas_ibuf);

  if (need_full_rebuild) {
    BKE_image_free_gpu_texture_caches(image);
  }
}

static ImageGPUTextures image_get_gpu_texture_tiled(Image *ima,
                                                    ImageUser *iuser,
                                                    const bool try_only)
{
  ImageGPUTextures result = {};
  result.need_tile_mapping = true;

  /* Get or create atlas and tile mapping image buffers. */
  ImBuf *atlas_ibuf, *mapping_ibuf;
  {
    std::scoped_lock lock(ima->runtime->cache_mutex);
    atlas_ibuf = image_udim_gpu_ibuf_ensure(ima, IMA_INDEX_UDIM_ATLAS);
    mapping_ibuf = image_udim_gpu_ibuf_ensure(ima, IMA_INDEX_UDIM_TILE_MAPPING);
  }

  /* Update time for garbage collection. */
  const int64_t now = BLI_time_now_seconds_i();
  atlas_ibuf->gpu.lastused = now;
  mapping_ibuf->gpu.lastused = now;

  /* Acquire textures if they exist. */
  result.texture = IMB_acquire_gpu_texture(
      ima->id.name + 2, atlas_ibuf, false, false, false, true);
  result.tile_mapping = IMB_acquire_gpu_texture(
      ima->id.name + 2, mapping_ibuf, false, false, false, true);

  if (try_only || (result.texture != nullptr && result.tile_mapping != nullptr)) {
    IMB_freeImBuf(atlas_ibuf);
    IMB_freeImBuf(mapping_ibuf);
    return result;
  }

  /* Recreate both textures in case one got freed (unlikely in practice). */
  if (result.texture) {
    GPU_texture_free(result.texture);
    result.texture = nullptr;
  }
  if (result.tile_mapping) {
    GPU_texture_free(result.tile_mapping);
    result.tile_mapping = nullptr;
  }

  /* Get changeset ID that we will update to. */
  const int64_t new_changeset_id = BKE_image_partial_update_flush(ima, nullptr);

  /* Acquire image buffer. */
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, nullptr);
  gpu::Texture *atlas_tex = nullptr;
  gpu::Texture *mapping_tex = nullptr;

  if (ibuf == nullptr) {
    image_gpu_log_load_error_once(ima, iuser);
  }
  else {
    /* Create atlas and tile mapping textures. */
    atlas_tex = gpu_texture_create_tile_array(ima, ibuf);
    if (atlas_tex) {
      mapping_tex = gpu_texture_create_tile_mapping(ima, atlas_tex);
    }
    if (atlas_tex && mapping_tex) {
      image_gpu_clear_load_error(ima);
    }
    else {
      image_gpu_log_load_error_once(ima, iuser);
    }
  }

  /* Set error texture if either failed to load. */
  if (atlas_tex == nullptr || mapping_tex == nullptr) {
    if (atlas_tex) {
      GPU_texture_free(atlas_tex);
    }
    if (mapping_tex) {
      GPU_texture_free(mapping_tex);
    }
    atlas_tex = GPU_texture_create_error(2, true);
    mapping_tex = GPU_texture_create_error(1, true);
  }

  BKE_image_release_ibuf(ima, ibuf, nullptr);

  /* Increase owned references for the result. */
  if (atlas_tex) {
    GPU_texture_ref(atlas_tex);
  }
  if (mapping_tex) {
    GPU_texture_ref(mapping_tex);
  }

  result.texture = atlas_tex;
  result.tile_mapping = mapping_tex;

  /* Assign to the image buffers, which takes the reference from creation. */
  IMB_assign_gpu_texture(atlas_ibuf, atlas_tex);
  IMB_assign_gpu_texture(mapping_ibuf, mapping_tex);

  atlas_ibuf->gpu.partial_update_changeset = new_changeset_id;
  mapping_ibuf->gpu.partial_update_changeset = new_changeset_id;

  IMB_freeImBuf(atlas_ibuf);
  IMB_freeImBuf(mapping_ibuf);

  return result;
}

static bool image_gpu_texture_fits_full_resolution(const ImBuf *ibuf)
{
  /* Check if this image buffer can fit in a GPU texture at full resolution. */
  const bool has_cpu_data = ibuf->float_data() || ibuf->byte_data();
  return !has_cpu_data || (GPU_is_safe_texture_size(ibuf->x, ibuf->y) &&
                           GPU_texture_size_with_limit(ibuf->x) == ibuf->x &&
                           GPU_texture_size_with_limit(ibuf->y) == ibuf->y);
}

static ImageGPUTextures image_get_gpu_texture_single(Image *ima,
                                                     ImageUser *iuser,
                                                     const bool use_viewers,
                                                     const bool only_full_resolution,
                                                     const bool try_only)
{
  ImageGPUTextures result = {};

  /* Acquire the image buffer. */
  void *lock = nullptr;
  bool cpu_load_failed = false;
  ImBuf *ibuf = BKE_image_acquire_ibuf_gpu(
      ima, iuser, use_viewers ? &lock : nullptr, &cpu_load_failed);

  bool gpu_load_failed = false;
  if (ibuf != nullptr && (!only_full_resolution || image_gpu_texture_fits_full_resolution(ibuf))) {
    /* Acquire a reference to the GPU texture. */
    const bool use_high_bitdepth = (ima->flag & IMA_HIGH_BITDEPTH);
    const bool store_premultiplied = BKE_image_has_gpu_texture_premultiplied_alpha(ima, ibuf);
    gpu::Texture *tex = IMB_acquire_gpu_texture(
        ima->id.name + 2, ibuf, use_high_bitdepth, store_premultiplied, true, try_only);
    if (tex) {
      GPU_texture_original_size_set(tex, ibuf->x, ibuf->y);
      image_gpu_clear_load_error(ima);
    }
    else if (!try_only) {
      image_gpu_log_load_error_once(ima, iuser);
    }
    if (!try_only) {
      image_cache_free_inactive_frame_gpu_textures(ima, ibuf);
    }
    result.texture = tex;
    gpu_load_failed = (tex == nullptr) && (ibuf->gpu.flag & IMB_GPU_LOAD_FAILED);
  }

  /* Release image buffer. */
  BKE_image_release_ibuf(ima, ibuf, lock);

  /* Return error texture if failed to load, including in try_only mode. This
   * way the caller will not consider it as still needing to be loaded. */
  if (result.texture == nullptr && (!try_only || cpu_load_failed || gpu_load_failed) &&
      !only_full_resolution)
  {
    image_gpu_log_load_error_once(ima, iuser);
    ImBuf *error_ibuf = image_gpu_error_imbuf_ensure();
    result.texture = IMB_acquire_gpu_texture(ima->id.name + 2, error_ibuf, false, false, false);
  }

  return result;
}

/* Returns the GPU textures representing the given image with the given image user. The image
 * texture cache is checked first and if cached texture exist, they will be returned. If try_only
 * is true, nullptr textures will be returned if no cached textures exists, otherwise, the textures
 * will be generated and added to the cached.
 *
 * If use_viewers is true, the image buffer will be acquired with locking to allow retrieval of
 * images of type viewer. If use_tile_mapping is true and the image is a tiled images, the
 * returned texture will be a 2D texture array with a mapping texture to sampling the image at
 * arbitrary tiles, otherwise, only the tile in the image user will be retrieved. */
static ImageGPUTextures image_get_gpu_texture(Image *ima,
                                              ImageUser *iuser,
                                              const bool use_viewers,
                                              const bool only_full_resolution,
                                              const bool use_tile_mapping,
                                              const bool try_only)
{
  if (ima == nullptr) {
    return {};
  }

  image_gpu_atlas_try_partial_update(ima, iuser);

  const bool tiled = (use_tile_mapping && ima->source == IMA_SRC_TILED);
  return tiled ?
             image_get_gpu_texture_tiled(ima, iuser, try_only) :
             image_get_gpu_texture_single(ima, iuser, use_viewers, only_full_resolution, try_only);
}

gpu::Texture *BKE_image_acquire_gpu_texture(Image *image, ImageUser *iuser)
{
  return image_get_gpu_texture(image, iuser, false, false, false, false).texture;
}

void BKE_image_assign_gpu_texture(Image *image, gpu::Texture *texture)
{
  /* Re-home an externally-created texture (e.g. a look-dev studio light) onto the image's
   * #ImBuf, which is where single-image GPU textures live. */
  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(image, nullptr, &lock);
  if (ibuf != nullptr) {
    IMB_assign_gpu_texture(ibuf, texture);
  }
  BKE_image_release_ibuf(image, ibuf, lock);
}

gpu::Texture *BKE_image_acquire_gpu_viewer_texture(Image *image,
                                                   ImageUser *iuser,
                                                   const bool only_full_resolution)
{
  return image_get_gpu_texture(image, iuser, true, only_full_resolution, false, false).texture;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material textures
 * \{ */

ImageGPUTextures BKE_image_acquire_gpu_material_texture(Image *image,
                                                        ImageUser *iuser,
                                                        const bool use_tile_mapping,
                                                        const bool try_only)
{
  return image_get_gpu_texture(image, iuser, false, false, use_tile_mapping, try_only);
}

bool BKE_image_has_gpu_material_texture(Image *image,
                                        ImageUser *iuser,
                                        const bool use_tile_mapping)
{
  const bool try_only = true;
  ImageGPUTextures result = image_get_gpu_texture(
      image, iuser, false, false, use_tile_mapping, try_only);
  const bool has_texture = result.texture != nullptr;

  /* Release reference, stays owned by the image buffer. */
  if (result.texture) {
    GPU_texture_free(result.texture);
  }
  if (result.tile_mapping) {
    GPU_texture_free(result.tile_mapping);
  }

  return has_texture;
}

void BKE_image_ensure_gpu_material_texture(Image *image,
                                           ImageUser *iuser,
                                           const bool use_tile_mapping)
{
  ImageGPUTextures result = image_get_gpu_texture(
      image, iuser, false, false, use_tile_mapping, false);

  /* Release reference, stays owned by the image buffer. */
  if (result.texture) {
    GPU_texture_free(result.texture);
  }
  if (result.tile_mapping) {
    GPU_texture_free(result.tile_mapping);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deletion
 * \{ */

void BKE_image_free_gpu_udim_textures(Image *ima)
{
  image_udim_gpu_ibuf_remove(ima, IMA_INDEX_UDIM_ATLAS);
  image_udim_gpu_ibuf_remove(ima, IMA_INDEX_UDIM_TILE_MAPPING);
}

void BKE_image_free_gpu_texture_caches(Image *ima)
{
  /* For Viewer images, the GPU texture can be generated directly by the compositor
   * without a CPU buffer, so it's not a cache and must be preserved. This is a
   * crude check, for future more general GPU image buffer support ImBuf will need
   * to carry this information. */
  if (ima->source == IMA_SRC_VIEWER) {
    return;
  }

  if (ima->runtime->cache) {
    std::scoped_lock lock(ima->runtime->cache_mutex);

    BKE_image_free_gpu_udim_textures(ima);

    ImBufCacheIter *iter = IMB_cacheIter_new(ima->runtime->cache);
    while (!IMB_cacheIter_done(iter)) {
      ImBuf *ibuf = IMB_cacheIter_getImBuf(iter);
      if (ibuf != nullptr) {
        IMB_free_gpu_textures(ibuf);
      }
      IMB_cacheIter_step(iter);
    }
    IMB_cacheIter_free(iter);
  }
}

void BKE_image_free_all_gpu_texture_caches(Main *bmain)
{
  if (bmain) {
    for (Image &ima : bmain->images) {
      BKE_image_free_gpu_texture_caches(&ima);
    }
  }
}

void BKE_image_free_anim_gpu_texture_caches(Main *bmain)
{
  if (bmain) {
    for (Image &ima : bmain->images) {
      if (BKE_image_is_animated(&ima)) {
        BKE_image_free_gpu_texture_caches(&ima);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paint Update
 * \{ */

void BKE_image_paint_set_mipmap(Main *bmain, bool mipmap)
{
  for (Image &ima : bmain->images) {
    if (ima.runtime->cache == nullptr) {
      continue;
    }
    std::scoped_lock lock(ima.runtime->cache_mutex);
    ImBufCacheIter *iter = IMB_cacheIter_new(ima.runtime->cache);
    while (!IMB_cacheIter_done(iter)) {
      ImBuf *ibuf = IMB_cacheIter_getImBuf(iter);
      const ImageCacheKey *key = static_cast<const ImageCacheKey *>(
          IMB_cacheIter_getUserKey(iter));
      if (ibuf != nullptr && ibuf->gpu.texture != nullptr &&
          key->index != IMA_INDEX_UDIM_TILE_MAPPING)
      {
        if (ibuf->gpu.flag & IMB_GPU_MIPMAP_COMPLETE) {
          GPU_texture_mipmap_mode(ibuf->gpu.texture, mipmap, true);
        }
        else {
          IMB_free_gpu_textures(ibuf);
        }
      }
      IMB_cacheIter_step(iter);
    }
    IMB_cacheIter_free(iter);
  }
}

/** \} */

int64_t BKE_image_partial_update_flush(Image *ima, const ImageUser *iuser)
{
  ImageUser tile_user;
  BKE_imageuser_default(&tile_user);
  if (iuser != nullptr) {
    tile_user = *iuser;
  }

  for (ImageTile &tile : ima->tiles) {
    tile_user.tile = tile.tile_number;
    void *lock;
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &tile_user, &lock);
    if (ibuf != nullptr) {
      IMB_partial_update_flush(ibuf);
    }
    BKE_image_release_ibuf(ima, ibuf, lock);
  }

  return IMB_partial_update_changeset_id_current();
}

}  // namespace blender
