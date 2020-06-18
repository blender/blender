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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_math.h"
#include "BLI_noise.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_image_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"

#include "BKE_image.h"
#include "BKE_node.h"

#include "BKE_colorband.h"
#include "BKE_material.h"
#include "BKE_scene.h"

#include "BKE_texture.h"

#include "MEM_guardedalloc.h"

#include "render_types.h"
#include "texture.h"

#include "RE_render_ext.h"
#include "RE_shader_ext.h"

static RNG_THREAD_ARRAY *random_tex_array;

void RE_texture_rng_init(void)
{
  random_tex_array = BLI_rng_threaded_new();
}

void RE_texture_rng_exit(void)
{
  if (random_tex_array == NULL) {
    return;
  }
  BLI_rng_threaded_free(random_tex_array);
  random_tex_array = NULL;
}

/* ------------------------------------------------------------------------- */

/* this allows colorbanded textures to control normals as well */
static void tex_normal_derivate(const Tex *tex, TexResult *texres)
{
  if (tex->flag & TEX_COLORBAND) {
    float col[4];
    if (BKE_colorband_evaluate(tex->coba, texres->tin, col)) {
      float fac0, fac1, fac2, fac3;

      fac0 = (col[0] + col[1] + col[2]);
      BKE_colorband_evaluate(tex->coba, texres->nor[0], col);
      fac1 = (col[0] + col[1] + col[2]);
      BKE_colorband_evaluate(tex->coba, texres->nor[1], col);
      fac2 = (col[0] + col[1] + col[2]);
      BKE_colorband_evaluate(tex->coba, texres->nor[2], col);
      fac3 = (col[0] + col[1] + col[2]);

      texres->nor[0] = (fac0 - fac1) / 3.0f;
      texres->nor[1] = (fac0 - fac2) / 3.0f;
      texres->nor[2] = (fac0 - fac3) / 3.0f;

      return;
    }
  }
  texres->nor[0] = texres->tin - texres->nor[0];
  texres->nor[1] = texres->tin - texres->nor[1];
  texres->nor[2] = texres->tin - texres->nor[2];
}

static int blend(const Tex *tex, const float texvec[3], TexResult *texres)
{
  float x, y, t;

  if (tex->flag & TEX_FLIPBLEND) {
    x = texvec[1];
    y = texvec[0];
  }
  else {
    x = texvec[0];
    y = texvec[1];
  }

  if (tex->stype == TEX_LIN) { /* lin */
    texres->tin = (1.0f + x) / 2.0f;
  }
  else if (tex->stype == TEX_QUAD) { /* quad */
    texres->tin = (1.0f + x) / 2.0f;
    if (texres->tin < 0.0f) {
      texres->tin = 0.0f;
    }
    else {
      texres->tin *= texres->tin;
    }
  }
  else if (tex->stype == TEX_EASE) { /* ease */
    texres->tin = (1.0f + x) / 2.0f;
    if (texres->tin <= 0.0f) {
      texres->tin = 0.0f;
    }
    else if (texres->tin >= 1.0f) {
      texres->tin = 1.0f;
    }
    else {
      t = texres->tin * texres->tin;
      texres->tin = (3.0f * t - 2.0f * t * texres->tin);
    }
  }
  else if (tex->stype == TEX_DIAG) { /* diag */
    texres->tin = (2.0f + x + y) / 4.0f;
  }
  else if (tex->stype == TEX_RAD) { /* radial */
    texres->tin = (atan2f(y, x) / (float)(2 * M_PI) + 0.5f);
  }
  else { /* sphere TEX_SPHERE */
    texres->tin = 1.0f - sqrtf(x * x + y * y + texvec[2] * texvec[2]);
    if (texres->tin < 0.0f) {
      texres->tin = 0.0f;
    }
    if (tex->stype == TEX_HALO) {
      texres->tin *= texres->tin; /* halo */
    }
  }

  BRICONT;

  return TEX_INT;
}

/* ------------------------------------------------------------------------- */
/* ************************************************************************* */

/* newnoise: all noisebased types now have different noisebases to choose from */

static int clouds(const Tex *tex, const float texvec[3], TexResult *texres)
{
  int rv = TEX_INT;

  texres->tin = BLI_gTurbulence(tex->noisesize,
                                texvec[0],
                                texvec[1],
                                texvec[2],
                                tex->noisedepth,
                                (tex->noisetype != TEX_NOISESOFT),
                                tex->noisebasis);

  if (texres->nor != NULL) {
    /* calculate bumpnormal */
    texres->nor[0] = BLI_gTurbulence(tex->noisesize,
                                     texvec[0] + tex->nabla,
                                     texvec[1],
                                     texvec[2],
                                     tex->noisedepth,
                                     (tex->noisetype != TEX_NOISESOFT),
                                     tex->noisebasis);
    texres->nor[1] = BLI_gTurbulence(tex->noisesize,
                                     texvec[0],
                                     texvec[1] + tex->nabla,
                                     texvec[2],
                                     tex->noisedepth,
                                     (tex->noisetype != TEX_NOISESOFT),
                                     tex->noisebasis);
    texres->nor[2] = BLI_gTurbulence(tex->noisesize,
                                     texvec[0],
                                     texvec[1],
                                     texvec[2] + tex->nabla,
                                     tex->noisedepth,
                                     (tex->noisetype != TEX_NOISESOFT),
                                     tex->noisebasis);

    tex_normal_derivate(tex, texres);
    rv |= TEX_NOR;
  }

  if (tex->stype == TEX_COLOR) {
    /* in this case, int. value should really be computed from color,
     * and bumpnormal from that, would be too slow, looks ok as is */
    texres->tr = texres->tin;
    texres->tg = BLI_gTurbulence(tex->noisesize,
                                 texvec[1],
                                 texvec[0],
                                 texvec[2],
                                 tex->noisedepth,
                                 (tex->noisetype != TEX_NOISESOFT),
                                 tex->noisebasis);
    texres->tb = BLI_gTurbulence(tex->noisesize,
                                 texvec[1],
                                 texvec[2],
                                 texvec[0],
                                 tex->noisedepth,
                                 (tex->noisetype != TEX_NOISESOFT),
                                 tex->noisebasis);
    BRICONTRGB;
    texres->ta = 1.0;
    return (rv | TEX_RGB);
  }

  BRICONT;

  return rv;
}

/* creates a sine wave */
static float tex_sin(float a)
{
  a = 0.5f + 0.5f * sinf(a);

  return a;
}

/* creates a saw wave */
static float tex_saw(float a)
{
  const float b = 2 * M_PI;

  int n = (int)(a / b);
  a -= n * b;
  if (a < 0) {
    a += b;
  }
  return a / b;
}

/* creates a triangle wave */
static float tex_tri(float a)
{
  const float b = 2 * M_PI;
  const float rmax = 1.0;

  a = rmax - 2.0f * fabsf(floorf((a * (1.0f / b)) + 0.5f) - (a * (1.0f / b)));

  return a;
}

/* computes basic wood intensity value at x,y,z */
static float wood_int(const Tex *tex, float x, float y, float z)
{
  float wi = 0;
  /* wave form:   TEX_SIN=0,  TEX_SAW=1,  TEX_TRI=2 */
  short wf = tex->noisebasis2;
  /* wood type:   TEX_BAND=0, TEX_RING=1, TEX_BANDNOISE=2, TEX_RINGNOISE=3 */
  short wt = tex->stype;

  float (*waveform[3])(float); /* create array of pointers to waveform functions */
  waveform[0] = tex_sin;       /* assign address of tex_sin() function to pointer array */
  waveform[1] = tex_saw;
  waveform[2] = tex_tri;

  if ((wf > TEX_TRI) || (wf < TEX_SIN)) {
    wf = 0; /* check to be sure noisebasis2 is initialized ahead of time */
  }

  if (wt == TEX_BAND) {
    wi = waveform[wf]((x + y + z) * 10.0f);
  }
  else if (wt == TEX_RING) {
    wi = waveform[wf](sqrtf(x * x + y * y + z * z) * 20.0f);
  }
  else if (wt == TEX_BANDNOISE) {
    wi = tex->turbul *
         BLI_gNoise(tex->noisesize, x, y, z, (tex->noisetype != TEX_NOISESOFT), tex->noisebasis);
    wi = waveform[wf]((x + y + z) * 10.0f + wi);
  }
  else if (wt == TEX_RINGNOISE) {
    wi = tex->turbul *
         BLI_gNoise(tex->noisesize, x, y, z, (tex->noisetype != TEX_NOISESOFT), tex->noisebasis);
    wi = waveform[wf](sqrtf(x * x + y * y + z * z) * 20.0f + wi);
  }

  return wi;
}

static int wood(const Tex *tex, const float texvec[3], TexResult *texres)
{
  int rv = TEX_INT;

  texres->tin = wood_int(tex, texvec[0], texvec[1], texvec[2]);
  if (texres->nor != NULL) {
    /* calculate bumpnormal */
    texres->nor[0] = wood_int(tex, texvec[0] + tex->nabla, texvec[1], texvec[2]);
    texres->nor[1] = wood_int(tex, texvec[0], texvec[1] + tex->nabla, texvec[2]);
    texres->nor[2] = wood_int(tex, texvec[0], texvec[1], texvec[2] + tex->nabla);

    tex_normal_derivate(tex, texres);
    rv |= TEX_NOR;
  }

  BRICONT;

  return rv;
}

/* computes basic marble intensity at x,y,z */
static float marble_int(const Tex *tex, float x, float y, float z)
{
  float n, mi;
  short wf = tex->noisebasis2; /* wave form:   TEX_SIN=0, TEX_SAW=1, TEX_TRI=2 */
  short mt = tex->stype;       /* marble type: TEX_SOFT=0, TEX_SHARP=1, TEX_SHAPER=2 */

  float (*waveform[3])(float); /* create array of pointers to waveform functions */
  waveform[0] = tex_sin;       /* assign address of tex_sin() function to pointer array */
  waveform[1] = tex_saw;
  waveform[2] = tex_tri;

  if ((wf > TEX_TRI) || (wf < TEX_SIN)) {
    wf = 0; /* check to be sure noisebasis2 isn't initialized ahead of time */
  }

  n = 5.0f * (x + y + z);

  mi = n + tex->turbul * BLI_gTurbulence(tex->noisesize,
                                         x,
                                         y,
                                         z,
                                         tex->noisedepth,
                                         (tex->noisetype != TEX_NOISESOFT),
                                         tex->noisebasis);

  if (mt >= TEX_SOFT) { /* TEX_SOFT always true */
    mi = waveform[wf](mi);
    if (mt == TEX_SHARP) {
      mi = sqrtf(mi);
    }
    else if (mt == TEX_SHARPER) {
      mi = sqrtf(sqrtf(mi));
    }
  }

  return mi;
}

static int marble(const Tex *tex, const float texvec[3], TexResult *texres)
{
  int rv = TEX_INT;

  texres->tin = marble_int(tex, texvec[0], texvec[1], texvec[2]);

  if (texres->nor != NULL) {
    /* calculate bumpnormal */
    texres->nor[0] = marble_int(tex, texvec[0] + tex->nabla, texvec[1], texvec[2]);
    texres->nor[1] = marble_int(tex, texvec[0], texvec[1] + tex->nabla, texvec[2]);
    texres->nor[2] = marble_int(tex, texvec[0], texvec[1], texvec[2] + tex->nabla);

    tex_normal_derivate(tex, texres);

    rv |= TEX_NOR;
  }

  BRICONT;

  return rv;
}

/* ------------------------------------------------------------------------- */

static int magic(const Tex *tex, const float texvec[3], TexResult *texres)
{
  float x, y, z, turb;
  int n;

  n = tex->noisedepth;
  turb = tex->turbul / 5.0f;

  x = sinf((texvec[0] + texvec[1] + texvec[2]) * 5.0f);
  y = cosf((-texvec[0] + texvec[1] - texvec[2]) * 5.0f);
  z = -cosf((-texvec[0] - texvec[1] + texvec[2]) * 5.0f);
  if (n > 0) {
    x *= turb;
    y *= turb;
    z *= turb;
    y = -cosf(x - y + z);
    y *= turb;
    if (n > 1) {
      x = cosf(x - y - z);
      x *= turb;
      if (n > 2) {
        z = sinf(-x - y - z);
        z *= turb;
        if (n > 3) {
          x = -cosf(-x + y - z);
          x *= turb;
          if (n > 4) {
            y = -sinf(-x + y + z);
            y *= turb;
            if (n > 5) {
              y = -cosf(-x + y + z);
              y *= turb;
              if (n > 6) {
                x = cosf(x + y + z);
                x *= turb;
                if (n > 7) {
                  z = sinf(x + y - z);
                  z *= turb;
                  if (n > 8) {
                    x = -cosf(-x - y + z);
                    x *= turb;
                    if (n > 9) {
                      y = -sinf(x - y + z);
                      y *= turb;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  if (turb != 0.0f) {
    turb *= 2.0f;
    x /= turb;
    y /= turb;
    z /= turb;
  }
  texres->tr = 0.5f - x;
  texres->tg = 0.5f - y;
  texres->tb = 0.5f - z;

  texres->tin = (1.0f / 3.0f) * (texres->tr + texres->tg + texres->tb);

  BRICONTRGB;
  texres->ta = 1.0f;

  return TEX_RGB;
}

/* ------------------------------------------------------------------------- */

/* newnoise: stucci also modified to use different noisebasis */
static int stucci(const Tex *tex, const float texvec[3], TexResult *texres)
{
  float nor[3], b2, ofs;
  int retval = TEX_INT;

  b2 = BLI_gNoise(tex->noisesize,
                  texvec[0],
                  texvec[1],
                  texvec[2],
                  (tex->noisetype != TEX_NOISESOFT),
                  tex->noisebasis);

  ofs = tex->turbul / 200.0f;

  if (tex->stype) {
    ofs *= (b2 * b2);
  }
  nor[0] = BLI_gNoise(tex->noisesize,
                      texvec[0] + ofs,
                      texvec[1],
                      texvec[2],
                      (tex->noisetype != TEX_NOISESOFT),
                      tex->noisebasis);
  nor[1] = BLI_gNoise(tex->noisesize,
                      texvec[0],
                      texvec[1] + ofs,
                      texvec[2],
                      (tex->noisetype != TEX_NOISESOFT),
                      tex->noisebasis);
  nor[2] = BLI_gNoise(tex->noisesize,
                      texvec[0],
                      texvec[1],
                      texvec[2] + ofs,
                      (tex->noisetype != TEX_NOISESOFT),
                      tex->noisebasis);

  texres->tin = nor[2];

  if (texres->nor) {

    copy_v3_v3(texres->nor, nor);
    tex_normal_derivate(tex, texres);

    if (tex->stype == TEX_WALLOUT) {
      texres->nor[0] = -texres->nor[0];
      texres->nor[1] = -texres->nor[1];
      texres->nor[2] = -texres->nor[2];
    }

    retval |= TEX_NOR;
  }

  if (tex->stype == TEX_WALLOUT) {
    texres->tin = 1.0f - texres->tin;
  }

  if (texres->tin < 0.0f) {
    texres->tin = 0.0f;
  }

  return retval;
}

/* ------------------------------------------------------------------------- */
/* newnoise: musgrave terrain noise types */

static int mg_mFractalOrfBmTex(const Tex *tex, const float texvec[3], TexResult *texres)
{
  int rv = TEX_INT;
  float (*mgravefunc)(float, float, float, float, float, float, int);

  if (tex->stype == TEX_MFRACTAL) {
    mgravefunc = mg_MultiFractal;
  }
  else {
    mgravefunc = mg_fBm;
  }

  texres->tin = tex->ns_outscale * mgravefunc(texvec[0],
                                              texvec[1],
                                              texvec[2],
                                              tex->mg_H,
                                              tex->mg_lacunarity,
                                              tex->mg_octaves,
                                              tex->noisebasis);

  if (texres->nor != NULL) {
    float offs = tex->nabla / tex->noisesize; /* also scaling of texvec */

    /* calculate bumpnormal */
    texres->nor[0] = tex->ns_outscale * mgravefunc(texvec[0] + offs,
                                                   texvec[1],
                                                   texvec[2],
                                                   tex->mg_H,
                                                   tex->mg_lacunarity,
                                                   tex->mg_octaves,
                                                   tex->noisebasis);
    texres->nor[1] = tex->ns_outscale * mgravefunc(texvec[0],
                                                   texvec[1] + offs,
                                                   texvec[2],
                                                   tex->mg_H,
                                                   tex->mg_lacunarity,
                                                   tex->mg_octaves,
                                                   tex->noisebasis);
    texres->nor[2] = tex->ns_outscale * mgravefunc(texvec[0],
                                                   texvec[1],
                                                   texvec[2] + offs,
                                                   tex->mg_H,
                                                   tex->mg_lacunarity,
                                                   tex->mg_octaves,
                                                   tex->noisebasis);

    tex_normal_derivate(tex, texres);
    rv |= TEX_NOR;
  }

  BRICONT;

  return rv;
}

static int mg_ridgedOrHybridMFTex(const Tex *tex, const float texvec[3], TexResult *texres)
{
  int rv = TEX_INT;
  float (*mgravefunc)(float, float, float, float, float, float, float, float, int);

  if (tex->stype == TEX_RIDGEDMF) {
    mgravefunc = mg_RidgedMultiFractal;
  }
  else {
    mgravefunc = mg_HybridMultiFractal;
  }

  texres->tin = tex->ns_outscale * mgravefunc(texvec[0],
                                              texvec[1],
                                              texvec[2],
                                              tex->mg_H,
                                              tex->mg_lacunarity,
                                              tex->mg_octaves,
                                              tex->mg_offset,
                                              tex->mg_gain,
                                              tex->noisebasis);

  if (texres->nor != NULL) {
    float offs = tex->nabla / tex->noisesize; /* also scaling of texvec */

    /* calculate bumpnormal */
    texres->nor[0] = tex->ns_outscale * mgravefunc(texvec[0] + offs,
                                                   texvec[1],
                                                   texvec[2],
                                                   tex->mg_H,
                                                   tex->mg_lacunarity,
                                                   tex->mg_octaves,
                                                   tex->mg_offset,
                                                   tex->mg_gain,
                                                   tex->noisebasis);
    texres->nor[1] = tex->ns_outscale * mgravefunc(texvec[0],
                                                   texvec[1] + offs,
                                                   texvec[2],
                                                   tex->mg_H,
                                                   tex->mg_lacunarity,
                                                   tex->mg_octaves,
                                                   tex->mg_offset,
                                                   tex->mg_gain,
                                                   tex->noisebasis);
    texres->nor[2] = tex->ns_outscale * mgravefunc(texvec[0],
                                                   texvec[1],
                                                   texvec[2] + offs,
                                                   tex->mg_H,
                                                   tex->mg_lacunarity,
                                                   tex->mg_octaves,
                                                   tex->mg_offset,
                                                   tex->mg_gain,
                                                   tex->noisebasis);

    tex_normal_derivate(tex, texres);
    rv |= TEX_NOR;
  }

  BRICONT;

  return rv;
}

static int mg_HTerrainTex(const Tex *tex, const float texvec[3], TexResult *texres)
{
  int rv = TEX_INT;

  texres->tin = tex->ns_outscale * mg_HeteroTerrain(texvec[0],
                                                    texvec[1],
                                                    texvec[2],
                                                    tex->mg_H,
                                                    tex->mg_lacunarity,
                                                    tex->mg_octaves,
                                                    tex->mg_offset,
                                                    tex->noisebasis);

  if (texres->nor != NULL) {
    float offs = tex->nabla / tex->noisesize; /* also scaling of texvec */

    /* calculate bumpnormal */
    texres->nor[0] = tex->ns_outscale * mg_HeteroTerrain(texvec[0] + offs,
                                                         texvec[1],
                                                         texvec[2],
                                                         tex->mg_H,
                                                         tex->mg_lacunarity,
                                                         tex->mg_octaves,
                                                         tex->mg_offset,
                                                         tex->noisebasis);
    texres->nor[1] = tex->ns_outscale * mg_HeteroTerrain(texvec[0],
                                                         texvec[1] + offs,
                                                         texvec[2],
                                                         tex->mg_H,
                                                         tex->mg_lacunarity,
                                                         tex->mg_octaves,
                                                         tex->mg_offset,
                                                         tex->noisebasis);
    texres->nor[2] = tex->ns_outscale * mg_HeteroTerrain(texvec[0],
                                                         texvec[1],
                                                         texvec[2] + offs,
                                                         tex->mg_H,
                                                         tex->mg_lacunarity,
                                                         tex->mg_octaves,
                                                         tex->mg_offset,
                                                         tex->noisebasis);

    tex_normal_derivate(tex, texres);
    rv |= TEX_NOR;
  }

  BRICONT;

  return rv;
}

static int mg_distNoiseTex(const Tex *tex, const float texvec[3], TexResult *texres)
{
  int rv = TEX_INT;

  texres->tin = mg_VLNoise(
      texvec[0], texvec[1], texvec[2], tex->dist_amount, tex->noisebasis, tex->noisebasis2);

  if (texres->nor != NULL) {
    float offs = tex->nabla / tex->noisesize; /* also scaling of texvec */

    /* calculate bumpnormal */
    texres->nor[0] = mg_VLNoise(texvec[0] + offs,
                                texvec[1],
                                texvec[2],
                                tex->dist_amount,
                                tex->noisebasis,
                                tex->noisebasis2);
    texres->nor[1] = mg_VLNoise(texvec[0],
                                texvec[1] + offs,
                                texvec[2],
                                tex->dist_amount,
                                tex->noisebasis,
                                tex->noisebasis2);
    texres->nor[2] = mg_VLNoise(texvec[0],
                                texvec[1],
                                texvec[2] + offs,
                                tex->dist_amount,
                                tex->noisebasis,
                                tex->noisebasis2);

    tex_normal_derivate(tex, texres);
    rv |= TEX_NOR;
  }

  BRICONT;

  return rv;
}

/* ------------------------------------------------------------------------- */
/* newnoise: Voronoi texture type
 *
 * probably the slowest, especially with minkovsky, bumpmapping, could be done another way.
 */

static int voronoiTex(const Tex *tex, const float texvec[3], TexResult *texres)
{
  int rv = TEX_INT;
  float da[4], pa[12]; /* distance and point coordinate arrays of 4 nearest neighbors */
  float aw1 = fabsf(tex->vn_w1);
  float aw2 = fabsf(tex->vn_w2);
  float aw3 = fabsf(tex->vn_w3);
  float aw4 = fabsf(tex->vn_w4);
  float sc = (aw1 + aw2 + aw3 + aw4);
  if (sc != 0.f) {
    sc = tex->ns_outscale / sc;
  }

  voronoi(texvec[0], texvec[1], texvec[2], da, pa, tex->vn_mexp, tex->vn_distm);
  texres->tin = sc * fabsf(dot_v4v4(&tex->vn_w1, da));

  if (tex->vn_coltype) {
    float ca[3]; /* cell color */
    cellNoiseV(pa[0], pa[1], pa[2], ca);
    texres->tr = aw1 * ca[0];
    texres->tg = aw1 * ca[1];
    texres->tb = aw1 * ca[2];
    cellNoiseV(pa[3], pa[4], pa[5], ca);
    texres->tr += aw2 * ca[0];
    texres->tg += aw2 * ca[1];
    texres->tb += aw2 * ca[2];
    cellNoiseV(pa[6], pa[7], pa[8], ca);
    texres->tr += aw3 * ca[0];
    texres->tg += aw3 * ca[1];
    texres->tb += aw3 * ca[2];
    cellNoiseV(pa[9], pa[10], pa[11], ca);
    texres->tr += aw4 * ca[0];
    texres->tg += aw4 * ca[1];
    texres->tb += aw4 * ca[2];
    if (tex->vn_coltype >= 2) {
      float t1 = (da[1] - da[0]) * 10;
      if (t1 > 1) {
        t1 = 1;
      }
      if (tex->vn_coltype == 3) {
        t1 *= texres->tin;
      }
      else {
        t1 *= sc;
      }
      texres->tr *= t1;
      texres->tg *= t1;
      texres->tb *= t1;
    }
    else {
      texres->tr *= sc;
      texres->tg *= sc;
      texres->tb *= sc;
    }
  }

  if (texres->nor != NULL) {
    float offs = tex->nabla / tex->noisesize; /* also scaling of texvec */

    /* calculate bumpnormal */
    voronoi(texvec[0] + offs, texvec[1], texvec[2], da, pa, tex->vn_mexp, tex->vn_distm);
    texres->nor[0] = sc * fabsf(dot_v4v4(&tex->vn_w1, da));
    voronoi(texvec[0], texvec[1] + offs, texvec[2], da, pa, tex->vn_mexp, tex->vn_distm);
    texres->nor[1] = sc * fabsf(dot_v4v4(&tex->vn_w1, da));
    voronoi(texvec[0], texvec[1], texvec[2] + offs, da, pa, tex->vn_mexp, tex->vn_distm);
    texres->nor[2] = sc * fabsf(dot_v4v4(&tex->vn_w1, da));

    tex_normal_derivate(tex, texres);
    rv |= TEX_NOR;
  }

  if (tex->vn_coltype) {
    BRICONTRGB;
    texres->ta = 1.0;
    return (rv | TEX_RGB);
  }

  BRICONT;

  return rv;
}

/* ------------------------------------------------------------------------- */

static int texnoise(const Tex *tex, TexResult *texres, int thread)
{
  float div = 3.0;
  int val, ran, loop, shift = 29;

  ran = BLI_rng_thread_rand(random_tex_array, thread);

  loop = tex->noisedepth;

  /* start from top bits since they have more variance */
  val = ((ran >> shift) & 3);

  while (loop--) {
    shift -= 2;
    val *= ((ran >> shift) & 3);
    div *= 3.0f;
  }

  texres->tin = ((float)val) / div;

  BRICONT;
  return TEX_INT;
}

/* ------------------------------------------------------------------------- */

static int cubemap_glob(const float n[3], float x, float y, float z, float *adr1, float *adr2)
{
  float x1, y1, z1, nor[3];
  int ret;

  if (n == NULL) {
    nor[0] = x;
    nor[1] = y;
    nor[2] = z; /* use local render coord */
  }
  else {
    copy_v3_v3(nor, n);
  }

  x1 = fabsf(nor[0]);
  y1 = fabsf(nor[1]);
  z1 = fabsf(nor[2]);

  if (z1 >= x1 && z1 >= y1) {
    *adr1 = (x + 1.0f) / 2.0f;
    *adr2 = (y + 1.0f) / 2.0f;
    ret = 0;
  }
  else if (y1 >= x1 && y1 >= z1) {
    *adr1 = (x + 1.0f) / 2.0f;
    *adr2 = (z + 1.0f) / 2.0f;
    ret = 1;
  }
  else {
    *adr1 = (y + 1.0f) / 2.0f;
    *adr2 = (z + 1.0f) / 2.0f;
    ret = 2;
  }
  return ret;
}

/* ------------------------------------------------------------------------- */

static void do_2d_mapping(
    const MTex *mtex, float texvec[3], const float n[3], float dxt[3], float dyt[3])
{
  Tex *tex;
  float fx, fy, fac1, area[8];
  int ok, proj, areaflag = 0, wrap;

  /* mtex variables localized, only cubemap doesn't cooperate yet... */
  wrap = mtex->mapping;
  tex = mtex->tex;

  if (!(dxt && dyt)) {

    if (wrap == MTEX_FLAT) {
      fx = (texvec[0] + 1.0f) / 2.0f;
      fy = (texvec[1] + 1.0f) / 2.0f;
    }
    else if (wrap == MTEX_TUBE) {
      map_to_tube(&fx, &fy, texvec[0], texvec[1], texvec[2]);
    }
    else if (wrap == MTEX_SPHERE) {
      map_to_sphere(&fx, &fy, texvec[0], texvec[1], texvec[2]);
    }
    else {
      cubemap_glob(n, texvec[0], texvec[1], texvec[2], &fx, &fy);
    }

    /* repeat */
    if (tex->extend == TEX_REPEAT) {
      if (tex->xrepeat > 1) {
        float origf = fx *= tex->xrepeat;

        if (fx > 1.0f) {
          fx -= (int)(fx);
        }
        else if (fx < 0.0f) {
          fx += 1 - (int)(fx);
        }

        if (tex->flag & TEX_REPEAT_XMIR) {
          int orig = (int)floor(origf);
          if (orig & 1) {
            fx = 1.0f - fx;
          }
        }
      }
      if (tex->yrepeat > 1) {
        float origf = fy *= tex->yrepeat;

        if (fy > 1.0f) {
          fy -= (int)(fy);
        }
        else if (fy < 0.0f) {
          fy += 1 - (int)(fy);
        }

        if (tex->flag & TEX_REPEAT_YMIR) {
          int orig = (int)floor(origf);
          if (orig & 1) {
            fy = 1.0f - fy;
          }
        }
      }
    }
    /* crop */
    if (tex->cropxmin != 0.0f || tex->cropxmax != 1.0f) {
      fac1 = tex->cropxmax - tex->cropxmin;
      fx = tex->cropxmin + fx * fac1;
    }
    if (tex->cropymin != 0.0f || tex->cropymax != 1.0f) {
      fac1 = tex->cropymax - tex->cropymin;
      fy = tex->cropymin + fy * fac1;
    }

    texvec[0] = fx;
    texvec[1] = fy;
  }
  else {

    if (wrap == MTEX_FLAT) {
      fx = (texvec[0] + 1.0f) / 2.0f;
      fy = (texvec[1] + 1.0f) / 2.0f;
      dxt[0] /= 2.0f;
      dxt[1] /= 2.0f;
      dxt[2] /= 2.0f;
      dyt[0] /= 2.0f;
      dyt[1] /= 2.0f;
      dyt[2] /= 2.0f;
    }
    else if (ELEM(wrap, MTEX_TUBE, MTEX_SPHERE)) {
      /* exception: the seam behind (y<0.0) */
      ok = 1;
      if (texvec[1] <= 0.0f) {
        fx = texvec[0] + dxt[0];
        fy = texvec[0] + dyt[0];
        if (fx >= 0.0f && fy >= 0.0f && texvec[0] >= 0.0f) {
          /* pass */
        }
        else if (fx <= 0.0f && fy <= 0.0f && texvec[0] <= 0.0f) {
          /* pass */
        }
        else {
          ok = 0;
        }
      }

      if (ok) {
        if (wrap == MTEX_TUBE) {
          map_to_tube(area, area + 1, texvec[0], texvec[1], texvec[2]);
          map_to_tube(
              area + 2, area + 3, texvec[0] + dxt[0], texvec[1] + dxt[1], texvec[2] + dxt[2]);
          map_to_tube(
              area + 4, area + 5, texvec[0] + dyt[0], texvec[1] + dyt[1], texvec[2] + dyt[2]);
        }
        else {
          map_to_sphere(area, area + 1, texvec[0], texvec[1], texvec[2]);
          map_to_sphere(
              area + 2, area + 3, texvec[0] + dxt[0], texvec[1] + dxt[1], texvec[2] + dxt[2]);
          map_to_sphere(
              area + 4, area + 5, texvec[0] + dyt[0], texvec[1] + dyt[1], texvec[2] + dyt[2]);
        }
        areaflag = 1;
      }
      else {
        if (wrap == MTEX_TUBE) {
          map_to_tube(&fx, &fy, texvec[0], texvec[1], texvec[2]);
        }
        else {
          map_to_sphere(&fx, &fy, texvec[0], texvec[1], texvec[2]);
        }
        dxt[0] /= 2.0f;
        dxt[1] /= 2.0f;
        dyt[0] /= 2.0f;
        dyt[1] /= 2.0f;
      }
    }
    else {

      proj = cubemap_glob(n, texvec[0], texvec[1], texvec[2], &fx, &fy);

      if (proj == 1) {
        SWAP(float, dxt[1], dxt[2]);
        SWAP(float, dyt[1], dyt[2]);
      }
      else if (proj == 2) {
        float f1 = dxt[0], f2 = dyt[0];
        dxt[0] = dxt[1];
        dyt[0] = dyt[1];
        dxt[1] = dxt[2];
        dyt[1] = dyt[2];
        dxt[2] = f1;
        dyt[2] = f2;
      }

      dxt[0] *= 0.5f;
      dxt[1] *= 0.5f;
      dxt[2] *= 0.5f;

      dyt[0] *= 0.5f;
      dyt[1] *= 0.5f;
      dyt[2] *= 0.5f;
    }

    /* if area, then reacalculate dxt[] and dyt[] */
    if (areaflag) {
      fx = area[0];
      fy = area[1];
      dxt[0] = area[2] - fx;
      dxt[1] = area[3] - fy;
      dyt[0] = area[4] - fx;
      dyt[1] = area[5] - fy;
    }

    /* repeat */
    if (tex->extend == TEX_REPEAT) {
      float max = 1.0f;
      if (tex->xrepeat > 1) {
        float origf = fx *= tex->xrepeat;

        /* TXF: omit mirror here, see comments in do_material_tex() after do_2d_mapping() call */
        if (tex->texfilter == TXF_BOX) {
          if (fx > 1.0f) {
            fx -= (int)(fx);
          }
          else if (fx < 0.0f) {
            fx += 1 - (int)(fx);
          }

          if (tex->flag & TEX_REPEAT_XMIR) {
            int orig = (int)floor(origf);
            if (orig & 1) {
              fx = 1.0f - fx;
            }
          }
        }

        max = tex->xrepeat;

        dxt[0] *= tex->xrepeat;
        dyt[0] *= tex->xrepeat;
      }
      if (tex->yrepeat > 1) {
        float origf = fy *= tex->yrepeat;

        /* TXF: omit mirror here, see comments in do_material_tex() after do_2d_mapping() call */
        if (tex->texfilter == TXF_BOX) {
          if (fy > 1.0f) {
            fy -= (int)(fy);
          }
          else if (fy < 0.0f) {
            fy += 1 - (int)(fy);
          }

          if (tex->flag & TEX_REPEAT_YMIR) {
            int orig = (int)floor(origf);
            if (orig & 1) {
              fy = 1.0f - fy;
            }
          }
        }

        if (max < tex->yrepeat) {
          max = tex->yrepeat;
        }

        dxt[1] *= tex->yrepeat;
        dyt[1] *= tex->yrepeat;
      }
      if (max != 1.0f) {
        dxt[2] *= max;
        dyt[2] *= max;
      }
    }
    /* crop */
    if (tex->cropxmin != 0.0f || tex->cropxmax != 1.0f) {
      fac1 = tex->cropxmax - tex->cropxmin;
      fx = tex->cropxmin + fx * fac1;
      dxt[0] *= fac1;
      dyt[0] *= fac1;
    }
    if (tex->cropymin != 0.0f || tex->cropymax != 1.0f) {
      fac1 = tex->cropymax - tex->cropymin;
      fy = tex->cropymin + fy * fac1;
      dxt[1] *= fac1;
      dyt[1] *= fac1;
    }

    texvec[0] = fx;
    texvec[1] = fy;
  }
}

/* ************************************** */

static int multitex(Tex *tex,
                    float texvec[3],
                    float dxt[3],
                    float dyt[3],
                    int osatex,
                    TexResult *texres,
                    const short thread,
                    const short which_output,
                    struct ImagePool *pool,
                    const bool skip_load_image,
                    const bool texnode_preview,
                    const bool use_nodes)
{
  float tmpvec[3];
  int retval = 0; /* return value, int:0, col:1, nor:2, everything:3 */

  texres->talpha = false; /* is set when image texture returns alpha (considered premul) */

  if (use_nodes && tex->use_nodes && tex->nodetree) {
    const float cfra = 1.0f; /* This was only set for Blender Internal render before. */
    retval = ntreeTexExecTree(tex->nodetree,
                              texres,
                              texvec,
                              dxt,
                              dyt,
                              osatex,
                              thread,
                              tex,
                              which_output,
                              cfra,
                              texnode_preview,
                              NULL);
  }
  else {
    switch (tex->type) {
      case 0:
        texres->tin = 0.0f;
        return 0;
      case TEX_CLOUDS:
        retval = clouds(tex, texvec, texres);
        break;
      case TEX_WOOD:
        retval = wood(tex, texvec, texres);
        break;
      case TEX_MARBLE:
        retval = marble(tex, texvec, texres);
        break;
      case TEX_MAGIC:
        retval = magic(tex, texvec, texres);
        break;
      case TEX_BLEND:
        retval = blend(tex, texvec, texres);
        break;
      case TEX_STUCCI:
        retval = stucci(tex, texvec, texres);
        break;
      case TEX_NOISE:
        retval = texnoise(tex, texres, thread);
        break;
      case TEX_IMAGE:
        if (osatex) {
          retval = imagewraposa(
              tex, tex->ima, NULL, texvec, dxt, dyt, texres, pool, skip_load_image);
        }
        else {
          retval = imagewrap(tex, tex->ima, texvec, texres, pool, skip_load_image);
        }
        if (tex->ima) {
          BKE_image_tag_time(tex->ima);
        }
        break;
      case TEX_MUSGRAVE:
        /* newnoise: musgrave types */

        /* ton: added this, for Blender convention reason.
         * artificer: added the use of tmpvec to avoid scaling texvec
         */
        copy_v3_v3(tmpvec, texvec);
        mul_v3_fl(tmpvec, 1.0f / tex->noisesize);

        switch (tex->stype) {
          case TEX_MFRACTAL:
          case TEX_FBM:
            retval = mg_mFractalOrfBmTex(tex, tmpvec, texres);
            break;
          case TEX_RIDGEDMF:
          case TEX_HYBRIDMF:
            retval = mg_ridgedOrHybridMFTex(tex, tmpvec, texres);
            break;
          case TEX_HTERRAIN:
            retval = mg_HTerrainTex(tex, tmpvec, texres);
            break;
        }
        break;
      /* newnoise: voronoi type */
      case TEX_VORONOI:
        /* ton: added this, for Blender convention reason.
         * artificer: added the use of tmpvec to avoid scaling texvec
         */
        copy_v3_v3(tmpvec, texvec);
        mul_v3_fl(tmpvec, 1.0f / tex->noisesize);

        retval = voronoiTex(tex, tmpvec, texres);
        break;
      case TEX_DISTNOISE:
        /* ton: added this, for Blender convention reason.
         * artificer: added the use of tmpvec to avoid scaling texvec
         */
        copy_v3_v3(tmpvec, texvec);
        mul_v3_fl(tmpvec, 1.0f / tex->noisesize);

        retval = mg_distNoiseTex(tex, tmpvec, texres);
        break;
    }
  }

  if (tex->flag & TEX_COLORBAND) {
    float col[4];
    if (BKE_colorband_evaluate(tex->coba, texres->tin, col)) {
      texres->talpha = true;
      texres->tr = col[0];
      texres->tg = col[1];
      texres->tb = col[2];
      texres->ta = col[3];
      retval |= TEX_RGB;
    }
  }
  return retval;
}

static int multitex_nodes_intern(Tex *tex,
                                 float texvec[3],
                                 float dxt[3],
                                 float dyt[3],
                                 int osatex,
                                 TexResult *texres,
                                 const short thread,
                                 short which_output,
                                 MTex *mtex,
                                 struct ImagePool *pool,
                                 const bool scene_color_manage,
                                 const bool skip_load_image,
                                 const bool texnode_preview,
                                 const bool use_nodes)
{
  if (tex == NULL) {
    memset(texres, 0, sizeof(TexResult));
    return 0;
  }

  if (mtex) {
    which_output = mtex->which_output;
  }

  if (tex->type == TEX_IMAGE) {
    int rgbnor;

    if (mtex) {
      /* we have mtex, use it for 2d mapping images only */
      do_2d_mapping(mtex, texvec, NULL, dxt, dyt);
      rgbnor = multitex(tex,
                        texvec,
                        dxt,
                        dyt,
                        osatex,
                        texres,
                        thread,
                        which_output,
                        pool,
                        skip_load_image,
                        texnode_preview,
                        use_nodes);

      if (mtex->mapto & MAP_COL) {
        ImBuf *ibuf = BKE_image_pool_acquire_ibuf(tex->ima, &tex->iuser, pool);

        /* don't linearize float buffers, assumed to be linear */
        if (ibuf != NULL && ibuf->rect_float == NULL && (rgbnor & TEX_RGB) && scene_color_manage) {
          IMB_colormanagement_colorspace_to_scene_linear_v3(&texres->tr, ibuf->rect_colorspace);
        }

        BKE_image_pool_release_ibuf(tex->ima, ibuf, pool);
      }
    }
    else {
      /* we don't have mtex, do default flat 2d projection */
      MTex localmtex;
      float texvec_l[3], dxt_l[3], dyt_l[3];

      localmtex.mapping = MTEX_FLAT;
      localmtex.tex = tex;
      localmtex.object = NULL;
      localmtex.texco = TEXCO_ORCO;

      copy_v3_v3(texvec_l, texvec);
      if (dxt && dyt) {
        copy_v3_v3(dxt_l, dxt);
        copy_v3_v3(dyt_l, dyt);
      }
      else {
        zero_v3(dxt_l);
        zero_v3(dyt_l);
      }

      do_2d_mapping(&localmtex, texvec_l, NULL, dxt_l, dyt_l);
      rgbnor = multitex(tex,
                        texvec_l,
                        dxt_l,
                        dyt_l,
                        osatex,
                        texres,
                        thread,
                        which_output,
                        pool,
                        skip_load_image,
                        texnode_preview,
                        use_nodes);

      {
        ImBuf *ibuf = BKE_image_pool_acquire_ibuf(tex->ima, &tex->iuser, pool);

        /* don't linearize float buffers, assumed to be linear */
        if (ibuf != NULL && ibuf->rect_float == NULL && (rgbnor & TEX_RGB) && scene_color_manage) {
          IMB_colormanagement_colorspace_to_scene_linear_v3(&texres->tr, ibuf->rect_colorspace);
        }

        BKE_image_pool_release_ibuf(tex->ima, ibuf, pool);
      }
    }

    return rgbnor;
  }
  else {
    return multitex(tex,
                    texvec,
                    dxt,
                    dyt,
                    osatex,
                    texres,
                    thread,
                    which_output,
                    pool,
                    skip_load_image,
                    texnode_preview,
                    use_nodes);
  }
}

/* this is called from the shader and texture nodes
 * Use it from render pipeline only!
 */
int multitex_nodes(Tex *tex,
                   float texvec[3],
                   float dxt[3],
                   float dyt[3],
                   int osatex,
                   TexResult *texres,
                   const short thread,
                   short which_output,
                   MTex *mtex,
                   struct ImagePool *pool)
{
  return multitex_nodes_intern(tex,
                               texvec,
                               dxt,
                               dyt,
                               osatex,
                               texres,
                               thread,
                               which_output,
                               mtex,
                               pool,
                               true,
                               false,
                               false,
                               true);
}

/**
 * \warning if the texres's values are not declared zero,
 * check the return value to be sure the color values are set before using the r/g/b values,
 * otherwise you may use uninitialized values - Campbell
 *
 * Use it for stuff which is out of render pipeline.
 */
int multitex_ext(Tex *tex,
                 float texvec[3],
                 float dxt[3],
                 float dyt[3],
                 int osatex,
                 TexResult *texres,
                 const short thread,
                 struct ImagePool *pool,
                 bool scene_color_manage,
                 const bool skip_load_image)
{
  return multitex_nodes_intern(tex,
                               texvec,
                               dxt,
                               dyt,
                               osatex,
                               texres,
                               thread,
                               0,
                               NULL,
                               pool,
                               scene_color_manage,
                               skip_load_image,
                               false,
                               true);
}

/* extern-tex doesn't support nodes (ntreeBeginExec() can't be called when rendering is going on)\
 *
 * Use it for stuff which is out of render pipeline.
 */
int multitex_ext_safe(Tex *tex,
                      float texvec[3],
                      TexResult *texres,
                      struct ImagePool *pool,
                      bool scene_color_manage,
                      const bool skip_load_image)
{
  return multitex_nodes_intern(tex,
                               texvec,
                               NULL,
                               NULL,
                               0,
                               texres,
                               0,
                               0,
                               NULL,
                               pool,
                               scene_color_manage,
                               skip_load_image,
                               false,
                               false);
}

/* ------------------------------------------------------------------------- */

/* in = destination, tex = texture, out = previous color */
/* fact = texture strength, facg = button strength value */
void texture_rgb_blend(
    float in[3], const float tex[3], const float out[3], float fact, float facg, int blendtype)
{
  float facm;

  switch (blendtype) {
    case MTEX_BLEND:
      fact *= facg;
      facm = 1.0f - fact;

      in[0] = (fact * tex[0] + facm * out[0]);
      in[1] = (fact * tex[1] + facm * out[1]);
      in[2] = (fact * tex[2] + facm * out[2]);
      break;

    case MTEX_MUL:
      fact *= facg;
      facm = 1.0f - fact;
      in[0] = (facm + fact * tex[0]) * out[0];
      in[1] = (facm + fact * tex[1]) * out[1];
      in[2] = (facm + fact * tex[2]) * out[2];
      break;

    case MTEX_SCREEN:
      fact *= facg;
      facm = 1.0f - fact;
      in[0] = 1.0f - (facm + fact * (1.0f - tex[0])) * (1.0f - out[0]);
      in[1] = 1.0f - (facm + fact * (1.0f - tex[1])) * (1.0f - out[1]);
      in[2] = 1.0f - (facm + fact * (1.0f - tex[2])) * (1.0f - out[2]);
      break;

    case MTEX_OVERLAY:
      fact *= facg;
      facm = 1.0f - fact;

      if (out[0] < 0.5f) {
        in[0] = out[0] * (facm + 2.0f * fact * tex[0]);
      }
      else {
        in[0] = 1.0f - (facm + 2.0f * fact * (1.0f - tex[0])) * (1.0f - out[0]);
      }
      if (out[1] < 0.5f) {
        in[1] = out[1] * (facm + 2.0f * fact * tex[1]);
      }
      else {
        in[1] = 1.0f - (facm + 2.0f * fact * (1.0f - tex[1])) * (1.0f - out[1]);
      }
      if (out[2] < 0.5f) {
        in[2] = out[2] * (facm + 2.0f * fact * tex[2]);
      }
      else {
        in[2] = 1.0f - (facm + 2.0f * fact * (1.0f - tex[2])) * (1.0f - out[2]);
      }
      break;

    case MTEX_SUB:
      fact = -fact;
      ATTR_FALLTHROUGH;
    case MTEX_ADD:
      fact *= facg;
      in[0] = (fact * tex[0] + out[0]);
      in[1] = (fact * tex[1] + out[1]);
      in[2] = (fact * tex[2] + out[2]);
      break;

    case MTEX_DIV:
      fact *= facg;
      facm = 1.0f - fact;

      if (tex[0] != 0.0f) {
        in[0] = facm * out[0] + fact * out[0] / tex[0];
      }
      if (tex[1] != 0.0f) {
        in[1] = facm * out[1] + fact * out[1] / tex[1];
      }
      if (tex[2] != 0.0f) {
        in[2] = facm * out[2] + fact * out[2] / tex[2];
      }

      break;

    case MTEX_DIFF:
      fact *= facg;
      facm = 1.0f - fact;
      in[0] = facm * out[0] + fact * fabsf(tex[0] - out[0]);
      in[1] = facm * out[1] + fact * fabsf(tex[1] - out[1]);
      in[2] = facm * out[2] + fact * fabsf(tex[2] - out[2]);
      break;

    case MTEX_DARK:
      fact *= facg;
      facm = 1.0f - fact;

      in[0] = min_ff(out[0], tex[0]) * fact + out[0] * facm;
      in[1] = min_ff(out[1], tex[1]) * fact + out[1] * facm;
      in[2] = min_ff(out[2], tex[2]) * fact + out[2] * facm;
      break;

    case MTEX_LIGHT:
      fact *= facg;

      in[0] = max_ff(fact * tex[0], out[0]);
      in[1] = max_ff(fact * tex[1], out[1]);
      in[2] = max_ff(fact * tex[2], out[2]);
      break;

    case MTEX_BLEND_HUE:
      fact *= facg;
      copy_v3_v3(in, out);
      ramp_blend(MA_RAMP_HUE, in, fact, tex);
      break;
    case MTEX_BLEND_SAT:
      fact *= facg;
      copy_v3_v3(in, out);
      ramp_blend(MA_RAMP_SAT, in, fact, tex);
      break;
    case MTEX_BLEND_VAL:
      fact *= facg;
      copy_v3_v3(in, out);
      ramp_blend(MA_RAMP_VAL, in, fact, tex);
      break;
    case MTEX_BLEND_COLOR:
      fact *= facg;
      copy_v3_v3(in, out);
      ramp_blend(MA_RAMP_COLOR, in, fact, tex);
      break;
    case MTEX_SOFT_LIGHT:
      fact *= facg;
      copy_v3_v3(in, out);
      ramp_blend(MA_RAMP_SOFT, in, fact, tex);
      break;
    case MTEX_LIN_LIGHT:
      fact *= facg;
      copy_v3_v3(in, out);
      ramp_blend(MA_RAMP_LINEAR, in, fact, tex);
      break;
  }
}

float texture_value_blend(float tex, float out, float fact, float facg, int blendtype)
{
  float in = 0.0, facm, col, scf;
  int flip = (facg < 0.0f);

  facg = fabsf(facg);

  fact *= facg;
  facm = 1.0f - fact;
  if (flip) {
    SWAP(float, fact, facm);
  }

  switch (blendtype) {
    case MTEX_BLEND:
      in = fact * tex + facm * out;
      break;

    case MTEX_MUL:
      facm = 1.0f - facg;
      in = (facm + fact * tex) * out;
      break;

    case MTEX_SCREEN:
      facm = 1.0f - facg;
      in = 1.0f - (facm + fact * (1.0f - tex)) * (1.0f - out);
      break;

    case MTEX_OVERLAY:
      facm = 1.0f - facg;
      if (out < 0.5f) {
        in = out * (facm + 2.0f * fact * tex);
      }
      else {
        in = 1.0f - (facm + 2.0f * fact * (1.0f - tex)) * (1.0f - out);
      }
      break;

    case MTEX_SUB:
      fact = -fact;
      ATTR_FALLTHROUGH;
    case MTEX_ADD:
      in = fact * tex + out;
      break;

    case MTEX_DIV:
      if (tex != 0.0f) {
        in = facm * out + fact * out / tex;
      }
      break;

    case MTEX_DIFF:
      in = facm * out + fact * fabsf(tex - out);
      break;

    case MTEX_DARK:
      in = min_ff(out, tex) * fact + out * facm;
      break;

    case MTEX_LIGHT:
      col = fact * tex;
      if (col > out) {
        in = col;
      }
      else {
        in = out;
      }
      break;

    case MTEX_SOFT_LIGHT:
      scf = 1.0f - (1.0f - tex) * (1.0f - out);
      in = facm * out + fact * ((1.0f - out) * tex * out) + (out * scf);
      break;

    case MTEX_LIN_LIGHT:
      if (tex > 0.5f) {
        in = out + fact * (2.0f * (tex - 0.5f));
      }
      else {
        in = out + fact * (2.0f * tex - 1.0f);
      }
      break;
  }

  return in;
}

/* ------------------------------------------------------------------------- */

/**
 * \param pool: Thread pool, may be NULL.
 *
 * \return True if the texture has color, otherwise false.
 */
bool RE_texture_evaluate(const MTex *mtex,
                         const float vec[3],
                         const int thread,
                         struct ImagePool *pool,
                         const bool skip_load_image,
                         const bool texnode_preview,
                         /* Return arguments. */
                         float *r_intensity,
                         float r_rgba[4])
{
  Tex *tex;
  TexResult texr;
  float dxt[3], dyt[3], texvec[3];
  int rgb;

  tex = mtex->tex;
  if (tex == NULL) {
    return 0;
  }
  texr.nor = NULL;

  /* placement */
  if (mtex->projx) {
    texvec[0] = mtex->size[0] * (vec[mtex->projx - 1] + mtex->ofs[0]);
  }
  else {
    texvec[0] = mtex->size[0] * (mtex->ofs[0]);
  }

  if (mtex->projy) {
    texvec[1] = mtex->size[1] * (vec[mtex->projy - 1] + mtex->ofs[1]);
  }
  else {
    texvec[1] = mtex->size[1] * (mtex->ofs[1]);
  }

  if (mtex->projz) {
    texvec[2] = mtex->size[2] * (vec[mtex->projz - 1] + mtex->ofs[2]);
  }
  else {
    texvec[2] = mtex->size[2] * (mtex->ofs[2]);
  }

  /* texture */
  if (tex->type == TEX_IMAGE) {
    do_2d_mapping(mtex, texvec, NULL, dxt, dyt);
  }

  rgb = multitex(tex,
                 texvec,
                 dxt,
                 dyt,
                 0,
                 &texr,
                 thread,
                 mtex->which_output,
                 pool,
                 skip_load_image,
                 texnode_preview,
                 true);

  if (rgb) {
    texr.tin = IMB_colormanagement_get_luminance(&texr.tr);
  }
  else {
    texr.tr = mtex->r;
    texr.tg = mtex->g;
    texr.tb = mtex->b;
  }

  *r_intensity = texr.tin;
  r_rgba[0] = texr.tr;
  r_rgba[1] = texr.tg;
  r_rgba[2] = texr.tb;
  r_rgba[3] = texr.ta;

  return (rgb != 0);
}
