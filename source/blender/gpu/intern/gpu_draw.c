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
#include "GPU_glew.h"
#include "GPU_platform.h"
#include "GPU_texture.h"

#include "PIL_time.h"

static void gpu_free_image_immediate(Image *ima);

//* Checking powers of two for images since OpenGL ES requires it */
#ifdef WITH_DDS
static bool is_power_of_2_resolution(int w, int h)
{
  return is_power_of_2_i(w) && is_power_of_2_i(h);
}
#endif

static bool is_over_resolution_limit(GLenum textarget, int w, int h)
{
  int size = (textarget == GL_TEXTURE_CUBE_MAP) ? GPU_max_cube_map_size() : GPU_max_texture_size();
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

/* Current OpenGL state caching for GPU_set_tpage */

static struct GPUTextureState {
  /* also controls min/mag filtering */
  bool domipmap;
  /* only use when 'domipmap' is set */
  bool linearmipmap;
  /* store this so that new images created while texture painting won't be set to mipmapped */
  bool texpaint;

  float anisotropic;
} GTS = {1, 0, 0, 1.0f};

/* Mipmap settings */

void GPU_set_mipmap(Main *bmain, bool mipmap)
{
  if (GTS.domipmap != mipmap) {
    GPU_free_images(bmain);
    GTS.domipmap = mipmap;
  }
}

void GPU_set_linear_mipmap(bool linear)
{
  if (GTS.linearmipmap != linear) {
    GTS.linearmipmap = linear;
  }
}

bool GPU_get_mipmap(void)
{
  return GTS.domipmap && !GTS.texpaint;
}

bool GPU_get_linear_mipmap(void)
{
  return GTS.linearmipmap;
}

static GLenum gpu_get_mipmap_filter(bool mag)
{
  /* linearmipmap is off by default *when mipmapping is off,
   * use unfiltered display */
  if (mag) {
    if (GTS.domipmap) {
      return GL_LINEAR;
    }
    else {
      return GL_NEAREST;
    }
  }
  else {
    if (GTS.domipmap) {
      if (GTS.linearmipmap) {
        return GL_LINEAR_MIPMAP_LINEAR;
      }
      else {
        return GL_LINEAR_MIPMAP_NEAREST;
      }
    }
    else {
      return GL_NEAREST;
    }
  }
}

/* Anisotropic filtering settings */
void GPU_set_anisotropic(float value)
{
  if (GTS.anisotropic != value) {
    GPU_samplers_free();

    /* Clamp value to the maximum value the graphics card supports */
    const float max = GPU_max_texture_anisotropy();
    if (value > max) {
      value = max;
    }

    GTS.anisotropic = value;

    GPU_samplers_init();
  }
}

float GPU_get_anisotropic(void)
{
  return GTS.anisotropic;
}

/* Set OpenGL state for an MTFace */

static GPUTexture **gpu_get_image_gputexture(Image *ima, GLenum textarget, const int multiview_eye)
{
  if (textarget == GL_TEXTURE_2D) {
    return &(ima->gputexture[TEXTARGET_TEXTURE_2D][multiview_eye]);
  }
  else if (textarget == GL_TEXTURE_CUBE_MAP) {
    return &(ima->gputexture[TEXTARGET_TEXTURE_CUBE_MAP][multiview_eye]);
  }
  else if (textarget == GL_TEXTURE_2D_ARRAY) {
    return &(ima->gputexture[TEXTARGET_TEXTURE_2D_ARRAY][multiview_eye]);
  }
  else if (textarget == GL_TEXTURE_1D_ARRAY) {
    return &(ima->gputexture[TEXTARGET_TEXTURE_TILE_MAPPING][multiview_eye]);
  }

  return NULL;
}

static uint gpu_texture_create_tile_mapping(Image *ima, const int multiview_eye)
{
  GPUTexture *tilearray = ima->gputexture[TEXTARGET_TEXTURE_2D_ARRAY][multiview_eye];

  if (tilearray == NULL) {
    return 0;
  }

  float array_w = GPU_texture_width(tilearray);
  float array_h = GPU_texture_height(tilearray);

  ImageTile *last_tile = ima->tiles.last;
  /* Tiles are sorted by number. */
  int max_tile = last_tile->tile_number - 1001;

  /* create image */
  int bindcode;
  glGenTextures(1, (GLuint *)&bindcode);
  glBindTexture(GL_TEXTURE_1D_ARRAY, bindcode);

  int width = max_tile + 1;
  float *data = MEM_callocN(width * 8 * sizeof(float), __func__);
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

  glTexImage2D(GL_TEXTURE_1D_ARRAY, 0, GL_RGBA32F, width, 2, 0, GL_RGBA, GL_FLOAT, data);
  MEM_freeN(data);

  glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glBindTexture(GL_TEXTURE_1D_ARRAY, 0);

  return bindcode;
}

typedef struct PackTile {
  FixedSizeBoxPack boxpack;
  ImageTile *tile;
  float pack_score;
} PackTile;

static int compare_packtile(const void *a, const void *b)
{
  const PackTile *tile_a = a;
  const PackTile *tile_b = b;

  return tile_a->pack_score < tile_b->pack_score;
}

static uint gpu_texture_create_tile_array(Image *ima, ImBuf *main_ibuf)
{
  int arraywidth = 0, arrayheight = 0;

  ListBase boxes = {NULL};

  LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
    ImageUser iuser;
    BKE_imageuser_default(&iuser);
    iuser.tile = tile->tile_number;
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, NULL);

    if (ibuf) {
      PackTile *packtile = MEM_callocN(sizeof(PackTile), __func__);
      packtile->tile = tile;
      packtile->boxpack.w = ibuf->x;
      packtile->boxpack.h = ibuf->y;

      if (is_over_resolution_limit(
              GL_TEXTURE_2D_ARRAY, packtile->boxpack.w, packtile->boxpack.h)) {
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

  /* create image */
  int bindcode;
  glGenTextures(1, (GLuint *)&bindcode);
  glBindTexture(GL_TEXTURE_2D_ARRAY, bindcode);

  GLenum data_type, internal_format;
  if (main_ibuf->rect_float) {
    data_type = GL_FLOAT;
    internal_format = (!(main_ibuf->flags & IB_halffloat) && (ima->flag & IMA_HIGH_BITDEPTH)) ?
                          GL_RGBA32F :
                          GL_RGBA16F;
  }
  else {
    data_type = GL_UNSIGNED_BYTE;
    internal_format = GL_RGBA8;
    if (!IMB_colormanagement_space_is_data(main_ibuf->rect_colorspace) &&
        !IMB_colormanagement_space_is_scene_linear(main_ibuf->rect_colorspace)) {
      internal_format = GL_SRGB8_ALPHA8;
    }
  }

  glTexImage3D(GL_TEXTURE_2D_ARRAY,
               0,
               internal_format,
               arraywidth,
               arrayheight,
               arraylayers,
               0,
               GL_RGBA,
               data_type,
               NULL);

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
      bool needs_scale = (ibuf->x != tilesize[0] || ibuf->y != tilesize[1]);

      ImBuf *scale_ibuf = NULL;
      if (ibuf->rect_float) {
        float *rect_float = ibuf->rect_float;

        const bool store_premultiplied = ima->alpha_mode != IMA_ALPHA_STRAIGHT;
        if (ibuf->channels != 4 || !store_premultiplied) {
          rect_float = MEM_mallocN(sizeof(float) * 4 * ibuf->x * ibuf->y, __func__);
          IMB_colormanagement_imbuf_to_float_texture(
              rect_float, 0, 0, ibuf->x, ibuf->y, ibuf, store_premultiplied);
        }

        float *pixeldata = rect_float;
        if (needs_scale) {
          scale_ibuf = IMB_allocFromBuffer(NULL, rect_float, ibuf->x, ibuf->y, 4);
          IMB_scaleImBuf(scale_ibuf, tilesize[0], tilesize[1]);
          pixeldata = scale_ibuf->rect_float;
        }

        glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                        0,
                        tileoffset[0],
                        tileoffset[1],
                        tilelayer,
                        tilesize[0],
                        tilesize[1],
                        1,
                        GL_RGBA,
                        GL_FLOAT,
                        pixeldata);

        if (rect_float != ibuf->rect_float) {
          MEM_freeN(rect_float);
        }
      }
      else {
        unsigned int *rect = ibuf->rect;

        if (!IMB_colormanagement_space_is_data(ibuf->rect_colorspace)) {
          rect = MEM_mallocN(sizeof(uchar) * 4 * ibuf->x * ibuf->y, __func__);
          IMB_colormanagement_imbuf_to_byte_texture((uchar *)rect,
                                                    0,
                                                    0,
                                                    ibuf->x,
                                                    ibuf->y,
                                                    ibuf,
                                                    internal_format == GL_SRGB8_ALPHA8,
                                                    ima->alpha_mode == IMA_ALPHA_PREMUL);
        }

        unsigned int *pixeldata = rect;
        if (needs_scale) {
          scale_ibuf = IMB_allocFromBuffer(rect, NULL, ibuf->x, ibuf->y, 4);
          IMB_scaleImBuf(scale_ibuf, tilesize[0], tilesize[1]);
          pixeldata = scale_ibuf->rect;
        }
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                        0,
                        tileoffset[0],
                        tileoffset[1],
                        tilelayer,
                        tilesize[0],
                        tilesize[1],
                        1,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        pixeldata);

        if (rect != ibuf->rect) {
          MEM_freeN(rect);
        }
      }
      if (scale_ibuf != NULL) {
        IMB_freeImBuf(scale_ibuf);
      }
    }

    BKE_image_release_ibuf(ima, ibuf, NULL);
  }

  if (GPU_get_mipmap()) {
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    if (ima) {
      ima->gpuflag |= IMA_GPU_MIPMAP_COMPLETE;
    }
  }

  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  return bindcode;
}

static uint gpu_texture_create_from_ibuf(Image *ima, ImBuf *ibuf, int textarget)
{
  uint bindcode = 0;
  const bool mipmap = GPU_get_mipmap();
  const bool half_float = (ibuf->flags & IB_halffloat) != 0;

#ifdef WITH_DDS
  if (ibuf->ftype == IMB_FTYPE_DDS) {
    /* DDS is loaded directly in compressed form. */
    GPU_create_gl_tex_compressed(&bindcode, textarget, ima, ibuf);
    return bindcode;
  }
#endif

  /* Regular uncompressed texture. */
  float *rect_float = ibuf->rect_float;
  uchar *rect = (uchar *)ibuf->rect;
  bool compress_as_srgb = false;

  if (rect_float == NULL) {
    /* Byte image is in original colorspace from the file. If the file is sRGB
     * scene linear, or non-color data no conversion is needed. Otherwise we
     * compress as scene linear + sRGB transfer function to avoid precision loss
     * in common cases.
     *
     * We must also convert to premultiplied for correct texture interpolation
     * and consistency with float images. */
    if (!IMB_colormanagement_space_is_data(ibuf->rect_colorspace)) {
      compress_as_srgb = !IMB_colormanagement_space_is_scene_linear(ibuf->rect_colorspace);

      rect = MEM_mallocN(sizeof(uchar) * 4 * ibuf->x * ibuf->y, __func__);
      if (rect == NULL) {
        return bindcode;
      }

      /* Texture storage of images is defined by the alpha mode of the image. The
       * downside of this is that there can be artifacts near alpha edges. However,
       * this allows us to use sRGB texture formats and preserves color values in
       * zero alpha areas, and appears generally closer to what game engines that we
       * want to be compatible with do. */
      const bool store_premultiplied = ima ? (ima->alpha_mode == IMA_ALPHA_PREMUL) : true;
      IMB_colormanagement_imbuf_to_byte_texture(
          rect, 0, 0, ibuf->x, ibuf->y, ibuf, compress_as_srgb, store_premultiplied);
    }
  }
  else {
    /* Float image is already in scene linear colorspace or non-color data by
     * convention, no colorspace conversion needed. But we do require 4 channels
     * currently. */
    const bool store_premultiplied = ima ? (ima->alpha_mode != IMA_ALPHA_STRAIGHT) : false;

    if (ibuf->channels != 4 || !store_premultiplied) {
      rect_float = MEM_mallocN(sizeof(float) * 4 * ibuf->x * ibuf->y, __func__);
      if (rect_float == NULL) {
        return bindcode;
      }
      IMB_colormanagement_imbuf_to_float_texture(
          rect_float, 0, 0, ibuf->x, ibuf->y, ibuf, store_premultiplied);
    }
  }

  /* Create OpenGL texture. */
  GPU_create_gl_tex(&bindcode,
                    (uint *)rect,
                    rect_float,
                    ibuf->x,
                    ibuf->y,
                    textarget,
                    mipmap,
                    half_float,
                    compress_as_srgb,
                    ima);

  /* Free buffers if needed. */
  if (rect && rect != (uchar *)ibuf->rect) {
    MEM_freeN(rect);
  }
  if (rect_float && rect_float != ibuf->rect_float) {
    MEM_freeN(rect_float);
  }

  return bindcode;
}

static GPUTexture **gpu_get_movieclip_gputexture(MovieClip *clip,
                                                 MovieClipUser *cuser,
                                                 GLenum textarget)
{
  MovieClip_RuntimeGPUTexture *tex;
  for (tex = clip->runtime.gputextures.first; tex; tex = tex->next) {
    if (memcmp(&tex->user, cuser, sizeof(MovieClipUser)) == 0) {
      break;
    }
  }

  if (tex == NULL) {
    tex = MEM_mallocN(sizeof(MovieClip_RuntimeGPUTexture), __func__);

    for (int i = 0; i < TEXTARGET_COUNT; i++) {
      tex->gputexture[i] = NULL;
    }

    memcpy(&tex->user, cuser, sizeof(MovieClipUser));
    BLI_addtail(&clip->runtime.gputextures, tex);
  }

  if (textarget == GL_TEXTURE_2D) {
    return &tex->gputexture[TEXTARGET_TEXTURE_2D];
  }
  else if (textarget == GL_TEXTURE_CUBE_MAP) {
    return &tex->gputexture[TEXTARGET_TEXTURE_CUBE_MAP];
  }

  return NULL;
}

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

static void gpu_texture_update_scaled_array(uchar *rect,
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
  ImBuf *ibuf = update_do_scale(
      rect, rect_float, &x, &y, &w, &h, tile_size[0], tile_size[1], full_w, full_h);

  /* Shift to account for tile packing. */
  x += tile_offset[0];
  y += tile_offset[1];

  if (ibuf->rect_float) {
    glTexSubImage3D(
        GL_TEXTURE_2D_ARRAY, 0, x, y, layer, w, h, 1, GL_RGBA, GL_FLOAT, ibuf->rect_float);
  }
  else {
    glTexSubImage3D(
        GL_TEXTURE_2D_ARRAY, 0, x, y, layer, w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
  }

  IMB_freeImBuf(ibuf);
}

static void gpu_texture_update_scaled(
    uchar *rect, float *rect_float, int full_w, int full_h, int x, int y, int w, int h)
{
  /* Partial update with scaling. */
  int limit_w = smaller_power_of_2_limit(full_w);
  int limit_h = smaller_power_of_2_limit(full_h);

  ImBuf *ibuf = update_do_scale(
      rect, rect_float, &x, &y, &w, &h, limit_w, limit_h, full_w, full_h);

  if (ibuf->rect_float) {
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_FLOAT, ibuf->rect_float);
  }
  else {
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
  }

  IMB_freeImBuf(ibuf);
}

static void gpu_texture_update_unscaled(uchar *rect,
                                        float *rect_float,
                                        int x,
                                        int y,
                                        int layer,
                                        int w,
                                        int h,
                                        GLint tex_stride,
                                        GLint tex_offset)
{
  /* Partial update without scaling. Stride and offset are used to copy only a
   * subset of a possible larger buffer than what we are updating. */
  GLint row_length;
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, tex_stride);

  if (layer >= 0) {
    if (rect_float == NULL) {
      glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                      0,
                      x,
                      y,
                      layer,
                      w,
                      h,
                      1,
                      GL_RGBA,
                      GL_UNSIGNED_BYTE,
                      rect + tex_offset);
    }
    else {
      glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
                      0,
                      x,
                      y,
                      layer,
                      w,
                      h,
                      1,
                      GL_RGBA,
                      GL_FLOAT,
                      rect_float + tex_offset);
    }
  }
  else {
    if (rect_float == NULL) {
      glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rect + tex_offset);
    }
    else {
      glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_FLOAT, rect_float + tex_offset);
    }
  }

  glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
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
    scaled = is_over_resolution_limit(GL_TEXTURE_2D, ibuf->x, ibuf->y);
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
  GLint tex_stride = ibuf->x;
  GLint tex_offset = ibuf->channels * (y * ibuf->x + x);

  if (rect_float == NULL) {
    /* Byte pixels. */
    if (!IMB_colormanagement_space_is_data(ibuf->rect_colorspace)) {
      const bool compress_as_srgb = !IMB_colormanagement_space_is_scene_linear(
          ibuf->rect_colorspace);

      rect = MEM_mallocN(sizeof(uchar) * 4 * w * h, __func__);
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
      rect_float = MEM_mallocN(sizeof(float) * 4 * w * h, __func__);
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
      gpu_texture_update_scaled_array(
          rect, rect_float, ibuf->x, ibuf->y, x, y, tilelayer, tileoffset, tilesize, w, h);
    }
    else {
      gpu_texture_update_scaled(rect, rect_float, ibuf->x, ibuf->y, x, y, w, h);
    }
  }
  else {
    /* Fast update at same resolution. */
    if (tile != NULL) {
      int *tileoffset = tile->runtime.tilearray_offset;
      int tilelayer = tile->runtime.tilearray_layer;
      gpu_texture_update_unscaled(rect,
                                  rect_float,
                                  x + tileoffset[0],
                                  y + tileoffset[1],
                                  tilelayer,
                                  w,
                                  h,
                                  tex_stride,
                                  tex_offset);
    }
    else {
      gpu_texture_update_unscaled(rect, rect_float, x, y, -1, w, h, tex_stride, tex_offset);
    }
  }

  /* Free buffers if needed. */
  if (rect && rect != (uchar *)ibuf->rect) {
    MEM_freeN(rect);
  }
  if (rect_float && rect_float != ibuf->rect_float) {
    MEM_freeN(rect_float);
  }

  if (GPU_get_mipmap()) {
    glGenerateMipmap((tile != NULL) ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D);
  }
  else {
    ima->gpuflag &= ~IMA_GPU_MIPMAP_COMPLETE;
  }

  GPU_texture_unbind(tex);
}

/* Get the GPUTexture for a given `Image`.
 *
 * `iuser` and `ibuf` are mutual exclusive parameters. The caller can pass the `ibuf` when already
 * available. It is also required when requesting the GPUTexture for a render result. */
GPUTexture *GPU_texture_from_blender(Image *ima, ImageUser *iuser, ImBuf *ibuf, int textarget)
{
#ifndef GPU_STANDALONE
  if (ima == NULL) {
    return NULL;
  }

  /* currently, gpu refresh tagging is used by ima sequences */
  if (ima->gpuflag & IMA_GPU_REFRESH) {
    gpu_free_image_immediate(ima);
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
  uint bindcode = 0;
  ImageTile *tile = BKE_image_get_tile(ima, 0);
  if (tile == NULL || tile->ok == 0) {
    *tex = GPU_texture_from_bindcode(textarget, bindcode);
    return *tex;
  }

  /* check if we have a valid image buffer */
  ImBuf *ibuf_intern = ibuf;
  if (ibuf_intern == NULL) {
    ibuf_intern = BKE_image_acquire_ibuf(ima, iuser, NULL);
    if (ibuf_intern == NULL) {
      *tex = GPU_texture_from_bindcode(textarget, bindcode);
      return *tex;
    }
  }

  if (textarget == GL_TEXTURE_2D_ARRAY) {
    bindcode = gpu_texture_create_tile_array(ima, ibuf_intern);
  }
  else if (textarget == GL_TEXTURE_1D_ARRAY) {
    bindcode = gpu_texture_create_tile_mapping(ima, iuser ? iuser->multiview_eye : 0);
  }
  else {
    bindcode = gpu_texture_create_from_ibuf(ima, ibuf_intern, textarget);
  }

  /* if `ibuf` was given, we don't own the `ibuf_intern` */
  if (ibuf == NULL) {
    BKE_image_release_ibuf(ima, ibuf_intern, NULL);
  }

  *tex = GPU_texture_from_bindcode(textarget, bindcode);

  GPU_texture_orig_size_set(*tex, ibuf_intern->x, ibuf_intern->y);

  if (textarget == GL_TEXTURE_1D_ARRAY) {
    /* Special for tile mapping. */
    GPU_texture_mipmap_mode(*tex, false, false);
  }

  return *tex;
#endif
  return NULL;
}

GPUTexture *GPU_texture_from_movieclip(MovieClip *clip, MovieClipUser *cuser, int textarget)
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
  uint bindcode = 0;
  ImBuf *ibuf = BKE_movieclip_get_ibuf(clip, cuser);
  if (ibuf == NULL) {
    *tex = GPU_texture_from_bindcode(textarget, bindcode);
    return *tex;
  }

  bindcode = gpu_texture_create_from_ibuf(NULL, ibuf, textarget);
  IMB_freeImBuf(ibuf);

  *tex = GPU_texture_from_bindcode(textarget, bindcode);
  return *tex;
#else
  return NULL;
#endif
}

void GPU_free_texture_movieclip(struct MovieClip *clip)
{
  /* number of gpu textures to keep around as cache
   * We don't want to keep too many GPU textures for
   * movie clips around, as they can be large.*/
  const int MOVIECLIP_NUM_GPUTEXTURES = 1;

  while (BLI_listbase_count(&clip->runtime.gputextures) > MOVIECLIP_NUM_GPUTEXTURES) {
    MovieClip_RuntimeGPUTexture *tex = BLI_pophead(&clip->runtime.gputextures);
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

static void **gpu_gen_cube_map(uint *rect, float *frect, int rectw, int recth)
{
  size_t block_size = frect ? sizeof(float[4]) : sizeof(uchar[4]);
  void **sides = NULL;
  int h = recth / 2;
  int w = rectw / 3;

  if (w != h) {
    return sides;
  }

  /* PosX, NegX, PosY, NegY, PosZ, NegZ */
  sides = MEM_mallocN(sizeof(void *) * 6, "");
  for (int i = 0; i < 6; i++) {
    sides[i] = MEM_mallocN(block_size * w * h, "");
  }

  /* divide image into six parts */
  /* ______________________
   * |      |      |      |
   * | NegX | NegY | PosX |
   * |______|______|______|
   * |      |      |      |
   * | NegZ | PosZ | PosY |
   * |______|______|______|
   */
  if (frect) {
    float(*frectb)[4] = (float(*)[4])frect;
    float(**fsides)[4] = (float(**)[4])sides;

    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        memcpy(&fsides[0][x * h + y], &frectb[(recth - y - 1) * rectw + 2 * w + x], block_size);
        memcpy(&fsides[1][x * h + y], &frectb[(y + h) * rectw + w - 1 - x], block_size);
        memcpy(
            &fsides[3][y * w + x], &frectb[(recth - y - 1) * rectw + 2 * w - 1 - x], block_size);
        memcpy(&fsides[5][y * w + x], &frectb[(h - y - 1) * rectw + w - 1 - x], block_size);
      }
      memcpy(&fsides[2][y * w], frectb[y * rectw + 2 * w], block_size * w);
      memcpy(&fsides[4][y * w], frectb[y * rectw + w], block_size * w);
    }
  }
  else {
    uint **isides = (uint **)sides;

    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        isides[0][x * h + y] = rect[(recth - y - 1) * rectw + 2 * w + x];
        isides[1][x * h + y] = rect[(y + h) * rectw + w - 1 - x];
        isides[3][y * w + x] = rect[(recth - y - 1) * rectw + 2 * w - 1 - x];
        isides[5][y * w + x] = rect[(h - y - 1) * rectw + w - 1 - x];
      }
      memcpy(&isides[2][y * w], &rect[y * rectw + 2 * w], block_size * w);
      memcpy(&isides[4][y * w], &rect[y * rectw + w], block_size * w);
    }
  }

  return sides;
}

static void gpu_del_cube_map(void **cube_map)
{
  int i;
  if (cube_map == NULL) {
    return;
  }
  for (i = 0; i < 6; i++) {
    MEM_freeN(cube_map[i]);
  }
  MEM_freeN(cube_map);
}

/* Image *ima can be NULL */
void GPU_create_gl_tex(uint *bind,
                       uint *rect,
                       float *frect,
                       int rectw,
                       int recth,
                       int textarget,
                       bool mipmap,
                       bool half_float,
                       bool use_srgb,
                       Image *ima)
{
  ImBuf *ibuf = NULL;

  if (textarget == GL_TEXTURE_2D && is_over_resolution_limit(textarget, rectw, recth)) {
    int tpx = rectw;
    int tpy = recth;
    rectw = smaller_power_of_2_limit(rectw);
    recth = smaller_power_of_2_limit(recth);

    if (frect) {
      ibuf = IMB_allocFromBuffer(NULL, frect, tpx, tpy, 4);
      IMB_scaleImBuf(ibuf, rectw, recth);

      frect = ibuf->rect_float;
    }
    else {
      ibuf = IMB_allocFromBuffer(rect, NULL, tpx, tpy, 4);
      IMB_scaleImBuf(ibuf, rectw, recth);

      rect = ibuf->rect;
    }
  }

  /* create image */
  glGenTextures(1, (GLuint *)bind);
  glBindTexture(textarget, *bind);

  GLenum float_format = (!half_float && (ima && (ima->flag & IMA_HIGH_BITDEPTH))) ? GL_RGBA32F :
                                                                                    GL_RGBA16F;
  GLenum internal_format = (frect) ? float_format : (use_srgb) ? GL_SRGB8_ALPHA8 : GL_RGBA8;

  if (textarget == GL_TEXTURE_2D) {
    if (frect) {
      glTexImage2D(GL_TEXTURE_2D, 0, internal_format, rectw, recth, 0, GL_RGBA, GL_FLOAT, frect);
    }
    else {
      glTexImage2D(
          GL_TEXTURE_2D, 0, internal_format, rectw, recth, 0, GL_RGBA, GL_UNSIGNED_BYTE, rect);
    }

    if (GPU_get_mipmap() && mipmap) {
      glGenerateMipmap(GL_TEXTURE_2D);
      if (ima) {
        ima->gpuflag |= IMA_GPU_MIPMAP_COMPLETE;
      }
    }
  }
  else if (textarget == GL_TEXTURE_CUBE_MAP) {
    int w = rectw / 3, h = recth / 2;

    if (h == w && is_power_of_2_i(h) && !is_over_resolution_limit(textarget, h, w)) {
      void **cube_map = gpu_gen_cube_map(rect, frect, rectw, recth);
      GLenum type = frect ? GL_FLOAT : GL_UNSIGNED_BYTE;

      if (cube_map) {
        for (int i = 0; i < 6; i++) {
          glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                       0,
                       internal_format,
                       w,
                       h,
                       0,
                       GL_RGBA,
                       type,
                       cube_map[i]);
        }
      }

      if (GPU_get_mipmap() && mipmap) {
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        if (ima) {
          ima->gpuflag |= IMA_GPU_MIPMAP_COMPLETE;
        }
      }

      gpu_del_cube_map(cube_map);
    }
    else {
      printf("Incorrect envmap size\n");
    }
  }

  glBindTexture(textarget, 0);

  if (ibuf) {
    IMB_freeImBuf(ibuf);
  }
}

/**
 * GPU_upload_dxt_texture() assumes that the texture is already bound and ready to go.
 * This is so the viewport and the BGE can share some code.
 * Returns false if the provided ImBuf doesn't have a supported DXT compression format
 */
bool GPU_upload_dxt_texture(ImBuf *ibuf, bool use_srgb)
{
#ifdef WITH_DDS
  GLint format = 0;
  int blocksize, height, width, i, size, offset = 0;

  width = ibuf->x;
  height = ibuf->y;

  if (GLEW_EXT_texture_compression_s3tc) {
    if (ibuf->dds_data.fourcc == FOURCC_DXT1) {
      format = (use_srgb) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT :
                            GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
    }
    else if (ibuf->dds_data.fourcc == FOURCC_DXT3) {
      format = (use_srgb) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT :
                            GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    }
    else if (ibuf->dds_data.fourcc == FOURCC_DXT5) {
      format = (use_srgb) ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT :
                            GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    }
  }

  if (format == 0) {
    fprintf(stderr, "Unable to find a suitable DXT compression, falling back to uncompressed\n");
    return false;
  }

  if (!is_power_of_2_resolution(width, height)) {
    fprintf(
        stderr,
        "Unable to load non-power-of-two DXT image resolution, falling back to uncompressed\n");
    return false;
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gpu_get_mipmap_filter(0));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gpu_get_mipmap_filter(1));

  blocksize = (ibuf->dds_data.fourcc == FOURCC_DXT1) ? 8 : 16;
  for (i = 0; i < ibuf->dds_data.nummipmaps && (width || height); i++) {
    if (width == 0) {
      width = 1;
    }
    if (height == 0) {
      height = 1;
    }

    size = ((width + 3) / 4) * ((height + 3) / 4) * blocksize;

    glCompressedTexImage2D(
        GL_TEXTURE_2D, i, format, width, height, 0, size, ibuf->dds_data.data + offset);

    offset += size;
    width >>= 1;
    height >>= 1;
  }

  /* set number of mipmap levels we have, needed in case they don't go down to 1x1 */
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, i - 1);

  return true;
#else
  UNUSED_VARS(ibuf, use_srgb);
  return false;
#endif
}

void GPU_create_gl_tex_compressed(unsigned int *bind, int textarget, Image *ima, ImBuf *ibuf)
{
  /* For DDS we only support data, scene linear and sRGB. Converting to
   * different colorspace would break the compression. */
  const bool use_srgb = !(IMB_colormanagement_space_is_data(ibuf->rect_colorspace) ||
                          IMB_colormanagement_space_is_scene_linear(ibuf->rect_colorspace));
  const bool mipmap = GPU_get_mipmap();
  const bool half_float = (ibuf->flags & IB_halffloat) != 0;

#ifndef WITH_DDS
  (void)ibuf;
  /* Fall back to uncompressed if DDS isn't enabled */
  GPU_create_gl_tex(
      bind, ibuf->rect, NULL, ibuf->x, ibuf->y, textarget, mipmap, half_float, use_srgb, ima);
#else
  glGenTextures(1, (GLuint *)bind);
  glBindTexture(textarget, *bind);

  if (textarget == GL_TEXTURE_2D && GPU_upload_dxt_texture(ibuf, use_srgb) == 0) {
    glDeleteTextures(1, (GLuint *)bind);
    GPU_create_gl_tex(
        bind, ibuf->rect, NULL, ibuf->x, ibuf->y, textarget, mipmap, half_float, use_srgb, ima);
  }

  glBindTexture(textarget, 0);
#endif
}

/* these two functions are called on entering and exiting texture paint mode,
 * temporary disabling/enabling mipmapping on all images for quick texture
 * updates with glTexSubImage2D. images that didn't change don't have to be
 * re-uploaded to OpenGL */
void GPU_paint_set_mipmap(Main *bmain, bool mipmap)
{
#ifndef GPU_STANDALONE
  if (!GTS.domipmap) {
    return;
  }

  GTS.texpaint = !mipmap;

  if (mipmap) {
    for (Image *ima = bmain->images.first; ima; ima = ima->id.next) {
      if (BKE_image_has_opengl_texture(ima)) {
        if (ima->gpuflag & IMA_GPU_MIPMAP_COMPLETE) {
          for (int eye = 0; eye < 2; eye++) {
            for (int a = 0; a < TEXTARGET_COUNT; a++) {
              if (ELEM(a, TEXTARGET_TEXTURE_2D, TEXTARGET_TEXTURE_2D_ARRAY)) {
                GPUTexture *tex = ima->gputexture[a][eye];
                if (tex != NULL) {
                  GPU_texture_bind(tex, 0);
                  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gpu_get_mipmap_filter(0));
                  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gpu_get_mipmap_filter(1));
                  GPU_texture_unbind(tex);
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
  }
  else {
    for (Image *ima = bmain->images.first; ima; ima = ima->id.next) {
      if (BKE_image_has_opengl_texture(ima)) {
        for (int eye = 0; eye < 2; eye++) {
          for (int a = 0; a < TEXTARGET_COUNT; a++) {
            if (ELEM(a, TEXTARGET_TEXTURE_2D, TEXTARGET_TEXTURE_2D_ARRAY)) {
              GPUTexture *tex = ima->gputexture[a][eye];
              if (tex != NULL) {
                GPU_texture_bind(tex, 0);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gpu_get_mipmap_filter(1));
                GPU_texture_unbind(tex);
              }
            }
          }
        }
      }
      else {
        ima->gpuflag &= ~IMA_GPU_MIPMAP_COMPLETE;
      }
    }
  }
#endif /* GPU_STANDALONE */
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

  GPUTexture *tex = ima->gputexture[TEXTARGET_TEXTURE_2D][0];
  /* Check if we need to update the main gputexture. */
  if (tex != NULL && tile == ima->tiles.first) {
    gpu_texture_update_from_ibuf(tex, ima, ibuf, NULL, x, y, w, h);
  }

  /* Check if we need to update the array gputexture. */
  tex = ima->gputexture[TEXTARGET_TEXTURE_2D_ARRAY][0];
  if (tex != NULL) {
    gpu_texture_update_from_ibuf(tex, ima, ibuf, tile, x, y, w, h);
  }

  BKE_image_release_ibuf(ima, ibuf, NULL);
#endif
}

static LinkNode *image_free_queue = NULL;
static ThreadMutex img_queue_mutex = BLI_MUTEX_INITIALIZER;

static void gpu_queue_image_for_free(Image *ima)
{
  BLI_mutex_lock(&img_queue_mutex);
  BLI_linklist_prepend(&image_free_queue, ima);
  BLI_mutex_unlock(&img_queue_mutex);
}

void GPU_free_unused_buffers(Main *bmain)
{
  if (!BLI_thread_is_main()) {
    return;
  }

  BLI_mutex_lock(&img_queue_mutex);

  /* images */
  for (LinkNode *node = image_free_queue; node; node = node->next) {
    Image *ima = node->link;

    /* check in case it was freed in the meantime */
    if (bmain && BLI_findindex(&bmain->images, ima) != -1) {
      GPU_free_image(ima);
    }
  }

  BLI_linklist_free(image_free_queue, NULL);
  image_free_queue = NULL;

  BLI_mutex_unlock(&img_queue_mutex);
}

static void gpu_free_image_immediate(Image *ima)
{
  for (int eye = 0; eye < 2; eye++) {
    for (int i = 0; i < TEXTARGET_COUNT; i++) {
      /* free glsl image binding */
      if (ima->gputexture[i][eye] != NULL) {
        GPU_texture_free(ima->gputexture[i][eye]);
        ima->gputexture[i][eye] = NULL;
      }
    }
  }

  ima->gpuflag &= ~IMA_GPU_MIPMAP_COMPLETE;
}

void GPU_free_image(Image *ima)
{
  if (!BLI_thread_is_main()) {
    gpu_queue_image_for_free(ima);
    return;
  }

  gpu_free_image_immediate(ima);
}

void GPU_free_images(Main *bmain)
{
  if (bmain) {
    for (Image *ima = bmain->images.first; ima; ima = ima->id.next) {
      GPU_free_image(ima);
    }
  }
}

/* same as above but only free animated images */
void GPU_free_images_anim(Main *bmain)
{
  if (bmain) {
    for (Image *ima = bmain->images.first; ima; ima = ima->id.next) {
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

  Image *ima = bmain->images.first;
  while (ima) {
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
    ima = ima->id.next;
  }
}
