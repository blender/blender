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
 * util.c
 */

/** \file
 * \ingroup imbuf
 */

#include "imbuf.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

#include "BKE_global.h"

#include "GPU_capabilities.h"
#include "GPU_texture.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

/* gpu ibuf utils */

static void imb_gpu_get_format(const ImBuf *ibuf,
                               bool high_bitdepth,
                               eGPUDataFormat *r_data_format,
                               eGPUTextureFormat *r_texture_format)
{
  const bool float_rect = (ibuf->rect_float != NULL);
  const bool use_srgb = (!IMB_colormanagement_space_is_data(ibuf->rect_colorspace) &&
                         !IMB_colormanagement_space_is_scene_linear(ibuf->rect_colorspace));
  high_bitdepth = (!(ibuf->flags & IB_halffloat) && high_bitdepth);

  *r_data_format = (float_rect) ? GPU_DATA_FLOAT : GPU_DATA_UNSIGNED_BYTE;

  if (float_rect) {
    *r_texture_format = high_bitdepth ? GPU_RGBA32F : GPU_RGBA16F;
  }
  else {
    *r_texture_format = use_srgb ? GPU_SRGB8_A8 : GPU_RGBA8;
  }
}

/* Return false if no suitable format was found. */
#ifdef WITH_DDS
static bool IMB_gpu_get_compressed_format(const ImBuf *ibuf, eGPUTextureFormat *r_texture_format)
{
  /* For DDS we only support data, scene linear and sRGB. Converting to
   * different colorspace would break the compression. */
  const bool use_srgb = (!IMB_colormanagement_space_is_data(ibuf->rect_colorspace) &&
                         !IMB_colormanagement_space_is_scene_linear(ibuf->rect_colorspace));

  if (ibuf->dds_data.fourcc == FOURCC_DXT1) {
    *r_texture_format = (use_srgb) ? GPU_SRGB8_A8_DXT1 : GPU_RGBA8_DXT1;
  }
  else if (ibuf->dds_data.fourcc == FOURCC_DXT3) {
    *r_texture_format = (use_srgb) ? GPU_SRGB8_A8_DXT3 : GPU_RGBA8_DXT3;
  }
  else if (ibuf->dds_data.fourcc == FOURCC_DXT5) {
    *r_texture_format = (use_srgb) ? GPU_SRGB8_A8_DXT5 : GPU_RGBA8_DXT5;
  }
  else {
    return false;
  }
  return true;
}
#endif

/**
 * Apply colormanagement and scale buffer if needed.
 * `*r_freedata` is set to true if the returned buffer need to be manually freed.
 */
static void *imb_gpu_get_data(const ImBuf *ibuf,
                              const bool do_rescale,
                              const int rescale_size[2],
                              const bool compress_as_srgb,
                              const bool store_premultiplied,
                              bool *r_freedata)
{
  const bool is_float_rect = (ibuf->rect_float != NULL);
  void *data_rect = (is_float_rect) ? (void *)ibuf->rect_float : (void *)ibuf->rect;
  bool freedata = false;

  if (is_float_rect) {
    /* Float image is already in scene linear colorspace or non-color data by
     * convention, no colorspace conversion needed. But we do require 4 channels
     * currently. */
    if (ibuf->channels != 4 || !store_premultiplied) {
      data_rect = MEM_mallocN(sizeof(float[4]) * ibuf->x * ibuf->y, __func__);
      *r_freedata = freedata = true;

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
      data_rect = MEM_mallocN(sizeof(uchar[4]) * ibuf->x * ibuf->y, __func__);
      *r_freedata = freedata = true;

      if (data_rect == NULL) {
        return NULL;
      }

      /* Texture storage of images is defined by the alpha mode of the image. The
       * downside of this is that there can be artifacts near alpha edges. However,
       * this allows us to use sRGB texture formats and preserves color values in
       * zero alpha areas, and appears generally closer to what game engines that we
       * want to be compatible with do. */
      IMB_colormanagement_imbuf_to_byte_texture(
          (uchar *)data_rect, 0, 0, ibuf->x, ibuf->y, ibuf, compress_as_srgb, store_premultiplied);
    }
  }

  if (do_rescale) {
    uint *rect = (is_float_rect) ? NULL : (uint *)data_rect;
    float *rect_float = (is_float_rect) ? (float *)data_rect : NULL;

    ImBuf *scale_ibuf = IMB_allocFromBuffer(rect, rect_float, ibuf->x, ibuf->y, 4);
    IMB_scaleImBuf(scale_ibuf, UNPACK2(rescale_size));

    if (freedata) {
      MEM_freeN(data_rect);
    }

    data_rect = (is_float_rect) ? (void *)scale_ibuf->rect_float : (void *)scale_ibuf->rect;
    *r_freedata = true;
    /* Steal the rescaled buffer to avoid double free. */
    scale_ibuf->rect_float = NULL;
    scale_ibuf->rect = NULL;
    IMB_freeImBuf(scale_ibuf);
  }
  return data_rect;
}

/* The ibuf is only here to detect the storage type. The produced texture will have undefined
 * content. It will need to be populated by using IMB_update_gpu_texture_sub(). */
GPUTexture *IMB_touch_gpu_texture(
    const char *name, ImBuf *ibuf, int w, int h, int layers, bool use_high_bitdepth)
{
  eGPUDataFormat data_format;
  eGPUTextureFormat tex_format;
  imb_gpu_get_format(ibuf, use_high_bitdepth, &data_format, &tex_format);

  GPUTexture *tex;
  if (layers > 0) {
    tex = GPU_texture_create_2d_array(name, w, h, layers, 1, tex_format, NULL);
  }
  else {
    tex = GPU_texture_create_2d(name, w, h, 9999, tex_format, NULL);
  }

  GPU_texture_anisotropic_filter(tex, true);
  return tex;
}

/* Will update a GPUTexture using the content of the ImBuf. Only one layer will be updated.
 * Will resize the ibuf if needed.
 * z is the layer to update. Unused if the texture is 2D. */
void IMB_update_gpu_texture_sub(GPUTexture *tex,
                                ImBuf *ibuf,
                                int x,
                                int y,
                                int z,
                                int w,
                                int h,
                                bool use_high_bitdepth,
                                bool use_premult)
{
  const bool do_rescale = (ibuf->x != w || ibuf->y != h);
  const int size[2] = {w, h};

  eGPUDataFormat data_format;
  eGPUTextureFormat tex_format;
  imb_gpu_get_format(ibuf, use_high_bitdepth, &data_format, &tex_format);

  const bool compress_as_srgb = (tex_format == GPU_SRGB8_A8);
  bool freebuf = false;

  void *data = imb_gpu_get_data(ibuf, do_rescale, size, compress_as_srgb, use_premult, &freebuf);

  /* Update Texture. */
  GPU_texture_update_sub(tex, data_format, data, x, y, z, w, h, 1);

  if (freebuf) {
    MEM_freeN(data);
  }
}

GPUTexture *IMB_create_gpu_texture(const char *name,
                                   ImBuf *ibuf,
                                   bool use_high_bitdepth,
                                   bool use_premult)
{
  GPUTexture *tex = NULL;
  int size[2] = {GPU_texture_size_with_limit(ibuf->x), GPU_texture_size_with_limit(ibuf->y)};
  bool do_rescale = (ibuf->x != size[0]) || (ibuf->y != size[1]);

#ifdef WITH_DDS
  if (ibuf->ftype == IMB_FTYPE_DDS) {
    eGPUTextureFormat compressed_format;
    if (!IMB_gpu_get_compressed_format(ibuf, &compressed_format)) {
      fprintf(stderr, "Unable to find a suitable DXT compression,");
    }
    else if (do_rescale) {
      fprintf(stderr, "Unable to load DXT image resolution,");
    }
    else if (!is_power_of_2_i(ibuf->x) || !is_power_of_2_i(ibuf->y)) {
      fprintf(stderr, "Unable to load non-power-of-two DXT image resolution,");
    }
    else {
      tex = GPU_texture_create_compressed_2d(name,
                                             ibuf->x,
                                             ibuf->y,
                                             ibuf->dds_data.nummipmaps,
                                             compressed_format,
                                             ibuf->dds_data.data);

      if (tex != NULL) {
        return tex;
      }

      fprintf(stderr, "ST3C support not found,");
    }
    /* Fallback to uncompressed texture. */
    fprintf(stderr, " falling back to uncompressed.\n");
  }
#endif

  eGPUDataFormat data_format;
  eGPUTextureFormat tex_format;
  imb_gpu_get_format(ibuf, use_high_bitdepth, &data_format, &tex_format);

  const bool compress_as_srgb = (tex_format == GPU_SRGB8_A8);
  bool freebuf = false;

  /* Create Texture. */
  tex = GPU_texture_create_2d(name, UNPACK2(size), 9999, tex_format, NULL);
  if (tex == NULL) {
    size[0] = max_ii(1, size[0] / 2);
    size[1] = max_ii(1, size[1] / 2);
    tex = GPU_texture_create_2d(name, UNPACK2(size), 9999, tex_format, NULL);
    do_rescale = true;
  }
  BLI_assert(tex != NULL);
  void *data = imb_gpu_get_data(ibuf, do_rescale, size, compress_as_srgb, use_premult, &freebuf);
  GPU_texture_update(tex, data_format, data);

  GPU_texture_anisotropic_filter(tex, true);

  if (freebuf) {
    MEM_freeN(data);
  }

  return tex;
}
