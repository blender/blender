/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_image_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_image.h"

#include "RE_texture.h"

#include "texture_common.h"

static void boxsample(ImBuf *ibuf,
                      float minx,
                      float miny,
                      float maxx,
                      float maxy,
                      TexResult *texres,
                      const short imaprepeat,
                      const short imapextend);

/* *********** IMAGEWRAPPING ****************** */

/* x and y have to be checked for image size */
static void ibuf_get_color(float col[4], ImBuf *ibuf, int x, int y)
{
  int ofs = y * ibuf->x + x;

  if (ibuf->float_buffer.data) {
    if (ibuf->channels == 4) {
      const float *fp = ibuf->float_buffer.data + 4 * ofs;
      copy_v4_v4(col, fp);
    }
    else if (ibuf->channels == 3) {
      const float *fp = ibuf->float_buffer.data + 3 * ofs;
      copy_v3_v3(col, fp);
      col[3] = 1.0f;
    }
    else {
      const float *fp = ibuf->float_buffer.data + ofs;
      col[0] = col[1] = col[2] = col[3] = *fp;
    }
  }
  else {
    const uchar *rect = ibuf->byte_buffer.data + 4 * ofs;

    col[0] = float(rect[0]) * (1.0f / 255.0f);
    col[1] = float(rect[1]) * (1.0f / 255.0f);
    col[2] = float(rect[2]) * (1.0f / 255.0f);
    col[3] = float(rect[3]) * (1.0f / 255.0f);

    /* bytes are internally straight, however render pipeline seems to expect premul */
    col[0] *= col[3];
    col[1] *= col[3];
    col[2] *= col[3];
  }
}

int imagewrap(Tex *tex,
              Image *ima,
              const float texvec[3],
              TexResult *texres,
              ImagePool *pool,
              const bool skip_load_image)
{
  float fx, fy;
  int x, y, retval;
  int xi, yi; /* original values */

  texres->tin = texres->trgba[3] = texres->trgba[0] = texres->trgba[1] = texres->trgba[2] = 0.0f;

  retval = TEX_RGB;

  /* quick tests */
  if (ima == nullptr) {
    return retval;
  }

  /* hack for icon render */
  if (skip_load_image && !BKE_image_has_loaded_ibuf(ima)) {
    return retval;
  }

  ImageUser *iuser = &tex->iuser;
  ImageUser local_iuser;
  if (ima->source == IMA_SRC_TILED) {
    /* tex->iuser might be shared by threads, so create a local copy. */
    local_iuser = tex->iuser;
    iuser = &local_iuser;

    float new_uv[2];
    iuser->tile = BKE_image_get_tile_from_pos(ima, texvec, new_uv, nullptr);
    fx = new_uv[0];
    fy = new_uv[1];
  }
  else {
    fx = texvec[0];
    fy = texvec[1];
  }

  ImBuf *ibuf = BKE_image_pool_acquire_ibuf(ima, iuser, pool);

  ima->flag |= IMA_USED_FOR_RENDER;

  if (ibuf == nullptr || (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data == nullptr))
  {
    BKE_image_pool_release_ibuf(ima, ibuf, pool);
    return retval;
  }

  /* setup mapping */
  if (tex->imaflag & TEX_IMAROT) {
    SWAP(float, fx, fy);
  }

  if (tex->extend == TEX_CHECKER) {
    int xs, ys;

    xs = int(floor(fx));
    ys = int(floor(fy));
    fx -= xs;
    fy -= ys;

    if ((tex->flag & TEX_CHECKER_ODD) == 0) {
      if ((xs + ys) & 1) {
        /* pass */
      }
      else {
        if (ima) {
          BKE_image_pool_release_ibuf(ima, ibuf, pool);
        }
        return retval;
      }
    }
    if ((tex->flag & TEX_CHECKER_EVEN) == 0) {
      if ((xs + ys) & 1) {
        if (ima) {
          BKE_image_pool_release_ibuf(ima, ibuf, pool);
        }
        return retval;
      }
    }
    /* scale around center, (0.5, 0.5) */
    if (tex->checkerdist < 1.0f) {
      fx = (fx - 0.5f) / (1.0f - tex->checkerdist) + 0.5f;
      fy = (fy - 0.5f) / (1.0f - tex->checkerdist) + 0.5f;
    }
  }

  x = xi = int(floorf(fx * ibuf->x));
  y = yi = int(floorf(fy * ibuf->y));

  if (tex->extend == TEX_CLIPCUBE) {
    if (x < 0 || y < 0 || x >= ibuf->x || y >= ibuf->y || texvec[2] < -1.0f || texvec[2] > 1.0f) {
      if (ima) {
        BKE_image_pool_release_ibuf(ima, ibuf, pool);
      }
      return retval;
    }
  }
  else if (ELEM(tex->extend, TEX_CLIP, TEX_CHECKER)) {
    if (x < 0 || y < 0 || x >= ibuf->x || y >= ibuf->y) {
      if (ima) {
        BKE_image_pool_release_ibuf(ima, ibuf, pool);
      }
      return retval;
    }
  }
  else {
    if (tex->extend == TEX_EXTEND) {
      if (x >= ibuf->x) {
        x = ibuf->x - 1;
      }
      else if (x < 0) {
        x = 0;
      }
    }
    else {
      x = x % ibuf->x;
      if (x < 0) {
        x += ibuf->x;
      }
    }
    if (tex->extend == TEX_EXTEND) {
      if (y >= ibuf->y) {
        y = ibuf->y - 1;
      }
      else if (y < 0) {
        y = 0;
      }
    }
    else {
      y = y % ibuf->y;
      if (y < 0) {
        y += ibuf->y;
      }
    }
  }

  /* Keep this before interpolation #29761. */
  if (ima) {
    if ((tex->imaflag & TEX_USEALPHA) && (ima->alpha_mode != IMA_ALPHA_IGNORE)) {
      if ((tex->imaflag & TEX_CALCALPHA) == 0) {
        texres->talpha = true;
      }
    }
  }

  /* interpolate */
  if (tex->imaflag & TEX_INTERPOL) {
    float filterx, filtery;
    filterx = (0.5f * tex->filtersize) / ibuf->x;
    filtery = (0.5f * tex->filtersize) / ibuf->y;

    /* Important that this value is wrapped #27782.
     * this applies the modifications made by the checks above,
     * back to the floating point values */
    fx -= float(xi - x) / float(ibuf->x);
    fy -= float(yi - y) / float(ibuf->y);

    boxsample(ibuf,
              fx - filterx,
              fy - filtery,
              fx + filterx,
              fy + filtery,
              texres,
              (tex->extend == TEX_REPEAT),
              (tex->extend == TEX_EXTEND));
  }
  else { /* no filtering */
    ibuf_get_color(texres->trgba, ibuf, x, y);
  }

  if (texres->talpha) {
    texres->tin = texres->trgba[3];
  }
  else if (tex->imaflag & TEX_CALCALPHA) {
    texres->trgba[3] = texres->tin = max_fff(texres->trgba[0], texres->trgba[1], texres->trgba[2]);
  }
  else {
    texres->trgba[3] = texres->tin = 1.0;
  }

  if (tex->flag & TEX_NEGALPHA) {
    texres->trgba[3] = 1.0f - texres->trgba[3];
  }

  /* de-premul, this is being pre-multiplied in shade_input_do_shade()
   * do not de-premul for generated alpha, it is already in straight */
  if (texres->trgba[3] != 1.0f && texres->trgba[3] > 1e-4f && !(tex->imaflag & TEX_CALCALPHA)) {
    fx = 1.0f / texres->trgba[3];
    texres->trgba[0] *= fx;
    texres->trgba[1] *= fx;
    texres->trgba[2] *= fx;
  }

  if (ima) {
    BKE_image_pool_release_ibuf(ima, ibuf, pool);
  }

  BRICONTRGB;

  return retval;
}

static void clipx_rctf_swap(rctf *stack, short *count, float x1, float x2)
{
  rctf *rf, *newrct;
  short a;

  a = *count;
  rf = stack;
  for (; a > 0; a--) {
    if (rf->xmin < x1) {
      if (rf->xmax < x1) {
        rf->xmin += (x2 - x1);
        rf->xmax += (x2 - x1);
      }
      else {
        if (rf->xmax > x2) {
          rf->xmax = x2;
        }
        newrct = stack + *count;
        (*count)++;

        newrct->xmax = x2;
        newrct->xmin = rf->xmin + (x2 - x1);
        newrct->ymin = rf->ymin;
        newrct->ymax = rf->ymax;

        if (newrct->xmin == newrct->xmax) {
          (*count)--;
        }

        rf->xmin = x1;
      }
    }
    else if (rf->xmax > x2) {
      if (rf->xmin > x2) {
        rf->xmin -= (x2 - x1);
        rf->xmax -= (x2 - x1);
      }
      else {
        if (rf->xmin < x1) {
          rf->xmin = x1;
        }
        newrct = stack + *count;
        (*count)++;

        newrct->xmin = x1;
        newrct->xmax = rf->xmax - (x2 - x1);
        newrct->ymin = rf->ymin;
        newrct->ymax = rf->ymax;

        if (newrct->xmin == newrct->xmax) {
          (*count)--;
        }

        rf->xmax = x2;
      }
    }
    rf++;
  }
}

static void clipy_rctf_swap(rctf *stack, short *count, float y1, float y2)
{
  rctf *rf, *newrct;
  short a;

  a = *count;
  rf = stack;
  for (; a > 0; a--) {
    if (rf->ymin < y1) {
      if (rf->ymax < y1) {
        rf->ymin += (y2 - y1);
        rf->ymax += (y2 - y1);
      }
      else {
        if (rf->ymax > y2) {
          rf->ymax = y2;
        }
        newrct = stack + *count;
        (*count)++;

        newrct->ymax = y2;
        newrct->ymin = rf->ymin + (y2 - y1);
        newrct->xmin = rf->xmin;
        newrct->xmax = rf->xmax;

        if (newrct->ymin == newrct->ymax) {
          (*count)--;
        }

        rf->ymin = y1;
      }
    }
    else if (rf->ymax > y2) {
      if (rf->ymin > y2) {
        rf->ymin -= (y2 - y1);
        rf->ymax -= (y2 - y1);
      }
      else {
        if (rf->ymin < y1) {
          rf->ymin = y1;
        }
        newrct = stack + *count;
        (*count)++;

        newrct->ymin = y1;
        newrct->ymax = rf->ymax - (y2 - y1);
        newrct->xmin = rf->xmin;
        newrct->xmax = rf->xmax;

        if (newrct->ymin == newrct->ymax) {
          (*count)--;
        }

        rf->ymax = y2;
      }
    }
    rf++;
  }
}

static float square_rctf(const rctf *rf)
{
  float x, y;

  x = BLI_rctf_size_x(rf);
  y = BLI_rctf_size_y(rf);
  return x * y;
}

static float clipx_rctf(rctf *rf, float x1, float x2)
{
  float size;

  size = BLI_rctf_size_x(rf);

  if (rf->xmin < x1) {
    rf->xmin = x1;
  }
  if (rf->xmax > x2) {
    rf->xmax = x2;
  }
  if (rf->xmin > rf->xmax) {
    rf->xmin = rf->xmax;
    return 0.0;
  }
  if (size != 0.0f) {
    return BLI_rctf_size_x(rf) / size;
  }
  return 1.0;
}

static float clipy_rctf(rctf *rf, float y1, float y2)
{
  float size;

  size = BLI_rctf_size_y(rf);

  if (rf->ymin < y1) {
    rf->ymin = y1;
  }
  if (rf->ymax > y2) {
    rf->ymax = y2;
  }

  if (rf->ymin > rf->ymax) {
    rf->ymin = rf->ymax;
    return 0.0;
  }
  if (size != 0.0f) {
    return BLI_rctf_size_y(rf) / size;
  }
  return 1.0;
}

static void boxsampleclip(ImBuf *ibuf, const rctf *rf, TexResult *texres)
{
  /* Sample box, is clipped already, and minx etc. have been set at ibuf size.
   * Enlarge with anti-aliased edges of the pixels. */

  float muly, mulx, div, col[4];
  int x, y, startx, endx, starty, endy;

  startx = int(floor(rf->xmin));
  endx = int(floor(rf->xmax));
  starty = int(floor(rf->ymin));
  endy = int(floor(rf->ymax));

  if (startx < 0) {
    startx = 0;
  }
  if (starty < 0) {
    starty = 0;
  }
  if (endx >= ibuf->x) {
    endx = ibuf->x - 1;
  }
  if (endy >= ibuf->y) {
    endy = ibuf->y - 1;
  }

  if (starty == endy && startx == endx) {
    ibuf_get_color(texres->trgba, ibuf, startx, starty);
  }
  else {
    div = texres->trgba[0] = texres->trgba[1] = texres->trgba[2] = texres->trgba[3] = 0.0;
    for (y = starty; y <= endy; y++) {

      muly = 1.0;

      if (starty == endy) {
        /* pass */
      }
      else {
        if (y == starty) {
          muly = 1.0f - (rf->ymin - y);
        }
        if (y == endy) {
          muly = (rf->ymax - y);
        }
      }

      if (startx == endx) {
        mulx = muly;

        ibuf_get_color(col, ibuf, startx, y);
        madd_v4_v4fl(texres->trgba, col, mulx);
        div += mulx;
      }
      else {
        for (x = startx; x <= endx; x++) {
          mulx = muly;
          if (x == startx) {
            mulx *= 1.0f - (rf->xmin - x);
          }
          if (x == endx) {
            mulx *= (rf->xmax - x);
          }

          ibuf_get_color(col, ibuf, x, y);
          /* TODO(jbakker): No need to do manual optimization. Branching is slower than multiplying
           * with 1. */
          if (mulx == 1.0f) {
            add_v4_v4(texres->trgba, col);
            div += 1.0f;
          }
          else {
            madd_v4_v4fl(texres->trgba, col, mulx);
            div += mulx;
          }
        }
      }
    }

    if (div != 0.0f) {
      div = 1.0f / div;
      mul_v4_fl(texres->trgba, div);
    }
    else {
      zero_v4(texres->trgba);
    }
  }
}

static void boxsample(ImBuf *ibuf,
                      float minx,
                      float miny,
                      float maxx,
                      float maxy,
                      TexResult *texres,
                      const short imaprepeat,
                      const short imapextend)
{
  /* Sample box, performs clip. minx etc are in range 0.0 - 1.0 .
   * Enlarge with anti-aliased edges of pixels.
   * If variable 'imaprepeat' has been set, the
   * clipped-away parts are sampled as well.
   */
  /* NOTE: actually minx etc isn't in the proper range...
   *       this due to filter size and offset vectors for bump. */
  /* NOTE: talpha must be initialized. */
  /* NOTE: even when 'imaprepeat' is set, this can only repeat once in any direction.
   * the point which min/max is derived from is assumed to be wrapped. */
  TexResult texr;
  rctf *rf, stack[8];
  float opp, tot, alphaclip = 1.0;
  short count = 1;

  rf = stack;
  rf->xmin = minx * (ibuf->x);
  rf->xmax = maxx * (ibuf->x);
  rf->ymin = miny * (ibuf->y);
  rf->ymax = maxy * (ibuf->y);

  texr.talpha = texres->talpha; /* is read by boxsample_clip */

  if (imapextend) {
    CLAMP(rf->xmin, 0.0f, ibuf->x - 1);
    CLAMP(rf->xmax, 0.0f, ibuf->x - 1);
  }
  else if (imaprepeat) {
    clipx_rctf_swap(stack, &count, 0.0, float(ibuf->x));
  }
  else {
    alphaclip = clipx_rctf(rf, 0.0, float(ibuf->x));

    if (alphaclip <= 0.0f) {
      texres->trgba[0] = texres->trgba[2] = texres->trgba[1] = texres->trgba[3] = 0.0;
      return;
    }
  }

  if (imapextend) {
    CLAMP(rf->ymin, 0.0f, ibuf->y - 1);
    CLAMP(rf->ymax, 0.0f, ibuf->y - 1);
  }
  else if (imaprepeat) {
    clipy_rctf_swap(stack, &count, 0.0, float(ibuf->y));
  }
  else {
    alphaclip *= clipy_rctf(rf, 0.0, float(ibuf->y));

    if (alphaclip <= 0.0f) {
      texres->trgba[0] = texres->trgba[2] = texres->trgba[1] = texres->trgba[3] = 0.0;
      return;
    }
  }

  if (count > 1) {
    tot = texres->trgba[0] = texres->trgba[2] = texres->trgba[1] = texres->trgba[3] = 0.0;
    while (count--) {
      boxsampleclip(ibuf, rf, &texr);

      opp = square_rctf(rf);
      tot += opp;

      texres->trgba[0] += opp * texr.trgba[0];
      texres->trgba[1] += opp * texr.trgba[1];
      texres->trgba[2] += opp * texr.trgba[2];
      if (texres->talpha) {
        texres->trgba[3] += opp * texr.trgba[3];
      }
      rf++;
    }
    if (tot != 0.0f) {
      texres->trgba[0] /= tot;
      texres->trgba[1] /= tot;
      texres->trgba[2] /= tot;
      if (texres->talpha) {
        texres->trgba[3] /= tot;
      }
    }
  }
  else {
    boxsampleclip(ibuf, rf, texres);
  }

  if (texres->talpha == 0) {
    texres->trgba[3] = 1.0;
  }

  if (alphaclip != 1.0f) {
    /* premul it all */
    texres->trgba[0] *= alphaclip;
    texres->trgba[1] *= alphaclip;
    texres->trgba[2] *= alphaclip;
    texres->trgba[3] *= alphaclip;
  }
}

/* -------------------------------------------------------------------- */
/* from here, some functions only used for the new filtering */

/* anisotropic filters, data struct used instead of long line of (possibly unused) func args */
struct afdata_t {
  float dxt[2], dyt[2];
  int intpol, extflag;
  /* feline only */
  float majrad, minrad, theta;
  int iProbes;
  float dusc, dvsc;
};

/* this only used here to make it easier to pass extend flags as single int */
enum { TXC_XMIR = 1, TXC_YMIR, TXC_REPT, TXC_EXTD };

/**
 * Similar to `ibuf_get_color()` but clips/wraps coords according to repeat/extend flags
 * returns true if out of range in clip-mode.
 */
static int ibuf_get_color_clip(float col[4], ImBuf *ibuf, int x, int y, int extflag)
{
  int clip = 0;
  switch (extflag) {
    case TXC_XMIR: /* y rep */
      x %= 2 * ibuf->x;
      x += x < 0 ? 2 * ibuf->x : 0;
      x = x >= ibuf->x ? 2 * ibuf->x - x - 1 : x;
      y %= ibuf->y;
      y += y < 0 ? ibuf->y : 0;
      break;
    case TXC_YMIR: /* x rep */
      x %= ibuf->x;
      x += x < 0 ? ibuf->x : 0;
      y %= 2 * ibuf->y;
      y += y < 0 ? 2 * ibuf->y : 0;
      y = y >= ibuf->y ? 2 * ibuf->y - y - 1 : y;
      break;
    case TXC_EXTD:
      x = (x < 0) ? 0 : ((x >= ibuf->x) ? (ibuf->x - 1) : x);
      y = (y < 0) ? 0 : ((y >= ibuf->y) ? (ibuf->y - 1) : y);
      break;
    case TXC_REPT:
      x %= ibuf->x;
      x += (x < 0) ? ibuf->x : 0;
      y %= ibuf->y;
      y += (y < 0) ? ibuf->y : 0;
      break;
    default: { /* as extend, if clipped, set alpha to 0.0 */
      if (x < 0) {
        x = 0;
      } /* TXF alpha: clip = 1; } */
      if (x >= ibuf->x) {
        x = ibuf->x - 1;
      } /* TXF alpha:  clip = 1; } */
      if (y < 0) {
        y = 0;
      } /* TXF alpha:  clip = 1; } */
      if (y >= ibuf->y) {
        y = ibuf->y - 1;
      } /* TXF alpha:  clip = 1; } */
    }
  }

  if (ibuf->float_buffer.data) {
    const float *fp = ibuf->float_buffer.data + (x + y * ibuf->x) * ibuf->channels;
    if (ibuf->channels == 1) {
      col[0] = col[1] = col[2] = col[3] = *fp;
    }
    else {
      col[0] = fp[0];
      col[1] = fp[1];
      col[2] = fp[2];
      col[3] = clip ? 0.0f : (ibuf->channels == 4 ? fp[3] : 1.0f);
    }
  }
  else {
    const uchar *rect = ibuf->byte_buffer.data + 4 * (x + y * ibuf->x);
    float inv_alpha_fac = (1.0f / 255.0f) * rect[3] * (1.0f / 255.0f);
    col[0] = rect[0] * inv_alpha_fac;
    col[1] = rect[1] * inv_alpha_fac;
    col[2] = rect[2] * inv_alpha_fac;
    col[3] = clip ? 0.0f : rect[3] * (1.0f / 255.0f);
  }
  return clip;
}

/* as above + bilerp */
static int ibuf_get_color_clip_bilerp(
    float col[4], ImBuf *ibuf, float u, float v, int intpol, int extflag)
{
  if (intpol) {
    float c00[4], c01[4], c10[4], c11[4];
    const float ufl = floorf(u -= 0.5f), vfl = floorf(v -= 0.5f);
    const float uf = u - ufl, vf = v - vfl;
    const float w00 = (1.0f - uf) * (1.0f - vf), w10 = uf * (1.0f - vf), w01 = (1.0f - uf) * vf,
                w11 = uf * vf;
    const int x1 = int(ufl), y1 = int(vfl), x2 = x1 + 1, y2 = y1 + 1;
    int clip = ibuf_get_color_clip(c00, ibuf, x1, y1, extflag);
    clip |= ibuf_get_color_clip(c10, ibuf, x2, y1, extflag);
    clip |= ibuf_get_color_clip(c01, ibuf, x1, y2, extflag);
    clip |= ibuf_get_color_clip(c11, ibuf, x2, y2, extflag);
    col[0] = w00 * c00[0] + w10 * c10[0] + w01 * c01[0] + w11 * c11[0];
    col[1] = w00 * c00[1] + w10 * c10[1] + w01 * c01[1] + w11 * c11[1];
    col[2] = w00 * c00[2] + w10 * c10[2] + w01 * c01[2] + w11 * c11[2];
    col[3] = clip ? 0.0f : w00 * c00[3] + w10 * c10[3] + w01 * c01[3] + w11 * c11[3];
    return clip;
  }
  return ibuf_get_color_clip(col, ibuf, int(u), int(v), extflag);
}

static void area_sample(TexResult *texr, ImBuf *ibuf, float fx, float fy, afdata_t *AFD)
{
  int xs, ys, clip = 0;
  float tc[4], xsd, ysd, cw = 0.0f;
  const float ux = ibuf->x * AFD->dxt[0], uy = ibuf->y * AFD->dxt[1];
  const float vx = ibuf->x * AFD->dyt[0], vy = ibuf->y * AFD->dyt[1];
  int xsam = int(0.5f * sqrtf(ux * ux + uy * uy) + 0.5f);
  int ysam = int(0.5f * sqrtf(vx * vx + vy * vy) + 0.5f);
  const int minsam = AFD->intpol ? 2 : 4;
  xsam = CLAMPIS(xsam, minsam, ibuf->x * 2);
  ysam = CLAMPIS(ysam, minsam, ibuf->y * 2);
  xsd = 1.0f / xsam;
  ysd = 1.0f / ysam;
  texr->trgba[0] = texr->trgba[1] = texr->trgba[2] = texr->trgba[3] = 0.0f;
  for (ys = 0; ys < ysam; ys++) {
    for (xs = 0; xs < xsam; xs++) {
      const float su = (xs + ((ys & 1) + 0.5f) * 0.5f) * xsd - 0.5f;
      const float sv = (ys + ((xs & 1) + 0.5f) * 0.5f) * ysd - 0.5f;
      const float pu = fx + su * AFD->dxt[0] + sv * AFD->dyt[0];
      const float pv = fy + su * AFD->dxt[1] + sv * AFD->dyt[1];
      const int out = ibuf_get_color_clip_bilerp(
          tc, ibuf, pu * ibuf->x, pv * ibuf->y, AFD->intpol, AFD->extflag);
      clip |= out;
      cw += out ? 0.0f : 1.0f;
      texr->trgba[0] += tc[0];
      texr->trgba[1] += tc[1];
      texr->trgba[2] += tc[2];
      texr->trgba[3] += texr->talpha ? tc[3] : 0.0f;
    }
  }
  xsd *= ysd;
  texr->trgba[0] *= xsd;
  texr->trgba[1] *= xsd;
  texr->trgba[2] *= xsd;
  /* clipping can be ignored if alpha used, texr->trgba[3] already includes filtered edge */
  texr->trgba[3] = texr->talpha ? texr->trgba[3] * xsd : (clip ? cw * xsd : 1.0f);
}

struct ReadEWAData {
  ImBuf *ibuf;
  afdata_t *AFD;
};

static void ewa_read_pixel_cb(void *userdata, int x, int y, float result[4])
{
  ReadEWAData *data = (ReadEWAData *)userdata;
  ibuf_get_color_clip(result, data->ibuf, x, y, data->AFD->extflag);
}

static void ewa_eval(TexResult *texr, ImBuf *ibuf, float fx, float fy, afdata_t *AFD)
{
  ReadEWAData data;
  const float uv[2] = {fx, fy};
  data.ibuf = ibuf;
  data.AFD = AFD;
  BLI_ewa_filter(ibuf->x,
                 ibuf->y,
                 AFD->intpol != 0,
                 texr->talpha,
                 uv,
                 AFD->dxt,
                 AFD->dyt,
                 ewa_read_pixel_cb,
                 &data,
                 texr->trgba);
}

static void feline_eval(TexResult *texr, ImBuf *ibuf, float fx, float fy, afdata_t *AFD)
{
  const int maxn = AFD->iProbes - 1;
  const float ll = ((AFD->majrad == AFD->minrad) ? 2.0f * AFD->majrad :
                                                   2.0f * (AFD->majrad - AFD->minrad)) /
                   (maxn ? float(maxn) : 1.0f);
  float du = maxn ? cosf(AFD->theta) * ll : 0.0f;
  float dv = maxn ? sinf(AFD->theta) * ll : 0.0f;
  // const float D = -0.5f*(du*du + dv*dv) / (AFD->majrad*AFD->majrad);
  const float D = (EWA_MAXIDX + 1) * 0.25f * (du * du + dv * dv) / (AFD->majrad * AFD->majrad);
  float d; /* TXF alpha: `cw = 0.0f`. */
  int n;   /* TXF alpha: `clip = 0`. */
  /* Have to use same scaling for du/dv here as for Ux/Vx/Uy/Vy (*after* D is calculated.) */
  du *= AFD->dusc;
  dv *= AFD->dvsc;
  d = texr->trgba[0] = texr->trgba[2] = texr->trgba[1] = texr->trgba[3] = 0.0f;
  for (n = -maxn; n <= maxn; n += 2) {
    float tc[4];
    const float hn = n * 0.5f;
    const float u = fx + hn * du, v = fy + hn * dv;
/* Can use ewa table here too. */
#if 0
    const float wt = expf(n * n * D);
#else
    const float wt = EWA_WTS[int(n * n * D)];
#endif
    /* `const int out =` */ ibuf_get_color_clip_bilerp(
        tc, ibuf, ibuf->x * u, ibuf->y * v, AFD->intpol, AFD->extflag);
    /* TXF alpha: `clip |= out;`
     * TXF alpha: `cw += out ? 0.0f : wt;` */
    texr->trgba[0] += tc[0] * wt;
    texr->trgba[1] += tc[1] * wt;
    texr->trgba[2] += tc[2] * wt;
    texr->trgba[3] += texr->talpha ? tc[3] * wt : 0.0f;
    d += wt;
  }

  d = 1.0f / d;
  texr->trgba[0] *= d;
  texr->trgba[1] *= d;
  texr->trgba[2] *= d;
  /* Clipping can be ignored if alpha used, `texr->trgba[3]` already includes filtered edge */
  texr->trgba[3] = texr->talpha ? texr->trgba[3] * d :
                                  1.0f; /* TXF alpha: `(clip ? cw*d : 1.0f);` */
}
#undef EWA_MAXIDX

static void alpha_clip_aniso(const ImBuf *ibuf,
                             float minx,
                             float miny,
                             float maxx,
                             float maxy,
                             int extflag,
                             TexResult *texres)
{
  float alphaclip;
  rctf rf;

  /* TXF alpha: we're doing the same alpha-clip here as box-sample, but I'm doubting
   * if this is actually correct for the all the filtering algorithms. */

  if (!ELEM(extflag, TXC_REPT, TXC_EXTD)) {
    rf.xmin = minx * (ibuf->x);
    rf.xmax = maxx * (ibuf->x);
    rf.ymin = miny * (ibuf->y);
    rf.ymax = maxy * (ibuf->y);

    alphaclip = clipx_rctf(&rf, 0.0, float(ibuf->x));
    alphaclip *= clipy_rctf(&rf, 0.0, float(ibuf->y));
    alphaclip = max_ff(alphaclip, 0.0f);

    if (alphaclip != 1.0f) {
      /* premul it all */
      texres->trgba[0] *= alphaclip;
      texres->trgba[1] *= alphaclip;
      texres->trgba[2] *= alphaclip;
      texres->trgba[3] *= alphaclip;
    }
  }
}

static void image_mipmap_test(Tex *tex, ImBuf *ibuf)
{
  if (tex->imaflag & TEX_MIPMAP) {
    if (ibuf->mipmap[0] && (ibuf->userflags & IB_MIPMAP_INVALID)) {
      BLI_thread_lock(LOCK_IMAGE);
      if (ibuf->userflags & IB_MIPMAP_INVALID) {
        IMB_remakemipmap(ibuf, tex->imaflag & TEX_GAUSS_MIP);
        ibuf->userflags &= ~IB_MIPMAP_INVALID;
      }
      BLI_thread_unlock(LOCK_IMAGE);
    }
    if (ibuf->mipmap[0] == nullptr) {
      BLI_thread_lock(LOCK_IMAGE);
      if (ibuf->mipmap[0] == nullptr) {
        IMB_makemipmap(ibuf, tex->imaflag & TEX_GAUSS_MIP);
      }
      BLI_thread_unlock(LOCK_IMAGE);
    }
    /* if no mipmap could be made, fall back on non-mipmap render */
    if (ibuf->mipmap[0] == nullptr) {
      tex->imaflag &= ~TEX_MIPMAP;
    }
  }
}

static int imagewraposa_aniso(Tex *tex,
                              Image *ima,
                              ImBuf *ibuf,
                              const float texvec[3],
                              float dxt[2],
                              float dyt[2],
                              TexResult *texres,
                              ImagePool *pool,
                              const bool skip_load_image)
{
  TexResult texr;
  float fx, fy, minx, maxx, miny, maxy;
  float maxd;
  int curmap, retval, intpol, extflag = 0;
  afdata_t AFD;

  void (*filterfunc)(TexResult *, ImBuf *, float, float, afdata_t *);
  switch (tex->texfilter) {
    case TXF_EWA:
      filterfunc = ewa_eval;
      break;
    case TXF_FELINE:
      filterfunc = feline_eval;
      break;
    case TXF_AREA:
    default:
      filterfunc = area_sample;
  }

  texres->tin = texres->trgba[3] = texres->trgba[0] = texres->trgba[1] = texres->trgba[2] = 0.0f;

  retval = TEX_RGB;

  /* quick tests */
  if (ibuf == nullptr && ima == nullptr) {
    return retval;
  }

  if (ima) { /* hack for icon render */
    if (skip_load_image && !BKE_image_has_loaded_ibuf(ima)) {
      return retval;
    }
    ibuf = BKE_image_pool_acquire_ibuf(ima, &tex->iuser, pool);
  }

  if ((ibuf == nullptr) ||
      ((ibuf->byte_buffer.data == nullptr) && (ibuf->float_buffer.data == nullptr)))
  {
    if (ima) {
      BKE_image_pool_release_ibuf(ima, ibuf, pool);
    }
    return retval;
  }

  if (ima) {
    ima->flag |= IMA_USED_FOR_RENDER;
  }

  /* mipmap test */
  image_mipmap_test(tex, ibuf);

  if (ima) {
    if ((tex->imaflag & TEX_USEALPHA) && (ima->alpha_mode != IMA_ALPHA_IGNORE)) {
      if ((tex->imaflag & TEX_CALCALPHA) == 0) {
        texres->talpha = true;
      }
    }
  }
  texr.talpha = texres->talpha;

  if (tex->imaflag & TEX_IMAROT) {
    fy = texvec[0];
    fx = texvec[1];
  }
  else {
    fx = texvec[0];
    fy = texvec[1];
  }

  /* pixel coordinates */
  minx = min_fff(dxt[0], dyt[0], dxt[0] + dyt[0]);
  maxx = max_fff(dxt[0], dyt[0], dxt[0] + dyt[0]);
  miny = min_fff(dxt[1], dyt[1], dxt[1] + dyt[1]);
  maxy = max_fff(dxt[1], dyt[1], dxt[1] + dyt[1]);

  /* tex_sharper has been removed */
  minx = (maxx - minx) * 0.5f;
  miny = (maxy - miny) * 0.5f;

  if (tex->imaflag & TEX_FILTER_MIN) {
    /* Make sure the filtersize is minimal in pixels
     * (normal, ref map can have miniature pixel dx/dy). */
    const float addval = (0.5f * tex->filtersize) / float(MIN2(ibuf->x, ibuf->y));
    if (addval > minx) {
      minx = addval;
    }
    if (addval > miny) {
      miny = addval;
    }
  }
  else if (tex->filtersize != 1.0f) {
    minx *= tex->filtersize;
    miny *= tex->filtersize;
    dxt[0] *= tex->filtersize;
    dxt[1] *= tex->filtersize;
    dyt[0] *= tex->filtersize;
    dyt[1] *= tex->filtersize;
  }

  if (tex->imaflag & TEX_IMAROT) {
    float t;
    SWAP(float, minx, miny);
    /* must rotate dxt/dyt 90 deg
     * yet another blender problem is that swapping X/Y axes (or any tex projection switches)
     * should do something similar, but it doesn't, it only swaps coords,
     * so filter area will be incorrect in those cases. */
    t = dxt[0];
    dxt[0] = dxt[1];
    dxt[1] = -t;
    t = dyt[0];
    dyt[0] = dyt[1];
    dyt[1] = -t;
  }

  /* side faces of unit-cube */
  minx = (minx > 0.25f) ? 0.25f : ((minx < 1e-5f) ? 1e-5f : minx);
  miny = (miny > 0.25f) ? 0.25f : ((miny < 1e-5f) ? 1e-5f : miny);

  /* repeat and clip */

  if (tex->extend == TEX_REPEAT) {
    if ((tex->flag & (TEX_REPEAT_XMIR | TEX_REPEAT_YMIR)) == (TEX_REPEAT_XMIR | TEX_REPEAT_YMIR)) {
      extflag = TXC_EXTD;
    }
    else if (tex->flag & TEX_REPEAT_XMIR) {
      extflag = TXC_XMIR;
    }
    else if (tex->flag & TEX_REPEAT_YMIR) {
      extflag = TXC_YMIR;
    }
    else {
      extflag = TXC_REPT;
    }
  }
  else if (tex->extend == TEX_EXTEND) {
    extflag = TXC_EXTD;
  }

  if (tex->extend == TEX_CHECKER) {
    int xs = int(floorf(fx)), ys = int(floorf(fy));
    /* both checkers available, no boundary exceptions, checkerdist will eat aliasing */
    if ((tex->flag & TEX_CHECKER_ODD) && (tex->flag & TEX_CHECKER_EVEN)) {
      fx -= xs;
      fy -= ys;
    }
    else if ((tex->flag & TEX_CHECKER_ODD) == 0 && (tex->flag & TEX_CHECKER_EVEN) == 0) {
      if (ima) {
        BKE_image_pool_release_ibuf(ima, ibuf, pool);
      }
      return retval;
    }
    else {
      int xs1 = int(floorf(fx - minx));
      int ys1 = int(floorf(fy - miny));
      int xs2 = int(floorf(fx + minx));
      int ys2 = int(floorf(fy + miny));
      if ((xs1 != xs2) || (ys1 != ys2)) {
        if (tex->flag & TEX_CHECKER_ODD) {
          fx -= ((xs1 + ys) & 1) ? xs2 : xs1;
          fy -= ((ys1 + xs) & 1) ? ys2 : ys1;
        }
        if (tex->flag & TEX_CHECKER_EVEN) {
          fx -= ((xs1 + ys) & 1) ? xs1 : xs2;
          fy -= ((ys1 + xs) & 1) ? ys1 : ys2;
        }
      }
      else {
        if ((tex->flag & TEX_CHECKER_ODD) == 0 && ((xs + ys) & 1) == 0) {
          if (ima) {
            BKE_image_pool_release_ibuf(ima, ibuf, pool);
          }
          return retval;
        }
        if ((tex->flag & TEX_CHECKER_EVEN) == 0 && (xs + ys) & 1) {
          if (ima) {
            BKE_image_pool_release_ibuf(ima, ibuf, pool);
          }
          return retval;
        }
        fx -= xs;
        fy -= ys;
      }
    }
    /* scale around center, (0.5, 0.5) */
    if (tex->checkerdist < 1.0f) {
      const float omcd = 1.0f / (1.0f - tex->checkerdist);
      fx = (fx - 0.5f) * omcd + 0.5f;
      fy = (fy - 0.5f) * omcd + 0.5f;
      minx *= omcd;
      miny *= omcd;
    }
  }

  if (tex->extend == TEX_CLIPCUBE) {
    if ((fx + minx) < 0.0f || (fy + miny) < 0.0f || (fx - minx) > 1.0f || (fy - miny) > 1.0f ||
        texvec[2] < -1.0f || texvec[2] > 1.0f)
    {
      if (ima) {
        BKE_image_pool_release_ibuf(ima, ibuf, pool);
      }
      return retval;
    }
  }
  else if (ELEM(tex->extend, TEX_CLIP, TEX_CHECKER)) {
    if ((fx + minx) < 0.0f || (fy + miny) < 0.0f || (fx - minx) > 1.0f || (fy - miny) > 1.0f) {
      if (ima) {
        BKE_image_pool_release_ibuf(ima, ibuf, pool);
      }
      return retval;
    }
  }
  else {
    if (tex->extend == TEX_EXTEND) {
      fx = (fx > 1.0f) ? 1.0f : ((fx < 0.0f) ? 0.0f : fx);
      fy = (fy > 1.0f) ? 1.0f : ((fy < 0.0f) ? 0.0f : fy);
    }
    else {
      fx -= floorf(fx);
      fy -= floorf(fy);
    }
  }

  intpol = tex->imaflag & TEX_INTERPOL;

  /* struct common data */
  copy_v2_v2(AFD.dxt, dxt);
  copy_v2_v2(AFD.dyt, dyt);
  AFD.intpol = intpol;
  AFD.extflag = extflag;

  /* NOTE(@brecht): added stupid clamping here, large dx/dy can give very large
   * filter sizes which take ages to render, it may be better to do this
   * more intelligently later in the code .. probably it's not noticeable */
  if (AFD.dxt[0] * AFD.dxt[0] + AFD.dxt[1] * AFD.dxt[1] > 2.0f * 2.0f) {
    mul_v2_fl(AFD.dxt, 2.0f / len_v2(AFD.dxt));
  }
  if (AFD.dyt[0] * AFD.dyt[0] + AFD.dyt[1] * AFD.dyt[1] > 2.0f * 2.0f) {
    mul_v2_fl(AFD.dyt, 2.0f / len_v2(AFD.dyt));
  }

  /* choice: */
  if (tex->imaflag & TEX_MIPMAP) {
    ImBuf *previbuf, *curibuf;
    float levf;
    int maxlev;
    ImBuf *mipmaps[IMB_MIPMAP_LEVELS + 1];

    /* Modify ellipse minor axis if too eccentric, use for area sampling as well
     * scaling `dxt/dyt` as done in PBRT is not the same
     * (as in `ewa_eval()`, scale by `sqrt(ibuf->x)` to maximize precision). */
    const float ff = sqrtf(ibuf->x), q = ibuf->y / ff;
    const float Ux = dxt[0] * ff, Vx = dxt[1] * q, Uy = dyt[0] * ff, Vy = dyt[1] * q;
    const float A = Vx * Vx + Vy * Vy;
    const float B = -2.0f * (Ux * Vx + Uy * Vy);
    const float C = Ux * Ux + Uy * Uy;
    const float F = A * C - B * B * 0.25f;
    float a, b, th, ecc;
    BLI_ewa_imp2radangle(A, B, C, F, &a, &b, &th, &ecc);
    if (tex->texfilter == TXF_FELINE) {
      float fProbes;
      a *= ff;
      b *= ff;
      a = max_ff(a, 1.0f);
      b = max_ff(b, 1.0f);
      fProbes = 2.0f * (a / b) - 1.0f;
      AFD.iProbes = round_fl_to_int(fProbes);
      AFD.iProbes = MIN2(AFD.iProbes, tex->afmax);
      if (AFD.iProbes < fProbes) {
        b = 2.0f * a / float(AFD.iProbes + 1);
      }
      AFD.majrad = a / ff;
      AFD.minrad = b / ff;
      AFD.theta = th;
      AFD.dusc = 1.0f / ff;
      AFD.dvsc = ff / float(ibuf->y);
    }
    else { /* EWA & area */
      if (ecc > float(tex->afmax)) {
        b = a / float(tex->afmax);
      }
      b *= ff;
    }
    maxd = max_ff(b, 1e-8f);
    levf = (float(M_LOG2E)) * logf(maxd);

    curmap = 0;
    maxlev = 1;
    mipmaps[0] = ibuf;
    while (curmap < IMB_MIPMAP_LEVELS) {
      mipmaps[curmap + 1] = ibuf->mipmap[curmap];
      if (ibuf->mipmap[curmap]) {
        maxlev++;
      }
      curmap++;
    }

    /* mipmap level */
    if (levf < 0.0f) { /* original image only */
      previbuf = curibuf = mipmaps[0];
      levf = 0.0f;
    }
    else if (levf >= maxlev - 1) {
      previbuf = curibuf = mipmaps[maxlev - 1];
      levf = 0.0f;
      if (tex->texfilter == TXF_FELINE) {
        AFD.iProbes = 1;
      }
    }
    else {
      const int lev = isnan(levf) ? 0 : int(levf);
      curibuf = mipmaps[lev];
      previbuf = mipmaps[lev + 1];
      levf -= floorf(levf);
    }

    /* filter functions take care of interpolation themselves, no need to modify dxt/dyt here */
    filterfunc(texres, curibuf, fx, fy, &AFD);
    if (previbuf != curibuf) { /* interpolate */
      filterfunc(&texr, previbuf, fx, fy, &AFD);
      texres->trgba[0] += levf * (texr.trgba[0] - texres->trgba[0]);
      texres->trgba[1] += levf * (texr.trgba[1] - texres->trgba[1]);
      texres->trgba[2] += levf * (texr.trgba[2] - texres->trgba[2]);
      texres->trgba[3] += levf * (texr.trgba[3] - texres->trgba[3]);
    }

    if (tex->texfilter != TXF_EWA) {
      alpha_clip_aniso(ibuf, fx - minx, fy - miny, fx + minx, fy + miny, extflag, texres);
    }
  }
  else { /* no mipmap */
    /* filter functions take care of interpolation themselves, no need to modify dxt/dyt here */
    if (tex->texfilter == TXF_FELINE) {
      const float ff = sqrtf(ibuf->x), q = ibuf->y / ff;
      const float Ux = dxt[0] * ff, Vx = dxt[1] * q, Uy = dyt[0] * ff, Vy = dyt[1] * q;
      const float A = Vx * Vx + Vy * Vy;
      const float B = -2.0f * (Ux * Vx + Uy * Vy);
      const float C = Ux * Ux + Uy * Uy;
      const float F = A * C - B * B * 0.25f;
      float a, b, th, ecc, fProbes;
      BLI_ewa_imp2radangle(A, B, C, F, &a, &b, &th, &ecc);
      a *= ff;
      b *= ff;
      a = max_ff(a, 1.0f);
      b = max_ff(b, 1.0f);
      fProbes = 2.0f * (a / b) - 1.0f;
      /* no limit to number of Probes here */
      AFD.iProbes = round_fl_to_int(fProbes);
      if (AFD.iProbes < fProbes) {
        b = 2.0f * a / float(AFD.iProbes + 1);
      }
      AFD.majrad = a / ff;
      AFD.minrad = b / ff;
      AFD.theta = th;
      AFD.dusc = 1.0f / ff;
      AFD.dvsc = ff / float(ibuf->y);
    }
    filterfunc(texres, ibuf, fx, fy, &AFD);
    if (tex->texfilter != TXF_EWA) {
      alpha_clip_aniso(ibuf, fx - minx, fy - miny, fx + minx, fy + miny, extflag, texres);
    }
  }

  if (tex->imaflag & TEX_CALCALPHA) {
    texres->trgba[3] = texres->tin = texres->trgba[3] *
                                     max_fff(texres->trgba[0], texres->trgba[1], texres->trgba[2]);
  }
  else {
    texres->tin = texres->trgba[3];
  }
  if (tex->flag & TEX_NEGALPHA) {
    texres->trgba[3] = 1.0f - texres->trgba[3];
  }

  /* de-premul, this is being pre-multiplied in shade_input_do_shade()
   * TXF: this currently does not (yet?) work properly, destroys edge AA in clip/checker mode,
   * so for now commented out also disabled in imagewraposa()
   * to be able to compare results with blender's default texture filtering */

  /* brecht: tried to fix this, see "TXF alpha" comments */

  /* do not de-premul for generated alpha, it is already in straight */
  if (texres->trgba[3] != 1.0f && texres->trgba[3] > 1e-4f && !(tex->imaflag & TEX_CALCALPHA)) {
    fx = 1.0f / texres->trgba[3];
    texres->trgba[0] *= fx;
    texres->trgba[1] *= fx;
    texres->trgba[2] *= fx;
  }

  if (ima) {
    BKE_image_pool_release_ibuf(ima, ibuf, pool);
  }

  BRICONTRGB;

  return retval;
}

int imagewraposa(Tex *tex,
                 Image *ima,
                 ImBuf *ibuf,
                 const float texvec[3],
                 const float DXT[2],
                 const float DYT[2],
                 TexResult *texres,
                 ImagePool *pool,
                 const bool skip_load_image)
{
  TexResult texr;
  float fx, fy, minx, maxx, miny, maxy, dx, dy, dxt[2], dyt[2];
  float maxd, pixsize;
  int curmap, retval, imaprepeat, imapextend;

  /* TXF: since dxt/dyt might be modified here and since they might be needed after imagewraposa()
   * call, make a local copy here so that original vecs remain untouched. */
  copy_v2_v2(dxt, DXT);
  copy_v2_v2(dyt, DYT);

  /* anisotropic filtering */
  if (tex->texfilter != TXF_BOX) {
    return imagewraposa_aniso(tex, ima, ibuf, texvec, dxt, dyt, texres, pool, skip_load_image);
  }

  texres->tin = texres->trgba[3] = texres->trgba[0] = texres->trgba[1] = texres->trgba[2] = 0.0f;

  retval = TEX_RGB;

  /* quick tests */
  if (ibuf == nullptr && ima == nullptr) {
    return retval;
  }
  if (ima) {

    /* hack for icon render */
    if (skip_load_image && !BKE_image_has_loaded_ibuf(ima)) {
      return retval;
    }

    ibuf = BKE_image_pool_acquire_ibuf(ima, &tex->iuser, pool);

    ima->flag |= IMA_USED_FOR_RENDER;
  }
  if (ibuf == nullptr || (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data == nullptr))
  {
    if (ima) {
      BKE_image_pool_release_ibuf(ima, ibuf, pool);
    }
    return retval;
  }

  /* mipmap test */
  image_mipmap_test(tex, ibuf);

  if (ima) {
    if ((tex->imaflag & TEX_USEALPHA) && (ima->alpha_mode != IMA_ALPHA_IGNORE)) {
      if ((tex->imaflag & TEX_CALCALPHA) == 0) {
        texres->talpha = true;
      }
    }
  }

  texr.talpha = texres->talpha;

  if (tex->imaflag & TEX_IMAROT) {
    fy = texvec[0];
    fx = texvec[1];
  }
  else {
    fx = texvec[0];
    fy = texvec[1];
  }

  /* pixel coordinates */

  minx = min_fff(dxt[0], dyt[0], dxt[0] + dyt[0]);
  maxx = max_fff(dxt[0], dyt[0], dxt[0] + dyt[0]);
  miny = min_fff(dxt[1], dyt[1], dxt[1] + dyt[1]);
  maxy = max_fff(dxt[1], dyt[1], dxt[1] + dyt[1]);

  /* tex_sharper has been removed */
  minx = (maxx - minx) / 2.0f;
  miny = (maxy - miny) / 2.0f;

  if (tex->imaflag & TEX_FILTER_MIN) {
    /* Make sure the filtersize is minimal in pixels
     * (normal, ref map can have miniature pixel dx/dy). */
    float addval = (0.5f * tex->filtersize) / float(MIN2(ibuf->x, ibuf->y));

    if (addval > minx) {
      minx = addval;
    }
    if (addval > miny) {
      miny = addval;
    }
  }
  else if (tex->filtersize != 1.0f) {
    minx *= tex->filtersize;
    miny *= tex->filtersize;

    dxt[0] *= tex->filtersize;
    dxt[1] *= tex->filtersize;
    dyt[0] *= tex->filtersize;
    dyt[1] *= tex->filtersize;
  }

  if (tex->imaflag & TEX_IMAROT) {
    SWAP(float, minx, miny);
  }

  if (minx > 0.25f) {
    minx = 0.25f;
  }
  else if (minx < 0.00001f) {
    minx = 0.00001f; /* side faces of unit-cube */
  }
  if (miny > 0.25f) {
    miny = 0.25f;
  }
  else if (miny < 0.00001f) {
    miny = 0.00001f;
  }

  /* repeat and clip */
  imaprepeat = (tex->extend == TEX_REPEAT);
  imapextend = (tex->extend == TEX_EXTEND);

  if (tex->extend == TEX_REPEAT) {
    if (tex->flag & (TEX_REPEAT_XMIR | TEX_REPEAT_YMIR)) {
      imaprepeat = 0;
      imapextend = 1;
    }
  }

  if (tex->extend == TEX_CHECKER) {
    int xs, ys, xs1, ys1, xs2, ys2, boundary;

    xs = int(floor(fx));
    ys = int(floor(fy));

    /* both checkers available, no boundary exceptions, checkerdist will eat aliasing */
    if ((tex->flag & TEX_CHECKER_ODD) && (tex->flag & TEX_CHECKER_EVEN)) {
      fx -= xs;
      fy -= ys;
    }
    else if ((tex->flag & TEX_CHECKER_ODD) == 0 && (tex->flag & TEX_CHECKER_EVEN) == 0) {
      if (ima) {
        BKE_image_pool_release_ibuf(ima, ibuf, pool);
      }
      return retval;
    }
    else {

      xs1 = int(floor(fx - minx));
      ys1 = int(floor(fy - miny));
      xs2 = int(floor(fx + minx));
      ys2 = int(floor(fy + miny));
      boundary = (xs1 != xs2) || (ys1 != ys2);

      if (boundary == 0) {
        if ((tex->flag & TEX_CHECKER_ODD) == 0) {
          if ((xs + ys) & 1) {
            /* pass */
          }
          else {
            if (ima) {
              BKE_image_pool_release_ibuf(ima, ibuf, pool);
            }
            return retval;
          }
        }
        if ((tex->flag & TEX_CHECKER_EVEN) == 0) {
          if ((xs + ys) & 1) {
            if (ima) {
              BKE_image_pool_release_ibuf(ima, ibuf, pool);
            }
            return retval;
          }
        }
        fx -= xs;
        fy -= ys;
      }
      else {
        if (tex->flag & TEX_CHECKER_ODD) {
          if ((xs1 + ys) & 1) {
            fx -= xs2;
          }
          else {
            fx -= xs1;
          }

          if ((ys1 + xs) & 1) {
            fy -= ys2;
          }
          else {
            fy -= ys1;
          }
        }
        if (tex->flag & TEX_CHECKER_EVEN) {
          if ((xs1 + ys) & 1) {
            fx -= xs1;
          }
          else {
            fx -= xs2;
          }

          if ((ys1 + xs) & 1) {
            fy -= ys1;
          }
          else {
            fy -= ys2;
          }
        }
      }
    }

    /* scale around center, (0.5, 0.5) */
    if (tex->checkerdist < 1.0f) {
      fx = (fx - 0.5f) / (1.0f - tex->checkerdist) + 0.5f;
      fy = (fy - 0.5f) / (1.0f - tex->checkerdist) + 0.5f;
      minx /= (1.0f - tex->checkerdist);
      miny /= (1.0f - tex->checkerdist);
    }
  }

  if (tex->extend == TEX_CLIPCUBE) {
    if (fx + minx < 0.0f || fy + miny < 0.0f || fx - minx > 1.0f || fy - miny > 1.0f ||
        texvec[2] < -1.0f || texvec[2] > 1.0f)
    {
      if (ima) {
        BKE_image_pool_release_ibuf(ima, ibuf, pool);
      }
      return retval;
    }
  }
  else if (ELEM(tex->extend, TEX_CLIP, TEX_CHECKER)) {
    if (fx + minx < 0.0f || fy + miny < 0.0f || fx - minx > 1.0f || fy - miny > 1.0f) {
      if (ima) {
        BKE_image_pool_release_ibuf(ima, ibuf, pool);
      }
      return retval;
    }
  }
  else {
    if (imapextend) {
      if (fx > 1.0f) {
        fx = 1.0f;
      }
      else if (fx < 0.0f) {
        fx = 0.0f;
      }
    }
    else {
      if (fx > 1.0f) {
        fx -= int(fx);
      }
      else if (fx < 0.0f) {
        fx += 1 - int(fx);
      }
    }

    if (imapextend) {
      if (fy > 1.0f) {
        fy = 1.0f;
      }
      else if (fy < 0.0f) {
        fy = 0.0f;
      }
    }
    else {
      if (fy > 1.0f) {
        fy -= int(fy);
      }
      else if (fy < 0.0f) {
        fy += 1 - int(fy);
      }
    }
  }

  /* Choice: */
  if (tex->imaflag & TEX_MIPMAP) {
    ImBuf *previbuf, *curibuf;

    dx = minx;
    dy = miny;
    maxd = max_ff(dx, dy);
    if (maxd > 0.5f) {
      maxd = 0.5f;
    }

    pixsize = 1.0f / float(MIN2(ibuf->x, ibuf->y));

    curmap = 0;
    previbuf = curibuf = ibuf;
    while (curmap < IMB_MIPMAP_LEVELS && ibuf->mipmap[curmap]) {
      if (maxd < pixsize) {
        break;
      }
      previbuf = curibuf;
      curibuf = ibuf->mipmap[curmap];
      pixsize = 1.0f / float(MIN2(curibuf->x, curibuf->y));
      curmap++;
    }

    if (previbuf != curibuf || (tex->imaflag & TEX_INTERPOL)) {
      /* sample at least 1 pixel */
      if (minx < 0.5f / ibuf->x) {
        minx = 0.5f / ibuf->x;
      }
      if (miny < 0.5f / ibuf->y) {
        miny = 0.5f / ibuf->y;
      }
    }

    maxx = fx + minx;
    minx = fx - minx;
    maxy = fy + miny;
    miny = fy - miny;

    boxsample(curibuf, minx, miny, maxx, maxy, texres, imaprepeat, imapextend);

    if (previbuf != curibuf) { /* interpolate */
      boxsample(previbuf, minx, miny, maxx, maxy, &texr, imaprepeat, imapextend);

      fx = 2.0f * (pixsize - maxd) / pixsize;

      if (fx >= 1.0f) {
        texres->trgba[3] = texr.trgba[3];
        texres->trgba[2] = texr.trgba[2];
        texres->trgba[1] = texr.trgba[1];
        texres->trgba[0] = texr.trgba[0];
      }
      else {
        fy = 1.0f - fx;
        texres->trgba[2] = fy * texres->trgba[2] + fx * texr.trgba[2];
        texres->trgba[1] = fy * texres->trgba[1] + fx * texr.trgba[1];
        texres->trgba[0] = fy * texres->trgba[0] + fx * texr.trgba[0];
        texres->trgba[3] = fy * texres->trgba[3] + fx * texr.trgba[3];
      }
    }
  }
  else {
    const int intpol = tex->imaflag & TEX_INTERPOL;
    if (intpol) {
      /* sample 1 pixel minimum */
      if (minx < 0.5f / ibuf->x) {
        minx = 0.5f / ibuf->x;
      }
      if (miny < 0.5f / ibuf->y) {
        miny = 0.5f / ibuf->y;
      }
    }

    boxsample(ibuf, fx - minx, fy - miny, fx + minx, fy + miny, texres, imaprepeat, imapextend);
  }

  if (tex->imaflag & TEX_CALCALPHA) {
    texres->trgba[3] = texres->tin = texres->trgba[3] *
                                     max_fff(texres->trgba[0], texres->trgba[1], texres->trgba[2]);
  }
  else {
    texres->tin = texres->trgba[3];
  }

  if (tex->flag & TEX_NEGALPHA) {
    texres->trgba[3] = 1.0f - texres->trgba[3];
  }

  /* de-premul, this is being pre-multiplied in shade_input_do_shade() */
  /* do not de-premul for generated alpha, it is already in straight */
  if (texres->trgba[3] != 1.0f && texres->trgba[3] > 1e-4f && !(tex->imaflag & TEX_CALCALPHA)) {
    mul_v3_fl(texres->trgba, 1.0f / texres->trgba[3]);
  }

  if (ima) {
    BKE_image_pool_release_ibuf(ima, ibuf, pool);
  }

  BRICONTRGB;

  return retval;
}

void image_sample(
    Image *ima, float fx, float fy, float dx, float dy, float result[4], ImagePool *pool)
{
  TexResult texres;
  ImBuf *ibuf = BKE_image_pool_acquire_ibuf(ima, nullptr, pool);

  if (UNLIKELY(ibuf == nullptr)) {
    zero_v4(result);
    return;
  }

  texres.talpha = true; /* boxsample expects to be initialized */
  boxsample(ibuf, fx, fy, fx + dx, fy + dy, &texres, 0, 1);
  copy_v4_v4(result, texres.trgba);

  ima->flag |= IMA_USED_FOR_RENDER;

  BKE_image_pool_release_ibuf(ima, ibuf, pool);
}

void ibuf_sample(ImBuf *ibuf, float fx, float fy, float dx, float dy, float result[4])
{
  TexResult texres = {0};
  afdata_t AFD;

  AFD.dxt[0] = dx;
  AFD.dxt[1] = dx;
  AFD.dyt[0] = dy;
  AFD.dyt[1] = dy;
  // copy_v2_v2(AFD.dxt, dx);
  // copy_v2_v2(AFD.dyt, dy);

  AFD.intpol = 1;
  AFD.extflag = TXC_EXTD;

  ewa_eval(&texres, ibuf, fx, fy, &AFD);

  copy_v4_v4(result, texres.trgba);
}
