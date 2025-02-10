/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <algorithm>

#include "BLI_task.hh"

#include "DNA_sequence_types.h"

#include "IMB_imbuf.hh"

#include "SEQ_render.hh"

#include "effects.hh"

using namespace blender;

struct WipeZone {
  float angle;
  int flip;
  int xo, yo;
  int width;
  float pythangle;
  float clockWidth;
  int type;
  bool forward;
};

static WipeZone precalc_wipe_zone(const WipeVars *wipe, int xo, int yo)
{
  WipeZone zone;
  zone.flip = (wipe->angle < 0.0f);
  zone.angle = tanf(fabsf(wipe->angle));
  zone.xo = xo;
  zone.yo = yo;
  zone.width = int(wipe->edgeWidth * ((xo + yo) / 2.0f));
  zone.pythangle = 1.0f / sqrtf(zone.angle * zone.angle + 1.0f);
  zone.clockWidth = wipe->edgeWidth * float(M_PI);
  zone.type = wipe->wipetype;
  zone.forward = wipe->forward != 0;
  return zone;
}

/**
 * This function calculates the blur band for the wipe effects.
 */
static float in_band(float width, float dist, int side, int dir)
{
  float alpha;

  if (width == 0) {
    return float(side);
  }

  if (width < dist) {
    return float(side);
  }

  if (side == 1) {
    alpha = (dist + 0.5f * width) / (width);
  }
  else {
    alpha = (0.5f * width - dist) / (width);
  }

  if (dir == 0) {
    alpha = 1 - alpha;
  }

  return alpha;
}

static float check_zone(const WipeZone *wipezone, int x, int y, float fac)
{
  float posx, posy, hyp, hyp2, angle, hwidth, b1, b2, b3, pointdist;
  float temp1, temp2, temp3, temp4; /* some placeholder variables */
  int xo = wipezone->xo;
  int yo = wipezone->yo;
  float halfx = xo * 0.5f;
  float halfy = yo * 0.5f;
  float widthf, output = 0;
  int width;

  if (wipezone->flip) {
    x = xo - x;
  }
  angle = wipezone->angle;

  if (wipezone->forward) {
    posx = fac * xo;
    posy = fac * yo;
  }
  else {
    posx = xo - fac * xo;
    posy = yo - fac * yo;
  }

  switch (wipezone->type) {
    case DO_SINGLE_WIPE:
      width = min_ii(wipezone->width, fac * yo);
      width = min_ii(width, yo - fac * yo);

      if (angle == 0.0f) {
        b1 = posy;
        b2 = y;
        hyp = fabsf(y - posy);
      }
      else {
        b1 = posy - (-angle) * posx;
        b2 = y - (-angle) * x;
        hyp = fabsf(angle * x + y + (-posy - angle * posx)) * wipezone->pythangle;
      }

      if (angle < 0) {
        temp1 = b1;
        b1 = b2;
        b2 = temp1;
      }

      if (wipezone->forward) {
        if (b1 < b2) {
          output = in_band(width, hyp, 1, 1);
        }
        else {
          output = in_band(width, hyp, 0, 1);
        }
      }
      else {
        if (b1 < b2) {
          output = in_band(width, hyp, 0, 1);
        }
        else {
          output = in_band(width, hyp, 1, 1);
        }
      }
      break;

    case DO_DOUBLE_WIPE:
      if (!wipezone->forward) {
        fac = 1.0f - fac; /* Go the other direction */
      }

      width = wipezone->width; /* calculate the blur width */
      hwidth = width * 0.5f;
      if (angle == 0) {
        b1 = posy * 0.5f;
        b3 = yo - posy * 0.5f;
        b2 = y;

        hyp = fabsf(y - posy * 0.5f);
        hyp2 = fabsf(y - (yo - posy * 0.5f));
      }
      else {
        b1 = posy * 0.5f - (-angle) * posx * 0.5f;
        b3 = (yo - posy * 0.5f) - (-angle) * (xo - posx * 0.5f);
        b2 = y - (-angle) * x;

        hyp = fabsf(angle * x + y + (-posy * 0.5f - angle * posx * 0.5f)) * wipezone->pythangle;
        hyp2 = fabsf(angle * x + y + (-(yo - posy * 0.5f) - angle * (xo - posx * 0.5f))) *
               wipezone->pythangle;
      }

      hwidth = min_ff(hwidth, fabsf(b3 - b1) / 2.0f);

      if (b2 < b1 && b2 < b3) {
        output = in_band(hwidth, hyp, 0, 1);
      }
      else if (b2 > b1 && b2 > b3) {
        output = in_band(hwidth, hyp2, 0, 1);
      }
      else {
        if (hyp < hwidth && hyp2 > hwidth) {
          output = in_band(hwidth, hyp, 1, 1);
        }
        else if (hyp > hwidth && hyp2 < hwidth) {
          output = in_band(hwidth, hyp2, 1, 1);
        }
        else {
          output = in_band(hwidth, hyp2, 1, 1) * in_band(hwidth, hyp, 1, 1);
        }
      }
      if (!wipezone->forward) {
        output = 1 - output;
      }
      break;
    case DO_CLOCK_WIPE:
      /*
       * temp1: angle of effect center in rads
       * temp2: angle of line through (halfx, halfy) and (x, y) in rads
       * temp3: angle of low side of blur
       * temp4: angle of high side of blur
       */
      output = 1.0f - fac;
      widthf = wipezone->clockWidth;
      temp1 = 2.0f * float(M_PI) * fac;

      if (wipezone->forward) {
        temp1 = 2.0f * float(M_PI) - temp1;
      }

      x = x - halfx;
      y = y - halfy;

      temp2 = atan2f(y, x);
      if (temp2 < 0.0f) {
        temp2 += 2.0f * float(M_PI);
      }

      if (wipezone->forward) {
        temp3 = temp1 - widthf * fac;
        temp4 = temp1 + widthf * (1 - fac);
      }
      else {
        temp3 = temp1 - widthf * (1 - fac);
        temp4 = temp1 + widthf * fac;
      }
      temp3 = std::max<float>(temp3, 0);
      temp4 = std::min(temp4, 2.0f * float(M_PI));

      if (temp2 < temp3) {
        output = 0;
      }
      else if (temp2 > temp4) {
        output = 1;
      }
      else {
        output = (temp2 - temp3) / (temp4 - temp3);
      }
      if (x == 0 && y == 0) {
        output = 1;
      }
      if (output != output) {
        output = 1;
      }
      if (wipezone->forward) {
        output = 1 - output;
      }
      break;
    case DO_IRIS_WIPE:
      if (xo > yo) {
        yo = xo;
      }
      else {
        xo = yo;
      }

      if (!wipezone->forward) {
        fac = 1 - fac;
      }

      width = wipezone->width;
      hwidth = width * 0.5f;

      temp1 = (halfx - (halfx)*fac);
      pointdist = hypotf(temp1, temp1);

      temp2 = hypotf(halfx - x, halfy - y);
      if (temp2 > pointdist) {
        output = in_band(hwidth, fabsf(temp2 - pointdist), 0, 1);
      }
      else {
        output = in_band(hwidth, fabsf(temp2 - pointdist), 1, 1);
      }

      if (!wipezone->forward) {
        output = 1 - output;
      }

      break;
  }
  if (output < 0) {
    output = 0;
  }
  else if (output > 1) {
    output = 1;
  }
  return output;
}

static void init_wipe_effect(Strip *strip)
{
  if (strip->effectdata) {
    MEM_freeN(strip->effectdata);
  }

  strip->effectdata = MEM_callocN(sizeof(WipeVars), "wipevars");
}

static int num_inputs_wipe()
{
  return 2;
}

static void free_wipe_effect(Strip *strip, const bool /*do_id_user*/)
{
  MEM_SAFE_FREE(strip->effectdata);
}

static void copy_wipe_effect(Strip *dst, const Strip *src, const int /*flag*/)
{
  dst->effectdata = MEM_dupallocN(src->effectdata);
}

template<typename T>
static void do_wipe_effect(
    const Strip *strip, float fac, int width, int height, const T *rect1, const T *rect2, T *out)
{
  using namespace blender;
  const WipeVars *wipe = (const WipeVars *)strip->effectdata;
  const WipeZone wipezone = precalc_wipe_zone(wipe, width, height);

  threading::parallel_for(IndexRange(height), 64, [&](const IndexRange y_range) {
    const T *cp1 = rect1 ? rect1 + y_range.first() * width * 4 : nullptr;
    const T *cp2 = rect2 ? rect2 + y_range.first() * width * 4 : nullptr;
    T *rt = out + y_range.first() * width * 4;
    for (const int y : y_range) {
      for (int x = 0; x < width; x++) {
        float check = check_zone(&wipezone, x, y, fac);
        if (check) {
          if (cp1) {
            float4 col1 = load_premul_pixel(cp1);
            float4 col2 = load_premul_pixel(cp2);
            float4 col = col1 * check + col2 * (1.0f - check);
            store_premul_pixel(col, rt);
          }
          else {
            store_opaque_black_pixel(rt);
          }
        }
        else {
          if (cp2) {
            memcpy(rt, cp2, sizeof(T) * 4);
          }
          else {
            store_opaque_black_pixel(rt);
          }
        }

        rt += 4;
        if (cp1 != nullptr) {
          cp1 += 4;
        }
        if (cp2 != nullptr) {
          cp2 += 4;
        }
      }
    }
  });
}

static ImBuf *do_wipe_effect(const SeqRenderData *context,
                             Strip *strip,
                             float /*timeline_frame*/,
                             float fac,
                             ImBuf *ibuf1,
                             ImBuf *ibuf2)
{
  ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2);

  if (out->float_buffer.data) {
    do_wipe_effect(strip,
                   fac,
                   context->rectx,
                   context->recty,
                   ibuf1->float_buffer.data,
                   ibuf2->float_buffer.data,
                   out->float_buffer.data);
  }
  else {
    do_wipe_effect(strip,
                   fac,
                   context->rectx,
                   context->recty,
                   ibuf1->byte_buffer.data,
                   ibuf2->byte_buffer.data,
                   out->byte_buffer.data);
  }

  return out;
}

void wipe_effect_get_handle(SeqEffectHandle &rval)
{
  rval.init = init_wipe_effect;
  rval.num_inputs = num_inputs_wipe;
  rval.free = free_wipe_effect;
  rval.copy = copy_wipe_effect;
  rval.early_out = early_out_fade;
  rval.get_default_fac = get_default_fac_fade;
  rval.execute = do_wipe_effect;
}
