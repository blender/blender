/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <algorithm>

#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "DNA_sequence_types.h"

#include "IMB_imbuf.hh"

#include "SEQ_render.hh"

#include "effects.hh"

namespace blender::seq {

struct WipeData {
  WipeData(const WipeVars *wipe, int width, int height, float fac)
  {
    this->type = eEffectWipeType(wipe->wipetype);
    this->forward = wipe->forward != 0;
    this->size = float2(width, height);

    if (this->type == SEQ_WIPE_SINGLE) {
      /* Position that the wipe line goes through: moves along
       * the image diagonal. The other diagonal when angle is negative. */
      this->pos = this->size * (this->forward ? fac : (1.0f - fac));
      if (wipe->angle < 0.0f) {
        this->pos.x = this->size.x - this->pos.x;
      }
    }
    if (this->type == SEQ_WIPE_DOUBLE) {
      /* For double blend, position goes from center of screen
       * along the diagonal. The other blend line position will be
       * a mirror of it. */
      float2 offset = this->size * (this->forward ? (1.0f - fac) : fac) * 0.5f;
      if (wipe->angle < 0.0f) {
        offset.x = -offset.x;
      }
      this->pos = this->size * 0.5f + offset;
    }

    /* Line direction: (cos(a), sin(a)). Perpendicular: (-sin(a), cos(a)).
     * Angle is negative to match previous behavior. */
    this->normal.x = -sinf(-wipe->angle);
    this->normal.y = cosf(-wipe->angle);

    /* Blend zone width. */
    float blend_width = wipe->edgeWidth * ((width + height) / 2.0f);
    if (ELEM(this->type, SEQ_WIPE_DOUBLE, SEQ_WIPE_IRIS)) {
      blend_width *= 0.5f;
    }
    /* For single/double wipes, make sure the blend zone goes to zero at start & end
     * of transition. */
    if (ELEM(this->type, SEQ_WIPE_SINGLE, SEQ_WIPE_DOUBLE)) {
      blend_width = std::min(blend_width, fac * this->size.y);
      blend_width = std::min(blend_width, this->size.y - fac * this->size.y);
    }
    this->blend_width_inv = math::safe_rcp(blend_width);

    if (this->type == SEQ_WIPE_IRIS) {
      /* Distance to Iris circle at current factor. */
      float2 iris = this->size * 0.5f * (this->forward ? (1.0f - fac) : fac);
      this->iris_dist = math::length(iris);
    }

    if (this->type == SEQ_WIPE_CLOCK) {
      float angle_cur = 2.0f * float(M_PI) * (this->forward ? (1.0f - fac) : fac);
      float angle_width = wipe->edgeWidth * float(M_PI);
      float delta_neg = angle_width * (this->forward ? fac : (1.0f - fac));
      float delta_pos = angle_width * (this->forward ? (1.0f - fac) : fac);
      this->clock_angles.x = std::max(angle_cur - delta_neg, 0.0f);
      this->clock_angles.y = std::min(angle_cur + delta_pos, 2.0f * float(M_PI));
      this->clock_angle_inv_dif = math::safe_rcp(this->clock_angles.y - this->clock_angles.x);
    }
  }

  float2 size;   /* Image size. */
  float2 pos;    /* Position that wipe line goes through. */
  float2 normal; /* Normal vector to single/double wipe line. */
  float blend_width_inv = 0.0f;
  float iris_dist = 0.0f;
  float2 clock_angles; /* Min, max clock angles at current factor. */
  float clock_angle_inv_dif = 0.0f;
  eEffectWipeType type;
  bool forward = false;
};

static float calc_wipe_band(float dist, float inv_width)
{
  if (inv_width == 0.0f) {
    return dist < 0.0f ? 0.0f : 1.0f;
  }
  return dist * inv_width + 0.5f;
}

static float calc_wipe_blend(const WipeData *data, int x, int y)
{
  float output = 0.0f;
  switch (data->type) {
    case SEQ_WIPE_SINGLE: {
      /* Distance to line: dot(pixel_pos - line_pos, line_normal). */
      float dist = math::dot(float2(x, y) - data->pos, data->normal);
      output = calc_wipe_band(dist, data->blend_width_inv);
    } break;

    case SEQ_WIPE_DOUBLE: {
      /* Distance to line: dot(pixel_pos - line_pos, line_normal).
       * For double wipe, we have two lines to calculate the distance to. */
      float2 pos1 = data->pos;
      float2 pos2 = data->size - data->pos;
      float dist1 = math::dot(float2(x, y) - pos1, -data->normal);
      float dist2 = math::dot(float2(x, y) - pos2, data->normal);
      float dist = std::min(dist1, dist2);
      output = calc_wipe_band(dist, data->blend_width_inv);
    } break;

    case SEQ_WIPE_CLOCK: {
      float2 offset = float2(x, y) - data->size * 0.5f;
      if (math::length_squared(offset) < 1.0e-3f) {
        output = 0.0f;
      }
      else {
        float angle;
        angle = atan2f(offset.y, offset.x);
        if (angle < 0.0f) {
          angle += 2.0f * float(M_PI);
        }
        if (angle < data->clock_angles.x) {
          output = 1;
        }
        else if (angle > data->clock_angles.y) {
          output = 0;
        }
        else {
          output = (data->clock_angles.y - angle) * data->clock_angle_inv_dif;
        }
      }
    } break;

    case SEQ_WIPE_IRIS: {
      float dist = math::distance(float2(x, y), data->size * 0.5f);
      output = calc_wipe_band(data->iris_dist - dist, data->blend_width_inv);
    } break;
  }
  if (!data->forward) {
    output = 1.0f - output;
  }
  return output;
}

static void init_wipe_effect(Strip *strip)
{
  MEM_SAFE_FREE(strip->effectdata);
  strip->effectdata = MEM_callocN<WipeVars>("wipevars");
}

static int num_inputs_wipe()
{
  return 2;
}

template<typename T>
static void do_wipe_effect(
    const Strip *strip, float fac, int width, int height, const T *rect1, const T *rect2, T *out)
{
  using namespace blender;
  const WipeVars *wipe = (const WipeVars *)strip->effectdata;

  const WipeData data(wipe, width, height, fac);

  threading::parallel_for(IndexRange(height), 64, [&](const IndexRange y_range) {
    const T *cp1 = rect1 + y_range.first() * width * 4;
    const T *cp2 = rect2 + y_range.first() * width * 4;
    T *rt = out + y_range.first() * width * 4;
    for (const int y : y_range) {
      for (int x = 0; x < width; x++) {
        float blend = calc_wipe_blend(&data, x, y);
        if (blend <= 0.0f) {
          memcpy(rt, cp2, sizeof(T) * 4);
        }
        else if (blend >= 1.0f) {
          memcpy(rt, cp1, sizeof(T) * 4);
        }
        else {
          float4 col1 = load_premul_pixel(cp1);
          float4 col2 = load_premul_pixel(cp2);
          float4 col = col1 * blend + col2 * (1.0f - blend);
          store_premul_pixel(col, rt);
        }

        rt += 4;
        cp1 += 4;
        cp2 += 4;
      }
    }
  });
}

static ImBuf *do_wipe_effect(const RenderData *context,
                             SeqRenderState * /*state*/,
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

void wipe_effect_get_handle(EffectHandle &rval)
{
  rval.init = init_wipe_effect;
  rval.num_inputs = num_inputs_wipe;
  rval.early_out = early_out_fade;
  rval.execute = do_wipe_effect;
}

}  // namespace blender::seq
