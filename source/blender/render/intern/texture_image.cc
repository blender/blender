/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "DNA_image_types.h"
#include "DNA_texture_types.h"

#include "BLI_math_interp.hh"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_image.hh"

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
  const int64_t ofs = int64_t(y) * ibuf->x + x;

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

    /* Bytes are internally straight, however render pipeline seems to expect pre-multiplied. */
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
    std::swap(fx, fy);
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

  /* De-pre-multiply, this is being pre-multiplied in #shade_input_do_shade()
   * do not de-pre-multiply for generated alpha, it is already in straight. */
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
        rf->xmax = std::min(rf->xmax, x2);
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
        rf->xmin = std::max(rf->xmin, x1);
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
        rf->ymax = std::min(rf->ymax, y2);
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
        rf->ymin = std::max(rf->ymin, y1);
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

  rf->xmin = std::max(rf->xmin, x1);
  rf->xmax = std::min(rf->xmax, x2);
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

  rf->ymin = std::max(rf->ymin, y1);
  rf->ymax = std::min(rf->ymax, y2);

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

  startx = std::max(startx, 0);
  starty = std::max(starty, 0);
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
    /* Pre-multiply it all. */
    texres->trgba[0] *= alphaclip;
    texres->trgba[1] *= alphaclip;
    texres->trgba[2] *= alphaclip;
    texres->trgba[3] *= alphaclip;
  }
}

/* -------------------------------------------------------------------- */
/* from here, some functions only used for the new filtering */

/* anisotropic filters, data struct used instead of long line of (possibly unused) func args */
struct AFData {
  float dxt[2], dyt[2];
  int intpol, extflag;
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
    default: {            /* as extend, if clipped, set alpha to 0.0 */
      x = std::max(x, 0); /* TXF alpha: clip = 1; } */
      if (x >= ibuf->x) {
        x = ibuf->x - 1;
      } /* TXF alpha: clip = 1; } */
      y = std::max(y, 0); /* TXF alpha: clip = 1; } */
      if (y >= ibuf->y) {
        y = ibuf->y - 1;
      } /* TXF alpha: clip = 1; } */
    }
  }

  if (ibuf->float_buffer.data) {
    const float *fp = ibuf->float_buffer.data + (x + int64_t(y) * ibuf->x) * ibuf->channels;
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
    const uchar *rect = ibuf->byte_buffer.data + 4 * (x + int64_t(y) * ibuf->x);
    float inv_alpha_fac = (1.0f / 255.0f) * rect[3] * (1.0f / 255.0f);
    col[0] = rect[0] * inv_alpha_fac;
    col[1] = rect[1] * inv_alpha_fac;
    col[2] = rect[2] * inv_alpha_fac;
    col[3] = clip ? 0.0f : rect[3] * (1.0f / 255.0f);
  }
  return clip;
}

struct ReadEWAData {
  ImBuf *ibuf;
  const AFData *AFD;
};

static void ewa_read_pixel_cb(void *userdata, int x, int y, float result[4])
{
  ReadEWAData *data = (ReadEWAData *)userdata;
  ibuf_get_color_clip(result, data->ibuf, x, y, data->AFD->extflag);
}

static void ewa_eval(TexResult *texr, ImBuf *ibuf, float fx, float fy, const AFData *AFD)
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

#undef EWA_MAXIDX

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
  AFData AFD;

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
