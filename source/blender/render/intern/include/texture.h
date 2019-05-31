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

#ifndef __TEXTURE_H__
#define __TEXTURE_H__

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
  texres->tr = tex->rfac * ((texres->tr - 0.5f) * tex->contrast + tex->bright - 0.5f); \
  texres->tg = tex->gfac * ((texres->tg - 0.5f) * tex->contrast + tex->bright - 0.5f); \
  texres->tb = tex->bfac * ((texres->tb - 0.5f) * tex->contrast + tex->bright - 0.5f); \
  if (!(tex->flag & TEX_NO_CLAMP)) { \
    if (texres->tr < 0.0f) \
      texres->tr = 0.0f; \
    if (texres->tg < 0.0f) \
      texres->tg = 0.0f; \
    if (texres->tb < 0.0f) \
      texres->tb = 0.0f; \
  } \
  if (tex->saturation != 1.0f) { \
    float _hsv[3]; \
    rgb_to_hsv(texres->tr, texres->tg, texres->tb, _hsv, _hsv + 1, _hsv + 2); \
    _hsv[1] *= tex->saturation; \
    hsv_to_rgb(_hsv[0], _hsv[1], _hsv[2], &texres->tr, &texres->tg, &texres->tb); \
    if ((tex->saturation > 1.0f) && !(tex->flag & TEX_NO_CLAMP)) { \
      if (texres->tr < 0.0f) \
        texres->tr = 0.0f; \
      if (texres->tg < 0.0f) \
        texres->tg = 0.0f; \
      if (texres->tb < 0.0f) \
        texres->tb = 0.0f; \
    } \
  } \
  ((void)0)

struct ImBuf;
struct Image;
struct ImagePool;
struct Tex;
struct TexResult;

/* imagetexture.h */

int imagewraposa(struct Tex *tex,
                 struct Image *ima,
                 struct ImBuf *ibuf,
                 const float texvec[3],
                 const float dxt[2],
                 const float dyt[2],
                 struct TexResult *texres,
                 struct ImagePool *pool,
                 const bool skip_load_image);
int imagewrap(struct Tex *tex,
              struct Image *ima,
              struct ImBuf *ibuf,
              const float texvec[3],
              struct TexResult *texres,
              struct ImagePool *pool,
              const bool skip_load_image);
void image_sample(struct Image *ima,
                  float fx,
                  float fy,
                  float dx,
                  float dy,
                  float result[4],
                  struct ImagePool *pool);

#endif /* __TEXTURE_H__ */
