/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "BLI_math_geom.h"
#include "BLI_noise.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_texture_types.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf_types.hh"

#include "BKE_colorband.hh"
#include "BKE_image.h"

#include "NOD_texture.h"

#include "MEM_guardedalloc.h"

#include "texture_common.h"

#include "RE_texture.h"

static RNG_THREAD_ARRAY *random_tex_array;

void RE_texture_rng_init()
{
  random_tex_array = BLI_rng_threaded_new();
}

void RE_texture_rng_exit()
{
  if (random_tex_array == nullptr) {
    return;
  }
  BLI_rng_threaded_free(random_tex_array);
  random_tex_array = nullptr;
}

/* ------------------------------------------------------------------------- */

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

  if (tex->stype == TEX_LIN) { /* Linear. */
    texres->tin = (1.0f + x) / 2.0f;
  }
  else if (tex->stype == TEX_QUAD) { /* Quadratic. */
    texres->tin = (1.0f + x) / 2.0f;
    if (texres->tin < 0.0f) {
      texres->tin = 0.0f;
    }
    else {
      texres->tin *= texres->tin;
    }
  }
  else if (tex->stype == TEX_EASE) { /* Ease. */
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
  else if (tex->stype == TEX_DIAG) { /* Diagonal. */
    texres->tin = (2.0f + x + y) / 4.0f;
  }
  else if (tex->stype == TEX_RAD) { /* Radial. */
    texres->tin = (atan2f(y, x) / float(2 * M_PI) + 0.5f);
  }
  else { /* sphere TEX_SPHERE */
    texres->tin = 1.0f - sqrtf(x * x + y * y + texvec[2] * texvec[2]);
    if (texres->tin < 0.0f) {
      texres->tin = 0.0f;
    }
    if (tex->stype == TEX_HALO) {
      texres->tin *= texres->tin; /* Halo. */
    }
  }

  BRICONT;

  return TEX_INT;
}

/* ------------------------------------------------------------------------- */
/* ************************************************************************* */

/* newnoise: all noise-based types now have different noise-bases to choose from. */

static int clouds(const Tex *tex, const float texvec[3], TexResult *texres)
{
  int rv = TEX_INT;

  texres->tin = BLI_noise_generic_turbulence(tex->noisesize,
                                             texvec[0],
                                             texvec[1],
                                             texvec[2],
                                             tex->noisedepth,
                                             (tex->noisetype != TEX_NOISESOFT),
                                             tex->noisebasis);

  if (tex->stype == TEX_COLOR) {
    texres->trgba[0] = texres->tin;
    texres->trgba[1] = BLI_noise_generic_turbulence(tex->noisesize,
                                                    texvec[1],
                                                    texvec[0],
                                                    texvec[2],
                                                    tex->noisedepth,
                                                    (tex->noisetype != TEX_NOISESOFT),
                                                    tex->noisebasis);
    texres->trgba[2] = BLI_noise_generic_turbulence(tex->noisesize,
                                                    texvec[1],
                                                    texvec[2],
                                                    texvec[0],
                                                    tex->noisedepth,
                                                    (tex->noisetype != TEX_NOISESOFT),
                                                    tex->noisebasis);
    BRICONTRGB;
    texres->trgba[3] = 1.0;
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

  int n = int(a / b);
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
  /* wave form: TEX_SIN=0, TEX_SAW=1, TEX_TRI=2. */
  short wf = tex->noisebasis2;
  /* wood type: TEX_BAND=0, TEX_RING=1, TEX_BANDNOISE=2, TEX_RINGNOISE=3. */
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
         BLI_noise_generic_noise(
             tex->noisesize, x, y, z, (tex->noisetype != TEX_NOISESOFT), tex->noisebasis);
    wi = waveform[wf]((x + y + z) * 10.0f + wi);
  }
  else if (wt == TEX_RINGNOISE) {
    wi = tex->turbul *
         BLI_noise_generic_noise(
             tex->noisesize, x, y, z, (tex->noisetype != TEX_NOISESOFT), tex->noisebasis);
    wi = waveform[wf](sqrtf(x * x + y * y + z * z) * 20.0f + wi);
  }

  return wi;
}

static int wood(const Tex *tex, const float texvec[3], TexResult *texres)
{
  int rv = TEX_INT;

  texres->tin = wood_int(tex, texvec[0], texvec[1], texvec[2]);

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

  mi = n + tex->turbul * BLI_noise_generic_turbulence(tex->noisesize,
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
  texres->trgba[0] = 0.5f - x;
  texres->trgba[1] = 0.5f - y;
  texres->trgba[2] = 0.5f - z;

  texres->tin = (1.0f / 3.0f) * (texres->trgba[0] + texres->trgba[1] + texres->trgba[2]);

  BRICONTRGB;
  texres->trgba[3] = 1.0f;

  return TEX_RGB;
}

/* ------------------------------------------------------------------------- */

/* newnoise: stucci also modified to use different noisebasis */
static int stucci(const Tex *tex, const float texvec[3], TexResult *texres)
{
  float b2, ofs;
  int retval = TEX_INT;

  b2 = BLI_noise_generic_noise(tex->noisesize,
                               texvec[0],
                               texvec[1],
                               texvec[2],
                               (tex->noisetype != TEX_NOISESOFT),
                               tex->noisebasis);

  ofs = tex->turbul / 200.0f;

  if (tex->stype) {
    ofs *= (b2 * b2);
  }

  texres->tin = BLI_noise_generic_noise(tex->noisesize,
                                        texvec[0],
                                        texvec[1],
                                        texvec[2] + ofs,
                                        (tex->noisetype != TEX_NOISESOFT),
                                        tex->noisebasis);

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
    mgravefunc = BLI_noise_mg_multi_fractal;
  }
  else {
    mgravefunc = BLI_noise_mg_fbm;
  }

  texres->tin = tex->ns_outscale * mgravefunc(texvec[0],
                                              texvec[1],
                                              texvec[2],
                                              tex->mg_H,
                                              tex->mg_lacunarity,
                                              tex->mg_octaves,
                                              tex->noisebasis);

  BRICONT;

  return rv;
}

static int mg_ridgedOrHybridMFTex(const Tex *tex, const float texvec[3], TexResult *texres)
{
  int rv = TEX_INT;
  float (*mgravefunc)(float, float, float, float, float, float, float, float, int);

  if (tex->stype == TEX_RIDGEDMF) {
    mgravefunc = BLI_noise_mg_ridged_multi_fractal;
  }
  else {
    mgravefunc = BLI_noise_mg_hybrid_multi_fractal;
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

  BRICONT;

  return rv;
}

static int mg_HTerrainTex(const Tex *tex, const float texvec[3], TexResult *texres)
{
  int rv = TEX_INT;

  texres->tin = tex->ns_outscale * BLI_noise_mg_hetero_terrain(texvec[0],
                                                               texvec[1],
                                                               texvec[2],
                                                               tex->mg_H,
                                                               tex->mg_lacunarity,
                                                               tex->mg_octaves,
                                                               tex->mg_offset,
                                                               tex->noisebasis);

  BRICONT;

  return rv;
}

static int mg_distNoiseTex(const Tex *tex, const float texvec[3], TexResult *texres)
{
  int rv = TEX_INT;

  texres->tin = BLI_noise_mg_variable_lacunarity(
      texvec[0], texvec[1], texvec[2], tex->dist_amount, tex->noisebasis, tex->noisebasis2);

  BRICONT;

  return rv;
}

/* ------------------------------------------------------------------------- */
/* newnoise: Voronoi texture type
 *
 * probably the slowest, especially with minkovsky, bump-mapping, could be done another way.
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
  if (sc != 0.0f) {
    sc = tex->ns_outscale / sc;
  }

  BLI_noise_voronoi(texvec[0], texvec[1], texvec[2], da, pa, tex->vn_mexp, tex->vn_distm);
  texres->tin = sc * fabsf(dot_v4v4(&tex->vn_w1, da));

  const bool is_color = ELEM(tex->vn_coltype, TEX_COL1, TEX_COL2, TEX_COL3);
  if (is_color) {
    float ca[3]; /* cell color */
    BLI_noise_cell_v3(pa[0], pa[1], pa[2], ca);
    texres->trgba[0] = aw1 * ca[0];
    texres->trgba[1] = aw1 * ca[1];
    texres->trgba[2] = aw1 * ca[2];
    BLI_noise_cell_v3(pa[3], pa[4], pa[5], ca);
    texres->trgba[0] += aw2 * ca[0];
    texres->trgba[1] += aw2 * ca[1];
    texres->trgba[2] += aw2 * ca[2];
    BLI_noise_cell_v3(pa[6], pa[7], pa[8], ca);
    texres->trgba[0] += aw3 * ca[0];
    texres->trgba[1] += aw3 * ca[1];
    texres->trgba[2] += aw3 * ca[2];
    BLI_noise_cell_v3(pa[9], pa[10], pa[11], ca);
    texres->trgba[0] += aw4 * ca[0];
    texres->trgba[1] += aw4 * ca[1];
    texres->trgba[2] += aw4 * ca[2];
    if (ELEM(tex->vn_coltype, TEX_COL2, TEX_COL3)) {
      float t1 = (da[1] - da[0]) * 10;
      if (t1 > 1) {
        t1 = 1;
      }
      if (tex->vn_coltype == TEX_COL3) {
        t1 *= texres->tin;
      }
      else {
        t1 *= sc;
      }
      texres->trgba[0] *= t1;
      texres->trgba[1] *= t1;
      texres->trgba[2] *= t1;
    }
    else {
      texres->trgba[0] *= sc;
      texres->trgba[1] *= sc;
      texres->trgba[2] *= sc;
    }
  }

  if (is_color) {
    BRICONTRGB;
    texres->trgba[3] = 1.0;
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

  texres->tin = float(val) / div;

  BRICONT;
  return TEX_INT;
}

/* ------------------------------------------------------------------------- */

static int cubemap_glob(const float n[3], float x, float y, float z, float *adr1, float *adr2)
{
  float x1, y1, z1, nor[3];
  int ret;

  if (n == nullptr) {
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

  /* #MTex variables localized, only cube-map doesn't cooperate yet. */
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
          fx -= int(fx);
        }
        else if (fx < 0.0f) {
          fx += 1 - int(fx);
        }

        if (tex->flag & TEX_REPEAT_XMIR) {
          int orig = int(floor(origf));
          if (orig & 1) {
            fx = 1.0f - fx;
          }
        }
      }
      if (tex->yrepeat > 1) {
        float origf = fy *= tex->yrepeat;

        if (fy > 1.0f) {
          fy -= int(fy);
        }
        else if (fy < 0.0f) {
          fy += 1 - int(fy);
        }

        if (tex->flag & TEX_REPEAT_YMIR) {
          int orig = int(floor(origf));
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
        std::swap(dxt[1], dxt[2]);
        std::swap(dyt[1], dyt[2]);
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

    /* If area, then recalculate `dxt[]` and `dyt[]` */
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
            fx -= int(fx);
          }
          else if (fx < 0.0f) {
            fx += 1 - int(fx);
          }

          if (tex->flag & TEX_REPEAT_XMIR) {
            int orig = int(floor(origf));
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
            fy -= int(fy);
          }
          else if (fy < 0.0f) {
            fy += 1 - int(fy);
          }

          if (tex->flag & TEX_REPEAT_YMIR) {
            int orig = int(floor(origf));
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
                    const float texvec[3],
                    float dxt[3],
                    float dyt[3],
                    int osatex,
                    TexResult *texres,
                    const short thread,
                    const short which_output,
                    ImagePool *pool,
                    const bool skip_load_image,
                    const bool texnode_preview,
                    const bool use_nodes)
{
  float tmpvec[3];
  int retval = 0; /* return value, TEX_INT or TEX_RGB. */

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
                              nullptr);
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
              tex, tex->ima, nullptr, texvec, dxt, dyt, texres, pool, skip_load_image);
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

        /* NOTE(@ton): added this, for Blender convention reason.
         * NOTE(@artificer): added the use of tmpvec to avoid scaling texvec. */
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
        /* NOTE(@ton): added this, for Blender convention reason.
         * NOTE(@artificer): added the use of tmpvec to avoid scaling texvec. */
        copy_v3_v3(tmpvec, texvec);
        mul_v3_fl(tmpvec, 1.0f / tex->noisesize);

        retval = voronoiTex(tex, tmpvec, texres);
        break;
      case TEX_DISTNOISE:
        /* NOTE(@ton): added this, for Blender convention reason.
         * NOTE(@artificer): added the use of tmpvec to avoid scaling texvec. */
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
      copy_v4_v4(texres->trgba, col);
      retval |= TEX_RGB;
    }
  }
  return retval;
}

static int multitex_nodes_intern(Tex *tex,
                                 const float texvec[3],
                                 float dxt[3],
                                 float dyt[3],
                                 int osatex,
                                 TexResult *texres,
                                 const short thread,
                                 short which_output,
                                 const MTex *mtex,
                                 ImagePool *pool,
                                 const bool scene_color_manage,
                                 const bool skip_load_image,
                                 const bool texnode_preview,
                                 const bool use_nodes)
{
  if (tex == nullptr) {
    memset(texres, 0, sizeof(TexResult));
    return 0;
  }

  if (mtex) {
    which_output = mtex->which_output;
  }

  if (tex->type == TEX_IMAGE) {
    int retval;

    if (mtex) {
      float texvec_l[3];
      copy_v3_v3(texvec_l, texvec);
      /* we have mtex, use it for 2d mapping images only */
      do_2d_mapping(mtex, texvec_l, nullptr, dxt, dyt);
      retval = multitex(tex,
                        texvec_l,
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
        if (ibuf != nullptr && ibuf->float_buffer.data == nullptr && (retval & TEX_RGB) &&
            scene_color_manage)
        {
          IMB_colormanagement_colorspace_to_scene_linear_v3(texres->trgba,
                                                            ibuf->byte_buffer.colorspace);
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
      localmtex.object = nullptr;
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

      do_2d_mapping(&localmtex, texvec_l, nullptr, dxt_l, dyt_l);
      retval = multitex(tex,
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
        if (ibuf != nullptr && ibuf->float_buffer.data == nullptr && (retval & TEX_RGB) &&
            scene_color_manage)
        {
          IMB_colormanagement_colorspace_to_scene_linear_v3(texres->trgba,
                                                            ibuf->byte_buffer.colorspace);
        }

        BKE_image_pool_release_ibuf(tex->ima, ibuf, pool);
      }
    }

    return retval;
  }

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

int multitex_nodes(Tex *tex,
                   const float texvec[3],
                   float dxt[3],
                   float dyt[3],
                   int osatex,
                   TexResult *texres,
                   const short thread,
                   short which_output,
                   const MTex *mtex,
                   ImagePool *pool)
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

int multitex_ext(Tex *tex,
                 const float texvec[3],
                 float dxt[3],
                 float dyt[3],
                 int osatex,
                 TexResult *texres,
                 const short thread,
                 ImagePool *pool,
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
                               nullptr,
                               pool,
                               scene_color_manage,
                               skip_load_image,
                               false,
                               true);
}

int multitex_ext_safe(Tex *tex,
                      const float texvec[3],
                      TexResult *texres,
                      ImagePool *pool,
                      bool scene_color_manage,
                      const bool skip_load_image)
{
  return multitex_nodes_intern(tex,
                               texvec,
                               nullptr,
                               nullptr,
                               0,
                               texres,
                               0,
                               0,
                               nullptr,
                               pool,
                               scene_color_manage,
                               skip_load_image,
                               false,
                               false);
}

/* ------------------------------------------------------------------------- */

float texture_value_blend(float tex, float out, float fact, float facg, int blendtype)
{
  float in = 0.0, facm, col, scf;
  int flip = (facg < 0.0f);

  facg = fabsf(facg);

  fact *= facg;
  facm = 1.0f - fact;
  if (flip) {
    std::swap(fact, facm);
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

bool RE_texture_evaluate(const MTex *mtex,
                         const float vec[3],
                         const int thread,
                         ImagePool *pool,
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
  if (tex == nullptr) {
    return false;
  }

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
    do_2d_mapping(mtex, texvec, nullptr, dxt, dyt);
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
    texr.tin = IMB_colormanagement_get_luminance(texr.trgba);
  }
  else {
    copy_v3_fl3(texr.trgba, mtex->r, mtex->g, mtex->b);
  }

  *r_intensity = texr.tin;
  copy_v4_v4(r_rgba, texr.trgba);

  return (rgb != 0);
}
