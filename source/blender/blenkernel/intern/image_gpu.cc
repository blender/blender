/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_boxpack_2d.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_time.h"

#include "DNA_image_types.h"
#include "DNA_userdef_types.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "BKE_global.hh"
#include "BKE_image.h"
#include "BKE_image_partial_update.hh"
#include "BKE_main.hh"

#include "GPU_capabilities.h"
#include "GPU_state.h"
#include "GPU_texture.h"

using namespace blender::bke::image::partial_update;

extern "C" {

/* Prototypes. */
static void gpu_free_unused_buffers();
static void image_free_gpu(Image *ima, const bool immediate);
static void image_update_gputexture_ex(
    Image *ima, ImageTile *tile, ImBuf *ibuf, int x, int y, int w, int h);

bool BKE_image_has_gpu_texture_premultiplied_alpha(Image *image, ImBuf *ibuf)
{
  if (image) {
    /* Render result and compositor output are always premultiplied */
    if (ELEM(image->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE)) {
      return true;
    }
    /* Generated images use pre multiplied float buffer, but straight alpha for byte buffers. */
    if (image->type == IMA_TYPE_UV_TEST && ibuf) {
      return ibuf->float_buffer.data != nullptr;
    }
  }
  if (ibuf) {
    if (ibuf->float_buffer.data) {
      return image ? (image->alpha_mode != IMA_ALPHA_STRAIGHT) : false;
    }

    return image ? (image->alpha_mode == IMA_ALPHA_PREMUL) : true;
  }
  return false;
}

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

static GPUTexture *gpu_texture_create_tile_mapping(Image *ima, const int multiview_eye)
{
  GPUTexture *tilearray = ima->gputexture[TEXTARGET_2D_ARRAY][multiview_eye];

  if (tilearray == nullptr) {
    return nullptr;
  }

  float array_w = GPU_texture_width(tilearray);
  float array_h = GPU_texture_height(tilearray);

  /* Determine maximum tile number. */
  BKE_image_sort_tiles(ima);
  ImageTile *last_tile = (ImageTile *)ima->tiles.last;
  int max_tile = last_tile->tile_number - 1001;

  /* create image */
  int width = max_tile + 1;
  float *data = (float *)MEM_callocN(width * 8 * sizeof(float), __func__);
  for (int i = 0; i < width; i++) {
    data[4 * i] = -1.0f;
  }
  LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
    int i = tile->tile_number - 1001;
    ImageTile_Runtime *tile_runtime = &tile->runtime;
    data[4 * i] = tile_runtime->tilearray_layer;

    float *tile_info = &data[4 * width + 4 * i];
    tile_info[0] = tile_runtime->tilearray_offset[0] / array_w;
    tile_info[1] = tile_runtime->tilearray_offset[1] / array_h;
    tile_info[2] = tile_runtime->tilearray_size[0] / array_w;
    tile_info[3] = tile_runtime->tilearray_size[1] / array_h;
  }

  GPUTexture *tex = GPU_texture_create_1d_array(
      ima->id.name + 2, width, 2, 1, GPU_RGBA32F, GPU_TEXTURE_USAGE_SHADER_READ, data);
  GPU_texture_mipmap_mode(tex, false, false);

  MEM_freeN(data);

  return tex;
}

struct PackTile {
  FixedSizeBoxPack boxpack;
  ImageTile *tile;
  float pack_score;
};

static int compare_packtile(const void *a, const void *b)
{
  const PackTile *tile_a = (const PackTile *)a;
  const PackTile *tile_b = (const PackTile *)b;

  return tile_a->pack_score < tile_b->pack_score;
}

static GPUTexture *gpu_texture_create_tile_array(Image *ima, ImBuf *main_ibuf)
{
  int arraywidth = 0, arrayheight = 0;
  ListBase boxes = {nullptr};

  int planes = 0;

  LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
    ImageUser iuser;
    BKE_imageuser_default(&iuser);
    iuser.tile = tile->tile_number;
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);

    if (ibuf) {
      PackTile *packtile = MEM_cnew<PackTile>(__func__);
      packtile->tile = tile;
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
      planes = max_ii(planes, ibuf->planes);
    }
  }

  BLI_assert(arraywidth > 0 && arrayheight > 0);

  BLI_listbase_sort(&boxes, compare_packtile);
  int arraylayers = 0;
  /* Keep adding layers until all tiles are packed. */
  while (boxes.first != nullptr) {
    ListBase packed = {nullptr};
    BLI_box_pack_2d_fixedarea(&boxes, arraywidth, arrayheight, &packed);
    BLI_assert(packed.first != nullptr);

    LISTBASE_FOREACH (PackTile *, packtile, &packed) {
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

    BLI_freelistN(&packed);
    arraylayers++;
  }

  const bool use_high_bitdepth = (ima->flag & IMA_HIGH_BITDEPTH);
  const bool use_grayscale = planes <= 8;
  /* Create Texture without content. */
  GPUTexture *tex = IMB_touch_gpu_texture(ima->id.name + 2,
                                          main_ibuf,
                                          arraywidth,
                                          arrayheight,
                                          arraylayers,
                                          use_high_bitdepth,
                                          use_grayscale);

  /* Upload each tile one by one. */
  LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
    ImageTile_Runtime *tile_runtime = &tile->runtime;
    int tilelayer = tile_runtime->tilearray_layer;
    int *tileoffset = tile_runtime->tilearray_offset;
    int *tilesize = tile_runtime->tilearray_size;

    if (tilesize[0] == 0 || tilesize[1] == 0) {
      continue;
    }

    ImageUser iuser;
    BKE_imageuser_default(&iuser);
    iuser.tile = tile->tile_number;
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);

    if (ibuf) {
      const bool store_premultiplied = BKE_image_has_gpu_texture_premultiplied_alpha(ima, ibuf);
      IMB_update_gpu_texture_sub(tex,
                                 ibuf,
                                 UNPACK2(tileoffset),
                                 tilelayer,
                                 UNPACK2(tilesize),
                                 use_high_bitdepth,
                                 use_grayscale,
                                 store_premultiplied);
    }

    BKE_image_release_ibuf(ima, ibuf, nullptr);
  }

  if (GPU_mipmap_enabled()) {
    GPU_texture_update_mipmap_chain(tex);
    GPU_texture_mipmap_mode(tex, true, true);
    if (ima) {
      ima->gpuflag |= IMA_GPU_MIPMAP_COMPLETE;
    }
  }
  else {
    GPU_texture_mipmap_mode(tex, false, true);
  }

  return tex;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Regular gpu texture
 * \{ */

static GPUTexture **get_image_gpu_texture_ptr(Image *ima,
                                              eGPUTextureTarget textarget,
                                              const int multiview_eye)
{
  const bool in_range = (int(textarget) >= 0) && (textarget < TEXTARGET_COUNT);
  BLI_assert(in_range);
  BLI_assert(ELEM(multiview_eye, 0, 1));

  if (in_range) {
    return &(ima->gputexture[textarget][multiview_eye]);
  }
  return nullptr;
}

static GPUTexture *image_gpu_texture_error_create(eGPUTextureTarget textarget)
{
  fprintf(stderr, "GPUTexture: Blender Texture Not Loaded!\n");
  switch (textarget) {
    case TEXTARGET_2D_ARRAY:
      return GPU_texture_create_error(2, true);
    case TEXTARGET_TILE_MAPPING:
      return GPU_texture_create_error(1, true);
    case TEXTARGET_2D:
    default:
      return GPU_texture_create_error(2, false);
  }
}

static void image_gpu_texture_partial_update_changes_available(
    Image *image, PartialUpdateChecker<ImageTileData>::CollectResult &changes)
{
  while (changes.get_next_change() == ePartialUpdateIterResult::ChangeAvailable) {
    /* Calculate the clipping region with the tile buffer.
     * TODO(jbakker): should become part of ImageTileData to deduplicate with image engine. */
    rcti buffer_rect;
    BLI_rcti_init(
        &buffer_rect, 0, changes.tile_data.tile_buffer->x, 0, changes.tile_data.tile_buffer->y);
    rcti clipped_update_region;
    const bool has_overlap = BLI_rcti_isect(
        &buffer_rect, &changes.changed_region.region, &clipped_update_region);
    if (!has_overlap) {
      continue;
    }

    image_update_gputexture_ex(image,
                               changes.tile_data.tile,
                               changes.tile_data.tile_buffer,
                               clipped_update_region.xmin,
                               clipped_update_region.ymin,
                               BLI_rcti_size_x(&clipped_update_region),
                               BLI_rcti_size_y(&clipped_update_region));
  }
}

static void image_gpu_texture_try_partial_update(Image *image, ImageUser *iuser)
{
  PartialUpdateChecker<ImageTileData> checker(image, iuser, image->runtime.partial_update_user);
  PartialUpdateChecker<ImageTileData>::CollectResult changes = checker.collect_changes();
  switch (changes.get_result_code()) {
    case ePartialUpdateCollectResult::FullUpdateNeeded: {
      image_free_gpu(image, true);
      break;
    }

    case ePartialUpdateCollectResult::PartialChangesDetected: {
      image_gpu_texture_partial_update_changes_available(image, changes);
      break;
    }

    case ePartialUpdateCollectResult::NoChangesDetected: {
      /* GPUTextures are up to date. */
      break;
    }
  }
}

void BKE_image_ensure_gpu_texture(Image *image, ImageUser *image_user)
{
  if (!image) {
    return;
  }

  /* Note that the image can cache both stereo views, so we only invalidate the cache if the view
   * index is more than 2. */
  if (image->gpu_pass != image_user->pass || image->gpu_layer != image_user->layer ||
      (image->gpu_view != image_user->multi_index && image_user->multi_index >= 2))
  {
    BKE_image_partial_update_mark_full_update(image);
  }
}

static ImageGPUTextures image_get_gpu_texture(Image *ima,
                                              ImageUser *iuser,
                                              const bool use_viewers,
                                              const bool use_tile_mapping)
{
  ImageGPUTextures result = {};

  if (ima == nullptr) {
    return result;
  }

  /* Free any unused GPU textures, since we know we are in a thread with OpenGL
   * context and might as well ensure we have as much space free as possible. */
  gpu_free_unused_buffers();

  /* Free GPU textures when requesting a different render pass/layer.
   * When `iuser` isn't set (texture painting single image mode) we assume that
   * the current `pass` and `layer` should be 0. */
  short requested_pass = iuser ? iuser->pass : 0;
  short requested_layer = iuser ? iuser->layer : 0;
  short requested_view = iuser ? iuser->multi_index : 0;
  /* There is room for 2 multiview textures. When a higher number is requested we should always
   * target the first view slot. This is fine as multi view images aren't used together. */
  if (requested_view < 2) {
    requested_view = 0;
  }
  if (ima->gpu_pass != requested_pass || ima->gpu_layer != requested_layer ||
      ima->gpu_view != requested_view)
  {
    ima->gpu_pass = requested_pass;
    ima->gpu_layer = requested_layer;
    ima->gpu_view = requested_view;
    /* The cache should be invalidated here, but it is intentionally isn't due to possible
     * performance implications, see the BKE_image_ensure_gpu_texture function for more
     * information. */
  }
#undef GPU_FLAGS_TO_CHECK

  if (ima->runtime.partial_update_user == nullptr) {
    ima->runtime.partial_update_user = BKE_image_partial_update_create(ima);
  }

  image_gpu_texture_try_partial_update(ima, iuser);

  /* Tag as in active use for garbage collector. */
  BKE_image_tag_time(ima);

  /* Test if we need to get a tiled array texture. */
  eGPUTextureTarget textarget = (use_tile_mapping && ima->source == IMA_SRC_TILED) ?
                                    TEXTARGET_2D_ARRAY :
                                    TEXTARGET_2D;

  /* Test if we already have a texture. */
  int current_view = iuser ? iuser->multi_index : 0;
  if (current_view >= 2) {
    current_view = 0;
  }
  GPUTexture **tex = get_image_gpu_texture_ptr(ima, textarget, current_view);
  if (*tex) {
    result.texture = *tex;
    result.tile_mapping = *get_image_gpu_texture_ptr(ima, TEXTARGET_TILE_MAPPING, current_view);
    return result;
  }

  /* Check if we have a valid image. If not, we return a dummy
   * texture with zero bind-code so we don't keep trying. */
  ImageTile *tile = BKE_image_get_tile(ima, 0);
  if (tile == nullptr) {
    *tex = image_gpu_texture_error_create(textarget);
    result.texture = *tex;
    return result;
  }

  /* check if we have a valid image buffer */
  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, (use_viewers) ? &lock : nullptr);
  if (ibuf == nullptr) {
    BKE_image_release_ibuf(ima, ibuf, (use_viewers) ? lock : nullptr);
    *tex = image_gpu_texture_error_create(textarget);
    result.texture = *tex;

    return result;
  }

  if (textarget == TEXTARGET_2D_ARRAY) {
    /* For materials, array and tile mapping in case there are UDIM tiles. */
    *tex = gpu_texture_create_tile_array(ima, ibuf);
    result.texture = *tex;

    GPUTexture **tile_mapping_tex = get_image_gpu_texture_ptr(
        ima, TEXTARGET_TILE_MAPPING, current_view);
    *tile_mapping_tex = gpu_texture_create_tile_mapping(ima, iuser ? iuser->multiview_eye : 0);
    result.tile_mapping = *tile_mapping_tex;
  }
  else {
    /* Single image texture. */
    const bool use_high_bitdepth = (ima->flag & IMA_HIGH_BITDEPTH);
    const bool store_premultiplied = BKE_image_has_gpu_texture_premultiplied_alpha(ima, ibuf);

    *tex = IMB_create_gpu_texture(ima->id.name + 2, ibuf, use_high_bitdepth, store_premultiplied);
    result.texture = *tex;

    if (*tex) {
      GPU_texture_extend_mode(*tex, GPU_SAMPLER_EXTEND_MODE_REPEAT);

      if (GPU_mipmap_enabled()) {
        GPU_texture_update_mipmap_chain(*tex);
        if (ima) {
          ima->gpuflag |= IMA_GPU_MIPMAP_COMPLETE;
        }
        GPU_texture_mipmap_mode(*tex, true, true);
      }
      else {
        GPU_texture_mipmap_mode(*tex, false, true);
      }
    }
  }

  if (*tex) {
    GPU_texture_original_size_set(*tex, ibuf->x, ibuf->y);
  }

  BKE_image_release_ibuf(ima, ibuf, (use_viewers) ? lock : nullptr);

  return result;
}

GPUTexture *BKE_image_get_gpu_texture(Image *image, ImageUser *iuser)
{
  return image_get_gpu_texture(image, iuser, false, false).texture;
}

GPUTexture *BKE_image_get_gpu_viewer_texture(Image *image, ImageUser *iuser)
{
  return image_get_gpu_texture(image, iuser, true, false).texture;
}

ImageGPUTextures BKE_image_get_gpu_material_texture(Image *image,
                                                    ImageUser *iuser,
                                                    const bool use_tile_mapping)
{
  return image_get_gpu_texture(image, iuser, false, use_tile_mapping);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delayed GPU texture free
 *
 * Image datablocks can be deleted by any thread, but there may not be any active OpenGL context.
 * In that case we push them into a queue and free the buffers later.
 * \{ */

static LinkNode *gpu_texture_free_queue = nullptr;
static ThreadMutex gpu_texture_queue_mutex = BLI_MUTEX_INITIALIZER;

static void gpu_free_unused_buffers()
{
  if (gpu_texture_free_queue == nullptr) {
    return;
  }

  BLI_mutex_lock(&gpu_texture_queue_mutex);

  while (gpu_texture_free_queue != nullptr) {
    GPUTexture *tex = static_cast<GPUTexture *>(BLI_linklist_pop(&gpu_texture_free_queue));
    GPU_texture_free(tex);
  }

  BLI_mutex_unlock(&gpu_texture_queue_mutex);
}

void BKE_image_free_unused_gpu_textures()
{
  if (BLI_thread_is_main()) {
    gpu_free_unused_buffers();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deletion
 * \{ */

static void image_free_gpu(Image *ima, const bool immediate)
{
  for (int eye = 0; eye < 2; eye++) {
    for (int i = 0; i < TEXTARGET_COUNT; i++) {
      if (ima->gputexture[i][eye] != nullptr) {
        if (immediate) {
          GPU_texture_free(ima->gputexture[i][eye]);
        }
        else {
          BLI_mutex_lock(&gpu_texture_queue_mutex);
          BLI_linklist_prepend(&gpu_texture_free_queue, ima->gputexture[i][eye]);
          BLI_mutex_unlock(&gpu_texture_queue_mutex);
        }

        ima->gputexture[i][eye] = nullptr;
      }
    }
  }

  ima->gpuflag &= ~IMA_GPU_MIPMAP_COMPLETE;
}

void BKE_image_free_gputextures(Image *ima)
{
  image_free_gpu(ima, BLI_thread_is_main());
}

void BKE_image_free_all_gputextures(Main *bmain)
{
  if (bmain) {
    LISTBASE_FOREACH (Image *, ima, &bmain->images) {
      BKE_image_free_gputextures(ima);
    }
  }
}

void BKE_image_free_anim_gputextures(Main *bmain)
{
  if (bmain) {
    LISTBASE_FOREACH (Image *, ima, &bmain->images) {
      if (BKE_image_is_animated(ima)) {
        BKE_image_free_gputextures(ima);
      }
    }
  }
}

void BKE_image_free_old_gputextures(Main *bmain)
{
  static int lasttime = 0;
  int ctime = int(BLI_check_seconds_timer());

  /*
   * Run garbage collector once for every collecting period of time
   * if textimeout is 0, that's the option to NOT run the collector
   */
  if (U.textimeout == 0 || ctime % U.texcollectrate || ctime == lasttime) {
    return;
  }

  /* of course not! */
  if (G.is_rendering) {
    return;
  }

  lasttime = ctime;

  LISTBASE_FOREACH (Image *, ima, &bmain->images) {
    if ((ima->flag & IMA_NOCOLLECT) == 0 && ctime - ima->lastused > U.textimeout) {
      /* If it's in GL memory, deallocate and set time tag to current time
       * This gives textures a "second chance" to be used before dying. */
      if (BKE_image_has_opengl_texture(ima)) {
        BKE_image_free_gputextures(ima);
        ima->lastused = ctime;
      }
      /* Otherwise, just kill the buffers */
      else {
        BKE_image_free_buffers(ima);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paint Update
 * \{ */

static ImBuf *update_do_scale(uchar *rect,
                              float *rect_float,
                              int *x,
                              int *y,
                              int *w,
                              int *h,
                              int limit_w,
                              int limit_h,
                              int full_w,
                              int full_h)
{
  /* Partial update with scaling. */
  float xratio = limit_w / float(full_w);
  float yratio = limit_h / float(full_h);

  int part_w = *w, part_h = *h;

  /* Find sub coordinates in scaled image. Take ceiling because we will be
   * losing 1 pixel due to rounding errors in x,y. */
  *x *= xratio;
  *y *= yratio;
  *w = int(ceil(xratio * (*w)));
  *h = int(ceil(yratio * (*h)));

  /* ...but take back if we are over the limit! */
  if (*x + *w > limit_w) {
    (*w)--;
  }
  if (*y + *h > limit_h) {
    (*h)--;
  }

  /* Scale pixels. */
  ImBuf *ibuf = IMB_allocFromBuffer(rect, rect_float, part_w, part_h, 4);
  IMB_scaleImBuf(ibuf, *w, *h);

  return ibuf;
}

static void gpu_texture_update_scaled(GPUTexture *tex,
                                      uchar *rect,
                                      float *rect_float,
                                      int full_w,
                                      int full_h,
                                      int x,
                                      int y,
                                      int layer,
                                      const int *tile_offset,
                                      const int *tile_size,
                                      int w,
                                      int h)
{
  ImBuf *ibuf;
  if (layer > -1) {
    ibuf = update_do_scale(
        rect, rect_float, &x, &y, &w, &h, tile_size[0], tile_size[1], full_w, full_h);

    /* Shift to account for tile packing. */
    x += tile_offset[0];
    y += tile_offset[1];
  }
  else {
    /* Partial update with scaling. */
    int limit_w = GPU_texture_width(tex);
    int limit_h = GPU_texture_height(tex);

    ibuf = update_do_scale(rect, rect_float, &x, &y, &w, &h, limit_w, limit_h, full_w, full_h);
  }

  void *data = (ibuf->float_buffer.data) ? (void *)(ibuf->float_buffer.data) :
                                           (void *)(ibuf->byte_buffer.data);
  eGPUDataFormat data_format = (ibuf->float_buffer.data) ? GPU_DATA_FLOAT : GPU_DATA_UBYTE;

  GPU_texture_update_sub(tex, data_format, data, x, y, layer, w, h, 1);

  IMB_freeImBuf(ibuf);
}

static void gpu_texture_update_unscaled(GPUTexture *tex,
                                        uchar *rect,
                                        float *rect_float,
                                        int x,
                                        int y,
                                        int layer,
                                        const int tile_offset[2],
                                        int w,
                                        int h,
                                        int tex_stride,
                                        int tex_offset)
{
  if (layer > -1) {
    /* Shift to account for tile packing. */
    x += tile_offset[0];
    y += tile_offset[1];
  }

  void *data = (rect_float) ? (void *)(rect_float + tex_offset) : (void *)(rect + tex_offset);
  eGPUDataFormat data_format = (rect_float) ? GPU_DATA_FLOAT : GPU_DATA_UBYTE;

  /* Partial update without scaling. Stride and offset are used to copy only a
   * subset of a possible larger buffer than what we are updating. */
  GPU_unpack_row_length_set(tex_stride);

  GPU_texture_update_sub(tex, data_format, data, x, y, layer, w, h, 1);
  /* Restore default. */
  GPU_unpack_row_length_set(0);
}

static void gpu_texture_update_from_ibuf(
    GPUTexture *tex, Image *ima, ImBuf *ibuf, ImageTile *tile, int x, int y, int w, int h)
{
  bool scaled;
  if (tile != nullptr) {
    ImageTile_Runtime *tile_runtime = &tile->runtime;
    int *tilesize = tile_runtime->tilearray_size;
    scaled = (ibuf->x != tilesize[0]) || (ibuf->y != tilesize[1]);
  }
  else {
    scaled = (GPU_texture_width(tex) != ibuf->x) || (GPU_texture_height(tex) != ibuf->y);
  }

  if (scaled) {
    /* Extra padding to account for bleed from neighboring pixels. */
    const int padding = 4;
    const int xmax = min_ii(x + w + padding, ibuf->x);
    const int ymax = min_ii(y + h + padding, ibuf->y);
    x = max_ii(x - padding, 0);
    y = max_ii(y - padding, 0);
    w = xmax - x;
    h = ymax - y;
  }

  /* Get texture data pointers. */
  float *rect_float = ibuf->float_buffer.data;
  uchar *rect = ibuf->byte_buffer.data;
  int tex_stride = ibuf->x;
  int tex_offset = ibuf->channels * (y * ibuf->x + x);

  const bool store_premultiplied = BKE_image_has_gpu_texture_premultiplied_alpha(ima, ibuf);
  if (rect_float) {
    /* Float image is already in scene linear colorspace or non-color data by
     * convention, no colorspace conversion needed. But we do require 4 channels
     * currently. */
    if (ibuf->channels != 4 || scaled || !store_premultiplied) {
      rect_float = (float *)MEM_mallocN(sizeof(float[4]) * w * h, __func__);
      if (rect_float == nullptr) {
        return;
      }

      tex_stride = w;
      tex_offset = 0;

      IMB_colormanagement_imbuf_to_float_texture(
          rect_float, x, y, w, h, ibuf, store_premultiplied);
    }
  }
  else {
    /* Byte image is in original colorspace from the file, and may need conversion. */
    if (IMB_colormanagement_space_is_data(ibuf->byte_buffer.colorspace) && !scaled) {
      /* Not scaled Non-color data, just store buffer as is. */
    }
    else if (IMB_colormanagement_space_is_srgb(ibuf->byte_buffer.colorspace) ||
             IMB_colormanagement_space_is_scene_linear(ibuf->byte_buffer.colorspace) ||
             IMB_colormanagement_space_is_data(ibuf->byte_buffer.colorspace))
    {
      /* sRGB or scene linear or scaled down non-color data , store as byte texture that the GPU
       * can decode directly. */
      rect = (uchar *)MEM_mallocN(sizeof(uchar[4]) * w * h, __func__);
      if (rect == nullptr) {
        return;
      }

      tex_stride = w;
      tex_offset = 0;

      /* Convert to scene linear with sRGB compression, and premultiplied for
       * correct texture interpolation. */
      IMB_colormanagement_imbuf_to_byte_texture(rect, x, y, w, h, ibuf, store_premultiplied);
    }
    else {
      /* Other colorspace, store as float texture to avoid precision loss. */
      rect_float = (float *)MEM_mallocN(sizeof(float[4]) * w * h, __func__);
      if (rect_float == nullptr) {
        return;
      }

      tex_stride = w;
      tex_offset = 0;

      IMB_colormanagement_imbuf_to_float_texture(
          rect_float, x, y, w, h, ibuf, store_premultiplied);
    }
  }

  if (scaled) {
    /* Slower update where we first have to scale the input pixels. */
    if (tile != nullptr) {
      ImageTile_Runtime *tile_runtime = &tile->runtime;
      int *tileoffset = tile_runtime->tilearray_offset;
      int *tilesize = tile_runtime->tilearray_size;
      int tilelayer = tile_runtime->tilearray_layer;
      gpu_texture_update_scaled(
          tex, rect, rect_float, ibuf->x, ibuf->y, x, y, tilelayer, tileoffset, tilesize, w, h);
    }
    else {
      gpu_texture_update_scaled(
          tex, rect, rect_float, ibuf->x, ibuf->y, x, y, -1, nullptr, nullptr, w, h);
    }
  }
  else {
    /* Fast update at same resolution. */
    if (tile != nullptr) {
      ImageTile_Runtime *tile_runtime = &tile->runtime;
      int *tileoffset = tile_runtime->tilearray_offset;
      int tilelayer = tile_runtime->tilearray_layer;
      gpu_texture_update_unscaled(
          tex, rect, rect_float, x, y, tilelayer, tileoffset, w, h, tex_stride, tex_offset);
    }
    else {
      gpu_texture_update_unscaled(
          tex, rect, rect_float, x, y, -1, nullptr, w, h, tex_stride, tex_offset);
    }
  }

  /* Free buffers if needed. */
  if (rect && rect != ibuf->byte_buffer.data) {
    MEM_freeN(rect);
  }
  if (rect_float && rect_float != ibuf->float_buffer.data) {
    MEM_freeN(rect_float);
  }

  if (GPU_mipmap_enabled()) {
    GPU_texture_update_mipmap_chain(tex);
  }
  else {
    ima->gpuflag &= ~IMA_GPU_MIPMAP_COMPLETE;
  }

  GPU_texture_unbind(tex);
}

static void image_update_gputexture_ex(
    Image *ima, ImageTile *tile, ImBuf *ibuf, int x, int y, int w, int h)
{
  const int eye = 0;
  GPUTexture *tex = ima->gputexture[TEXTARGET_2D][eye];
  /* Check if we need to update the main gputexture. */
  if (tex != nullptr && tile == ima->tiles.first) {
    gpu_texture_update_from_ibuf(tex, ima, ibuf, nullptr, x, y, w, h);
  }

  /* Check if we need to update the array gputexture. */
  tex = ima->gputexture[TEXTARGET_2D_ARRAY][eye];
  if (tex != nullptr) {
    gpu_texture_update_from_ibuf(tex, ima, ibuf, tile, x, y, w, h);
  }
}

void BKE_image_update_gputexture(Image *ima, ImageUser *iuser, int x, int y, int w, int h)
{
  ImageTile *image_tile = BKE_image_get_tile_from_iuser(ima, iuser);
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, nullptr);
  BKE_image_update_gputexture_delayed(ima, image_tile, ibuf, x, y, w, h);
  BKE_image_release_ibuf(ima, ibuf, nullptr);
}

void BKE_image_update_gputexture_delayed(
    Image *ima, ImageTile *image_tile, ImBuf *ibuf, int x, int y, int w, int h)
{
  /* Check for full refresh. */
  if (ibuf != nullptr && ima->source != IMA_SRC_TILED && x == 0 && y == 0 && w == ibuf->x &&
      h == ibuf->y)
  {
    BKE_image_partial_update_mark_full_update(ima);
  }
  else {
    rcti dirty_region;
    BLI_rcti_init(&dirty_region, x, x + w, y, y + h);
    BKE_image_partial_update_mark_region(ima, image_tile, ibuf, &dirty_region);
  }
}

void BKE_image_paint_set_mipmap(Main *bmain, bool mipmap)
{
  LISTBASE_FOREACH (Image *, ima, &bmain->images) {
    if (BKE_image_has_opengl_texture(ima)) {
      if (ima->gpuflag & IMA_GPU_MIPMAP_COMPLETE) {
        for (int a = 0; a < TEXTARGET_COUNT; a++) {
          if (ELEM(a, TEXTARGET_2D, TEXTARGET_2D_ARRAY)) {
            for (int eye = 0; eye < 2; eye++) {
              GPUTexture *tex = ima->gputexture[a][eye];
              if (tex != nullptr) {
                GPU_texture_mipmap_mode(tex, mipmap, true);
              }
            }
          }
        }
      }
      else {
        BKE_image_free_gputextures(ima);
      }
    }
    else {
      ima->gpuflag &= ~IMA_GPU_MIPMAP_COMPLETE;
    }
  }
}

/** \} */
}
