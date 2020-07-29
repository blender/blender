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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Utility functions for dealing with OpenGL texture & material context,
 * mipmap generation and light objects.
 *
 * These are some obscure rendering functions shared between the game engine (not anymore)
 * and the blender, in this module to avoid duplication
 * and abstract them away from the rest a bit.
 */

#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_boxpack_2d.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_image_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_movieclip.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_texture.h"

#include "PIL_time.h"

static void gpu_free_image(Image *ima, const bool immediate);
static void gpu_free_unused_buffers(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utility functions
 * \{ */

/** Checking powers of two for images since OpenGL ES requires it */
#ifdef WITH_DDS
static bool is_power_of_2_resolution(int w, int h)
{
  return is_power_of_2_i(w) && is_power_of_2_i(h);
}
#endif

static bool is_over_resolution_limit(int w, int h)
{
  int size = GPU_max_texture_size();
  int reslimit = (U.glreslimit != 0) ? min_ii(U.glreslimit, size) : size;

  return (w > reslimit || h > reslimit);
}

static int smaller_power_of_2_limit(int num)
{
  int reslimit = (U.glreslimit != 0) ? min_ii(U.glreslimit, GPU_max_texture_size()) :
                                       GPU_max_texture_size();
  /* take texture clamping into account */
  if (num > reslimit) {
    return reslimit;
  }

  return power_of_2_min_i(num);
}

static GPUTexture **gpu_get_image_gputexture(Image *ima,
                                             eGPUTextureTarget textarget,
                                             const int multiview_eye)
{
  const bool in_range = (textarget >= 0) && (textarget < TEXTARGET_COUNT);
  BLI_assert(in_range);

  if (in_range) {
    return &(ima->gputexture[textarget][multiview_eye]);
  }
  return NULL;
}

static GPUTexture **gpu_get_movieclip_gputexture(MovieClip *clip,
                                                 MovieClipUser *cuser,
                                                 eGPUTextureTarget textarget)
{
  LISTBASE_FOREACH (MovieClip_RuntimeGPUTexture *, tex, &clip->runtime.gputextures) {
    if (memcmp(&tex->user, cuser, sizeof(MovieClipUser)) == 0) {
      if (tex == NULL) {
        tex = (MovieClip_RuntimeGPUTexture *)MEM_mallocN(sizeof(MovieClip_RuntimeGPUTexture),
                                                         __func__);

        for (int i = 0; i < TEXTARGET_COUNT; i++) {
          tex->gputexture[i] = NULL;
        }

        memcpy(&tex->user, cuser, sizeof(MovieClipUser));
        BLI_addtail(&clip->runtime.gputextures, tex);
      }

      return &tex->gputexture[textarget];
    }
  }
  return NULL;
}

/* Apply colormanagement and scale buffer if needed. */
static void *get_ibuf_data(const Image *ima,
                           const ImBuf *ibuf,
                           const bool do_rescale,
                           const int rescale_size[2],
                           const bool compress_as_srgb,
                           bool *r_freebuf)
{
  const bool is_float_rect = (ibuf->rect_float != NULL);
  void *data_rect = (is_float_rect) ? (void *)ibuf->rect_float : (void *)ibuf->rect;

  if (is_float_rect) {
    /* Float image is already in scene linear colorspace or non-color data by
     * convention, no colorspace conversion needed. But we do require 4 channels
     * currently. */
    const bool store_premultiplied = ima ? (ima->alpha_mode != IMA_ALPHA_STRAIGHT) : false;

    if (ibuf->channels != 4 || !store_premultiplied) {
      data_rect = MEM_mallocN(sizeof(float) * 4 * ibuf->x * ibuf->y, __func__);
      *r_freebuf = true;

      if (data_rect == NULL) {
        return NULL;
      }

      IMB_colormanagement_imbuf_to_float_texture(
          (float *)data_rect, 0, 0, ibuf->x, ibuf->y, ibuf, store_premultiplied);
    }
  }
  else {
    /* Byte image is in original colorspace from the file. If the file is sRGB
     * scene linear, or non-color data no conversion is needed. Otherwise we
     * compress as scene linear + sRGB transfer function to avoid precision loss
     * in common cases.
     *
     * We must also convert to premultiplied for correct texture interpolation
     * and consistency with float images. */
    if (!IMB_colormanagement_space_is_data(ibuf->rect_colorspace)) {
      data_rect = MEM_mallocN(sizeof(uchar) * 4 * ibuf->x * ibuf->y, __func__);
      *r_freebuf = true;

      if (data_rect == NULL) {
        return NULL;
      }

      /* Texture storage of images is defined by the alpha mode of the image. The
       * downside of this is that there can be artifacts near alpha edges. However,
       * this allows us to use sRGB texture formats and preserves color values in
       * zero alpha areas, and appears generally closer to what game engines that we
       * want to be compatible with do. */
      const bool store_premultiplied = ima ? (ima->alpha_mode == IMA_ALPHA_PREMUL) : true;
      IMB_colormanagement_imbuf_to_byte_texture(
          (uchar *)data_rect, 0, 0, ibuf->x, ibuf->y, ibuf, compress_as_srgb, store_premultiplied);
    }
  }

  if (do_rescale) {
    uint *rect = (is_float_rect) ? NULL : (uint *)data_rect;
    float *rect_float = (is_float_rect) ? (float *)data_rect : NULL;

    ImBuf *scale_ibuf = IMB_allocFromBuffer(rect, rect_float, ibuf->x, ibuf->y, 4);
    IMB_scaleImBuf(scale_ibuf, UNPACK2(rescale_size));

    data_rect = (is_float_rect) ? (void *)scale_ibuf->rect_float : (void *)scale_ibuf->rect;
    *r_freebuf = true;
    /* Steal the rescaled buffer to avoid double free. */
    scale_ibuf->rect_float = NULL;
    scale_ibuf->rect = NULL;
    IMB_freeImBuf(scale_ibuf);
  }
  return data_rect;
}

static void get_texture_format_from_ibuf(const Image *ima,
                                         const ImBuf *ibuf,
                                         eGPUDataFormat *r_data_format,
                                         eGPUTextureFormat *r_texture_format)
{
  const bool float_rect = (ibuf->rect_float != NULL);
  const bool high_bitdepth = (!(ibuf->flags & IB_halffloat) && (ima->flag & IMA_HIGH_BITDEPTH));
  const bool use_srgb = (!IMB_colormanagement_space_is_data(ibuf->rect_colorspace) &&
                         !IMB_colormanagement_space_is_scene_linear(ibuf->rect_colorspace));

  *r_data_format = (float_rect) ? GPU_DATA_FLOAT : GPU_DATA_UNSIGNED_BYTE;

  if (float_rect) {
    *r_texture_format = high_bitdepth ? GPU_RGBA32F : GPU_RGBA16F;
  }
  else {
    *r_texture_format = use_srgb ? GPU_SRGB8_A8 : GPU_RGBA8;
  }
}

/* Return false if no suitable format was found. */
static bool get_texture_compressed_format_from_ibuf(const ImBuf *ibuf,
                                                    eGPUTextureFormat *r_data_format)
{
#ifdef WITH_DDS
  /* For DDS we only support data, scene linear and sRGB. Converting to
   * different colorspace would break the compression. */
  const bool use_srgb = (!IMB_colormanagement_space_is_data(ibuf->rect_colorspace) &&
                         !IMB_colormanagement_space_is_scene_linear(ibuf->rect_colorspace));

  if (ibuf->dds_data.fourcc == FOURCC_DXT1) {
    *r_data_format = (use_srgb) ? GPU_SRGB8_A8_DXT1 : GPU_RGBA8_DXT1;
  }
  else if (ibuf->dds_data.fourcc == FOURCC_DXT3) {
    *r_data_format = (use_srgb) ? GPU_SRGB8_A8_DXT3 : GPU_RGBA8_DXT3;
  }
  else if (ibuf->dds_data.fourcc == FOURCC_DXT5) {
    *r_data_format = (use_srgb) ? GPU_SRGB8_A8_DXT5 : GPU_RGBA8_DXT5;
  }
  else {
    return false;
  }
  return true;
#else
  return false;
#endif
}

static bool mipmap_enabled(void)
{
  /* This used to be a userpref option. Maybe it will be re-introduce late. */
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UDIM gpu texture
 * \{ */

static GPUTexture *gpu_texture_create_tile_mapping(Image *ima, const int multiview_eye)
{
  GPUTexture *tilearray = ima->gputexture[TEXTARGET_2D_ARRAY][multiview_eye];

  if (tilearray == NULL) {
    return 0;
  }

  float array_w = GPU_texture_width(tilearray);
  float array_h = GPU_texture_height(tilearray);

  ImageTile *last_tile = (ImageTile *)ima->tiles.last;
  /* Tiles are sorted by number. */
  int max_tile = last_tile->tile_number - 1001;

  /* create image */
  int width = max_tile + 1;
  float *data = (float *)MEM_callocN(width * 8 * sizeof(float), __func__);
  for (int i = 0; i < width; i++) {
    data[4 * i] = -1.0f;
  }
  LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
    int i = tile->tile_number - 1001;
    data[4 * i] = tile->runtime.tilearray_layer;

    float *tile_info = &data[4 * width + 4 * i];
    tile_info[0] = tile->runtime.tilearray_offset[0] / array_w;
    tile_info[1] = tile->runtime.tilearray_offset[1] / array_h;
    tile_info[2] = tile->runtime.tilearray_size[0] / array_w;
    tile_info[3] = tile->runtime.tilearray_size[1] / array_h;
  }

  GPUTexture *tex = GPU_texture_create_1d_array(width, 2, GPU_RGBA32F, data, NULL);
  GPU_texture_mipmap_mode(tex, false, false);

  MEM_freeN(data);

  return tex;
}

typedef struct PackTile {
  FixedSizeBoxPack boxpack;
  ImageTile *tile;
  float pack_score;
} PackTile;

static int compare_packtile(const void *a, const void *b)
{
  const PackTile *tile_a = (const PackTile *)a;
  const PackTile *tile_b = (const PackTile *)b;

  return tile_a->pack_score < tile_b->pack_score;
}

static GPUTexture *gpu_texture_create_tile_array(Image *ima, ImBuf *main_ibuf)
{
  int arraywidth = 0, arrayheight = 0;
  ListBase boxes = {NULL};

  LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
    ImageUser iuser;
    BKE_imageuser_default(&iuser);
    iuser.tile = tile->tile_number;
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, NULL);

    if (ibuf) {
      PackTile *packtile = (PackTile *)MEM_callocN(sizeof(PackTile), __func__);
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

      BKE_image_release_ibuf(ima, ibuf, NULL);
      BLI_addtail(&boxes, packtile);
    }
  }

  BLI_assert(arraywidth > 0 && arrayheight > 0);

  BLI_listbase_sort(&boxes, compare_packtile);
  int arraylayers = 0;
  /* Keep adding layers until all tiles are packed. */
  while (boxes.first != NULL) {
    ListBase packed = {NULL};
    BLI_box_pack_2d_fixedarea(&boxes, arraywidth, arrayheight, &packed);
    BLI_assert(packed.first != NULL);

    LISTBASE_FOREACH (PackTile *, packtile, &packed) {
      ImageTile *tile = packtile->tile;
      int *tileoffset = tile->runtime.tilearray_offset;
      int *tilesize = tile->runtime.tilearray_size;

      tileoffset[0] = packtile->boxpack.x;
      tileoffset[1] = packtile->boxpack.y;
      tilesize[0] = packtile->boxpack.w;
      tilesize[1] = packtile->boxpack.h;
      tile->runtime.tilearray_layer = arraylayers;
    }

    BLI_freelistN(&packed);
    arraylayers++;
  }

  eGPUDataFormat data_format;
  eGPUTextureFormat tex_format;
  get_texture_format_from_ibuf(ima, main_ibuf, &data_format, &tex_format);

  /* Create Texture. */
  GPUTexture *tex = GPU_texture_create_nD(
      arraywidth, arrayheight, arraylayers, 2, NULL, tex_format, data_format, 0, false, NULL);

  GPU_texture_bind(tex, 0);

  /* Upload each tile one by one. */
  LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
    int tilelayer = tile->runtime.tilearray_layer;
    int *tileoffset = tile->runtime.tilearray_offset;
    int *tilesize = tile->runtime.tilearray_size;

    if (tilesize[0] == 0 || tilesize[1] == 0) {
      continue;
    }

    ImageUser iuser;
    BKE_imageuser_default(&iuser);
    iuser.tile = tile->tile_number;
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, NULL);

    if (ibuf) {
      const bool needs_scale = (ibuf->x != tilesize[0] || ibuf->y != tilesize[1]);
      const bool compress_as_srgb = (tex_format == GPU_SRGB8_A8);
      bool freebuf = false;

      void *pixeldata = get_ibuf_data(
          ima, ibuf, needs_scale, tilesize, compress_as_srgb, &freebuf);
      GPU_texture_update_sub(
          tex, data_format, pixeldata, UNPACK2(tileoffset), tilelayer, UNPACK2(tilesize), 1);

      if (freebuf) {
        MEM_SAFE_FREE(pixeldata);
      }
    }

    BKE_image_release_ibuf(ima, ibuf, NULL);
  }

  if (mipmap_enabled()) {
    GPU_texture_generate_mipmap(tex);
    if (ima) {
      ima->gpuflag |= IMA_GPU_MIPMAP_COMPLETE;
    }
  }

  GPU_texture_unbind(tex);

  return tex;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Regular gpu texture
 * \{ */

static GPUTexture *gpu_texture_create_from_ibuf(Image *ima, ImBuf *ibuf)
{
  GPUTexture *tex = NULL;
  bool do_rescale = is_over_resolution_limit(ibuf->x, ibuf->y);

#ifdef WITH_DDS
  if (ibuf->ftype == IMB_FTYPE_DDS) {
    eGPUTextureFormat compressed_format;
    if (!get_texture_compressed_format_from_ibuf(ibuf, &compressed_format)) {
      fprintf(stderr, "Unable to find a suitable DXT compression,");
    }
    else if (do_rescale) {
      fprintf(stderr, "Unable to load DXT image resolution,");
    }
    else if (!is_power_of_2_resolution(ibuf->x, ibuf->y)) {
      fprintf(stderr, "Unable to load non-power-of-two DXT image resolution,");
    }
    else {
      tex = GPU_texture_create_compressed(
          ibuf->x, ibuf->y, ibuf->dds_data.nummipmaps, compressed_format, ibuf->dds_data.data);

      if (tex != NULL) {
        return tex;
      }
      else {
        fprintf(stderr, "ST3C support not found,");
      }
    }
    /* Fallback to uncompressed texture. */
    fprintf(stderr, " falling back to uncompressed.\n");
  }
#else
  (void)get_texture_compressed_format_from_ibuf;
#endif

  eGPUDataFormat data_format;
  eGPUTextureFormat tex_format;
  get_texture_format_from_ibuf(ima, ibuf, &data_format, &tex_format);

  int size[2] = {ibuf->x, ibuf->y};
  if (do_rescale) {
    size[0] = smaller_power_of_2_limit(size[0]);
    size[1] = smaller_power_of_2_limit(size[1]);
  }

  const bool compress_as_srgb = (tex_format == GPU_SRGB8_A8);
  bool freebuf = false;

  void *data = get_ibuf_data(ima, ibuf, do_rescale, size, compress_as_srgb, &freebuf);

  /* Create Texture. */
  tex = GPU_texture_create_nD(UNPACK2(size), 0, 2, data, tex_format, data_format, 0, false, NULL);

  GPU_texture_anisotropic_filter(tex, true);

  if (mipmap_enabled()) {
    GPU_texture_bind(tex, 0);
    GPU_texture_generate_mipmap(tex);
    GPU_texture_unbind(tex);
    if (ima) {
      ima->gpuflag |= IMA_GPU_MIPMAP_COMPLETE;
    }
    GPU_texture_mipmap_mode(tex, true, true);
  }
  else {
    GPU_texture_mipmap_mode(tex, false, true);
  }

  if (freebuf) {
    MEM_SAFE_FREE(data);
  }

  return tex;
}

/* Get the GPUTexture for a given `Image`.
 *
 * `iuser` and `ibuf` are mutual exclusive parameters. The caller can pass the `ibuf` when already
 * available. It is also required when requesting the GPUTexture for a render result. */
GPUTexture *GPU_texture_from_blender(Image *ima,
                                     ImageUser *iuser,
                                     ImBuf *ibuf,
                                     eGPUTextureTarget textarget)
{
#ifndef GPU_STANDALONE
  if (ima == NULL) {
    return NULL;
  }

  /* Free any unused GPU textures, since we know we are in a thread with OpenGL
   * context and might as well ensure we have as much space free as possible. */
  gpu_free_unused_buffers();

  /* currently, gpu refresh tagging is used by ima sequences */
  if (ima->gpuflag & IMA_GPU_REFRESH) {
    gpu_free_image(ima, true);
    ima->gpuflag &= ~IMA_GPU_REFRESH;
  }

  /* Tag as in active use for garbage collector. */
  BKE_image_tag_time(ima);

  /* Test if we already have a texture. */
  GPUTexture **tex = gpu_get_image_gputexture(ima, textarget, iuser ? iuser->multiview_eye : 0);
  if (*tex) {
    return *tex;
  }

  /* Check if we have a valid image. If not, we return a dummy
   * texture with zero bindcode so we don't keep trying. */
  ImageTile *tile = BKE_image_get_tile(ima, 0);
  if (tile == NULL || tile->ok == 0) {
    *tex = GPU_texture_create_error(textarget);
    return *tex;
  }

  /* check if we have a valid image buffer */
  ImBuf *ibuf_intern = ibuf;
  if (ibuf_intern == NULL) {
    ibuf_intern = BKE_image_acquire_ibuf(ima, iuser, NULL);
    if (ibuf_intern == NULL) {
      *tex = GPU_texture_create_error(textarget);
      return *tex;
    }
  }

  if (textarget == TEXTARGET_2D_ARRAY) {
    *tex = gpu_texture_create_tile_array(ima, ibuf_intern);
  }
  else if (textarget == TEXTARGET_TILE_MAPPING) {
    *tex = gpu_texture_create_tile_mapping(ima, iuser ? iuser->multiview_eye : 0);
  }
  else {
    *tex = gpu_texture_create_from_ibuf(ima, ibuf_intern);
  }

  /* if `ibuf` was given, we don't own the `ibuf_intern` */
  if (ibuf == NULL) {
    BKE_image_release_ibuf(ima, ibuf_intern, NULL);
  }

  GPU_texture_orig_size_set(*tex, ibuf_intern->x, ibuf_intern->y);

  return *tex;
#endif
  return NULL;
}

GPUTexture *GPU_texture_from_movieclip(MovieClip *clip,
                                       MovieClipUser *cuser,
                                       eGPUTextureTarget textarget)
{
#ifndef GPU_STANDALONE
  if (clip == NULL) {
    return NULL;
  }

  GPUTexture **tex = gpu_get_movieclip_gputexture(clip, cuser, textarget);
  if (*tex) {
    return *tex;
  }

  /* check if we have a valid image buffer */
  ImBuf *ibuf = BKE_movieclip_get_ibuf(clip, cuser);
  if (ibuf == NULL) {
    *tex = GPU_texture_create_error(textarget);
    return *tex;
  }

  *tex = gpu_texture_create_from_ibuf(NULL, ibuf);

  IMB_freeImBuf(ibuf);

  return *tex;
#else
  return NULL;
#endif
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
  float xratio = limit_w / (float)full_w;
  float yratio = limit_h / (float)full_h;

  int part_w = *w, part_h = *h;

  /* Find sub coordinates in scaled image. Take ceiling because we will be
   * losing 1 pixel due to rounding errors in x,y. */
  *x *= xratio;
  *y *= yratio;
  *w = (int)ceil(xratio * (*w));
  *h = (int)ceil(yratio * (*h));

  /* ...but take back if we are over the limit! */
  if (*x + *w > limit_w) {
    (*w)--;
  }
  if (*y + *h > limit_h) {
    (*h)--;
  }

  /* Scale pixels. */
  ImBuf *ibuf = IMB_allocFromBuffer((uint *)rect, rect_float, part_w, part_h, 4);
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
    int limit_w = smaller_power_of_2_limit(full_w);
    int limit_h = smaller_power_of_2_limit(full_h);

    ibuf = update_do_scale(rect, rect_float, &x, &y, &w, &h, limit_w, limit_h, full_w, full_h);
  }

  void *data = (ibuf->rect_float) ? (void *)(ibuf->rect_float) : (void *)(ibuf->rect);
  eGPUDataFormat data_format = (ibuf->rect_float) ? GPU_DATA_FLOAT : GPU_DATA_UNSIGNED_BYTE;

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
  eGPUDataFormat data_format = (rect_float) ? GPU_DATA_FLOAT : GPU_DATA_UNSIGNED_BYTE;

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
  /* Partial update of texture for texture painting. This is often much
   * quicker than fully updating the texture for high resolution images. */
  GPU_texture_bind(tex, 0);

  bool scaled;
  if (tile != NULL) {
    int *tilesize = tile->runtime.tilearray_size;
    scaled = (ibuf->x != tilesize[0]) || (ibuf->y != tilesize[1]);
  }
  else {
    scaled = is_over_resolution_limit(ibuf->x, ibuf->y);
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
  float *rect_float = ibuf->rect_float;
  uchar *rect = (uchar *)ibuf->rect;
  int tex_stride = ibuf->x;
  int tex_offset = ibuf->channels * (y * ibuf->x + x);

  if (rect_float == NULL) {
    /* Byte pixels. */
    if (!IMB_colormanagement_space_is_data(ibuf->rect_colorspace)) {
      const bool compress_as_srgb = !IMB_colormanagement_space_is_scene_linear(
          ibuf->rect_colorspace);

      rect = (uchar *)MEM_mallocN(sizeof(uchar) * 4 * w * h, __func__);
      if (rect == NULL) {
        return;
      }

      tex_stride = w;
      tex_offset = 0;

      /* Convert to scene linear with sRGB compression, and premultiplied for
       * correct texture interpolation. */
      const bool store_premultiplied = (ima->alpha_mode == IMA_ALPHA_PREMUL);
      IMB_colormanagement_imbuf_to_byte_texture(
          rect, x, y, w, h, ibuf, compress_as_srgb, store_premultiplied);
    }
  }
  else {
    /* Float pixels. */
    const bool store_premultiplied = (ima->alpha_mode != IMA_ALPHA_STRAIGHT);

    if (ibuf->channels != 4 || scaled || !store_premultiplied) {
      rect_float = (float *)MEM_mallocN(sizeof(float) * 4 * w * h, __func__);
      if (rect_float == NULL) {
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
    if (tile != NULL) {
      int *tileoffset = tile->runtime.tilearray_offset;
      int *tilesize = tile->runtime.tilearray_size;
      int tilelayer = tile->runtime.tilearray_layer;
      gpu_texture_update_scaled(
          tex, rect, rect_float, ibuf->x, ibuf->y, x, y, tilelayer, tileoffset, tilesize, w, h);
    }
    else {
      gpu_texture_update_scaled(
          tex, rect, rect_float, ibuf->x, ibuf->y, x, y, -1, NULL, NULL, w, h);
    }
  }
  else {
    /* Fast update at same resolution. */
    if (tile != NULL) {
      int *tileoffset = tile->runtime.tilearray_offset;
      int tilelayer = tile->runtime.tilearray_layer;
      gpu_texture_update_unscaled(
          tex, rect, rect_float, x, y, tilelayer, tileoffset, w, h, tex_stride, tex_offset);
    }
    else {
      gpu_texture_update_unscaled(
          tex, rect, rect_float, x, y, -1, NULL, w, h, tex_stride, tex_offset);
    }
  }

  /* Free buffers if needed. */
  if (rect && rect != (uchar *)ibuf->rect) {
    MEM_freeN(rect);
  }
  if (rect_float && rect_float != ibuf->rect_float) {
    MEM_freeN(rect_float);
  }

  if (mipmap_enabled()) {
    GPU_texture_generate_mipmap(tex);
  }
  else {
    ima->gpuflag &= ~IMA_GPU_MIPMAP_COMPLETE;
  }

  GPU_texture_unbind(tex);
}

void GPU_paint_update_image(Image *ima, ImageUser *iuser, int x, int y, int w, int h)
{
#ifndef GPU_STANDALONE
  ImBuf *ibuf = BKE_image_acquire_ibuf(ima, iuser, NULL);
  ImageTile *tile = BKE_image_get_tile_from_iuser(ima, iuser);

  if ((ibuf == NULL) || (w == 0) || (h == 0)) {
    /* Full reload of texture. */
    GPU_free_image(ima);
  }

  GPUTexture *tex = ima->gputexture[TEXTARGET_2D][0];
  /* Check if we need to update the main gputexture. */
  if (tex != NULL && tile == ima->tiles.first) {
    gpu_texture_update_from_ibuf(tex, ima, ibuf, NULL, x, y, w, h);
  }

  /* Check if we need to update the array gputexture. */
  tex = ima->gputexture[TEXTARGET_2D_ARRAY][0];
  if (tex != NULL) {
    gpu_texture_update_from_ibuf(tex, ima, ibuf, tile, x, y, w, h);
  }

  BKE_image_release_ibuf(ima, ibuf, NULL);
#endif
}

/* these two functions are called on entering and exiting texture paint mode,
 * temporary disabling/enabling mipmapping on all images for quick texture
 * updates with glTexSubImage2D. images that didn't change don't have to be
 * re-uploaded to OpenGL */
void GPU_paint_set_mipmap(Main *bmain, bool mipmap)
{
#ifndef GPU_STANDALONE
  LISTBASE_FOREACH (Image *, ima, &bmain->images) {
    if (BKE_image_has_opengl_texture(ima)) {
      if (ima->gpuflag & IMA_GPU_MIPMAP_COMPLETE) {
        for (int eye = 0; eye < 2; eye++) {
          for (int a = 0; a < TEXTARGET_COUNT; a++) {
            if (ELEM(a, TEXTARGET_2D, TEXTARGET_2D_ARRAY)) {
              GPUTexture *tex = ima->gputexture[a][eye];
              if (tex != NULL) {
                GPU_texture_mipmap_mode(tex, mipmap, true);
              }
            }
          }
        }
      }
      else {
        GPU_free_image(ima);
      }
    }
    else {
      ima->gpuflag &= ~IMA_GPU_MIPMAP_COMPLETE;
    }
  }
#endif /* GPU_STANDALONE */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delayed GPU texture free
 *
 * Image datablocks can be deleted by any thread, but there may not be any active OpenGL context.
 * In that case we push them into a queue and free the buffers later.
 * \{ */

static LinkNode *gpu_texture_free_queue = NULL;
static ThreadMutex gpu_texture_queue_mutex = BLI_MUTEX_INITIALIZER;

static void gpu_free_unused_buffers()
{
  if (gpu_texture_free_queue == NULL) {
    return;
  }

  BLI_mutex_lock(&gpu_texture_queue_mutex);

  if (gpu_texture_free_queue != NULL) {
    GPUTexture *tex;
    while ((tex = (GPUTexture *)BLI_linklist_pop(&gpu_texture_free_queue))) {
      GPU_texture_free(tex);
    }
    gpu_texture_free_queue = NULL;
  }

  BLI_mutex_unlock(&gpu_texture_queue_mutex);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deletion
 * \{ */

static void gpu_free_image(Image *ima, const bool immediate)
{
  for (int eye = 0; eye < 2; eye++) {
    for (int i = 0; i < TEXTARGET_COUNT; i++) {
      if (ima->gputexture[i][eye] != NULL) {
        if (immediate) {
          GPU_texture_free(ima->gputexture[i][eye]);
        }
        else {
          BLI_mutex_lock(&gpu_texture_queue_mutex);
          BLI_linklist_prepend(&gpu_texture_free_queue, ima->gputexture[i][eye]);
          BLI_mutex_unlock(&gpu_texture_queue_mutex);
        }

        ima->gputexture[i][eye] = NULL;
      }
    }
  }

  ima->gpuflag &= ~IMA_GPU_MIPMAP_COMPLETE;
}

void GPU_free_unused_buffers()
{
  if (BLI_thread_is_main()) {
    gpu_free_unused_buffers();
  }
}

void GPU_free_image(Image *ima)
{
  gpu_free_image(ima, BLI_thread_is_main());
}

void GPU_free_movieclip(struct MovieClip *clip)
{
  /* number of gpu textures to keep around as cache
   * We don't want to keep too many GPU textures for
   * movie clips around, as they can be large.*/
  const int MOVIECLIP_NUM_GPUTEXTURES = 1;

  while (BLI_listbase_count(&clip->runtime.gputextures) > MOVIECLIP_NUM_GPUTEXTURES) {
    MovieClip_RuntimeGPUTexture *tex = (MovieClip_RuntimeGPUTexture *)BLI_pophead(
        &clip->runtime.gputextures);
    for (int i = 0; i < TEXTARGET_COUNT; i++) {
      /* free glsl image binding */
      if (tex->gputexture[i]) {
        GPU_texture_free(tex->gputexture[i]);
        tex->gputexture[i] = NULL;
      }
    }
    MEM_freeN(tex);
  }
}

void GPU_free_images(Main *bmain)
{
  if (bmain) {
    LISTBASE_FOREACH (Image *, ima, &bmain->images) {
      GPU_free_image(ima);
    }
  }
}

/* same as above but only free animated images */
void GPU_free_images_anim(Main *bmain)
{
  if (bmain) {
    LISTBASE_FOREACH (Image *, ima, &bmain->images) {
      if (BKE_image_is_animated(ima)) {
        GPU_free_image(ima);
      }
    }
  }
}

void GPU_free_images_old(Main *bmain)
{
  static int lasttime = 0;
  int ctime = (int)PIL_check_seconds_timer();

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
        GPU_free_image(ima);
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
