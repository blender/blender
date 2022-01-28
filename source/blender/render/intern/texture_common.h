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
 * \ingroup render
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define BRICONT \
  texres->tin = (texres->tin - 0.5f) * tex->contrast + tex->bright - 0.5f; \
  if (!(tex->flag & TEX_NO_CLAMP)) { \
    if (texres->tin < 0.0f) { \
      texres->tin = 0.0f; \
    } \
    else if (texres->tin > 1.0f) { \
      texres->tin = 1.0f; \
    } \
  } \
  ((void)0)

#define BRICONTRGB \
  texres->trgba[0] = tex->rfac * \
                     ((texres->trgba[0] - 0.5f) * tex->contrast + tex->bright - 0.5f); \
  texres->trgba[1] = tex->gfac * \
                     ((texres->trgba[1] - 0.5f) * tex->contrast + tex->bright - 0.5f); \
  texres->trgba[2] = tex->bfac * \
                     ((texres->trgba[2] - 0.5f) * tex->contrast + tex->bright - 0.5f); \
  if (!(tex->flag & TEX_NO_CLAMP)) { \
    if (texres->trgba[0] < 0.0f) { \
      texres->trgba[0] = 0.0f; \
    } \
    if (texres->trgba[1] < 0.0f) { \
      texres->trgba[1] = 0.0f; \
    } \
    if (texres->trgba[2] < 0.0f) { \
      texres->trgba[2] = 0.0f; \
    } \
  } \
  if (tex->saturation != 1.0f) { \
    float _hsv[3]; \
    rgb_to_hsv(texres->trgba[0], texres->trgba[1], texres->trgba[2], _hsv, _hsv + 1, _hsv + 2); \
    _hsv[1] *= tex->saturation; \
    hsv_to_rgb( \
        _hsv[0], _hsv[1], _hsv[2], &texres->trgba[0], &texres->trgba[1], &texres->trgba[2]); \
    if ((tex->saturation > 1.0f) && !(tex->flag & TEX_NO_CLAMP)) { \
      if (texres->trgba[0] < 0.0f) { \
        texres->trgba[0] = 0.0f; \
      } \
      if (texres->trgba[1] < 0.0f) { \
        texres->trgba[1] = 0.0f; \
      } \
      if (texres->trgba[2] < 0.0f) { \
        texres->trgba[2] = 0.0f; \
      } \
    } \
  } \
  ((void)0)

struct ImBuf;
struct Image;
struct ImagePool;
struct Tex;
struct TexResult;

/* texture_image.c */

int imagewraposa(struct Tex *tex,
                 struct Image *ima,
                 struct ImBuf *ibuf,
                 const float texvec[3],
                 const float dxt[2],
                 const float dyt[2],
                 struct TexResult *texres,
                 struct ImagePool *pool,
                 bool skip_load_image);
int imagewrap(struct Tex *tex,
              struct Image *ima,
              const float texvec[3],
              struct TexResult *texres,
              struct ImagePool *pool,
              bool skip_load_image);
void image_sample(struct Image *ima,
                  float fx,
                  float fy,
                  float dx,
                  float dy,
                  float result[4],
                  struct ImagePool *pool);

#ifdef __cplusplus
}
#endif
