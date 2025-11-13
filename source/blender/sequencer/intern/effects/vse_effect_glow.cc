/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "SEQ_render.hh"

#include "effects.hh"

namespace blender::seq {

static void glow_blur_bitmap(
    const float4 *src, float4 *map, int width, int height, float blur, int quality)
{
  using namespace blender;

  /* If we're not really blurring, bail out */
  if (blur <= 0) {
    return;
  }

  /* If result would be no blurring, early out. */
  const int halfWidth = ((quality + 1) * blur);
  if (halfWidth == 0) {
    return;
  }

  Array<float4> temp(width * height);

  /* Initialize the gaussian filter.
   * TODO: use code from #RE_filter_value. */
  Array<float> filter(halfWidth * 2);
  const float k = -1.0f / (2.0f * float(M_PI) * blur * blur);
  float weight = 0;
  for (int ix = 0; ix < halfWidth; ix++) {
    weight = exp(k * (ix * ix));
    filter[halfWidth - ix] = weight;
    filter[halfWidth + ix] = weight;
  }
  filter[0] = weight;
  /* Normalize the array */
  float fval = 0;
  for (int ix = 0; ix < halfWidth * 2; ix++) {
    fval += filter[ix];
  }
  for (int ix = 0; ix < halfWidth * 2; ix++) {
    filter[ix] /= fval;
  }

  /* Blur the rows: read map, write temp */
  threading::parallel_for(IndexRange(height), 32, [&](const IndexRange y_range) {
    for (const int y : y_range) {
      for (int x = 0; x < width; x++) {
        float4 curColor = float4(0.0f);
        int xmin = math::max(x - halfWidth, 0);
        int xmax = math::min(x + halfWidth, width);
        for (int nx = xmin, index = (xmin - x) + halfWidth; nx < xmax; nx++, index++) {
          curColor += map[nx + y * width] * filter[index];
        }
        temp[x + y * width] = curColor;
      }
    }
  });

  /* Blur the columns: read temp, write map */
  threading::parallel_for(IndexRange(width), 32, [&](const IndexRange x_range) {
    const float4 one = float4(1.0f);
    for (const int x : x_range) {
      for (int y = 0; y < height; y++) {
        float4 curColor = float4(0.0f);
        int ymin = math::max(y - halfWidth, 0);
        int ymax = math::min(y + halfWidth, height);
        for (int ny = ymin, index = (ymin - y) + halfWidth; ny < ymax; ny++, index++) {
          curColor += temp[x + ny * width] * filter[index];
        }
        if (src != nullptr) {
          curColor = math::min(one, src[x + y * width] + curColor);
        }
        map[x + y * width] = curColor;
      }
    }
  });
}

static void blur_isolate_highlights(const float4 *in,
                                    float4 *out,
                                    int width,
                                    int height,
                                    float threshold,
                                    float boost,
                                    float clamp)
{
  using namespace blender;
  threading::parallel_for(IndexRange(height), 64, [&](const IndexRange y_range) {
    const float4 clampv = float4(clamp);
    for (const int y : y_range) {
      int index = y * width;
      for (int x = 0; x < width; x++, index++) {

        /* Isolate the intensity */
        float intensity = (in[index].x + in[index].y + in[index].z - threshold);
        float4 val;
        if (intensity > 0) {
          val = math::min(clampv, in[index] * (boost * intensity));
        }
        else {
          val = float4(0.0f);
        }
        out[index] = val;
      }
    }
  });
}

static void init_glow_effect(Strip *strip)
{
  MEM_SAFE_FREE(strip->effectdata);
  GlowVars *data = MEM_callocN<GlowVars>("glowvars");
  strip->effectdata = data;
  data->fMini = 0.25f;
  data->fClamp = 1.0f;
  data->fBoost = 0.5f;
  data->dDist = 3.0f;
  data->dQuality = 3;
  data->bNoComp = 0;
}

static int num_inputs_glow()
{
  return 1;
}

static void do_glow_effect_byte(Strip *strip,
                                int render_size,
                                float fac,
                                int x,
                                int y,
                                uchar *rect1,
                                uchar * /*rect2*/,
                                uchar *out)
{
  using namespace blender;
  GlowVars *glow = (GlowVars *)strip->effectdata;

  Array<float4> inbuf(x * y);
  Array<float4> outbuf(x * y);

  using namespace blender;
  IMB_colormanagement_transform_byte_to_float(*inbuf.data(), rect1, x, y, 4, "sRGB", "sRGB");

  blur_isolate_highlights(
      inbuf.data(), outbuf.data(), x, y, glow->fMini * 3.0f, glow->fBoost * fac, glow->fClamp);
  glow_blur_bitmap(glow->bNoComp ? nullptr : inbuf.data(),
                   outbuf.data(),
                   x,
                   y,
                   glow->dDist * (render_size / 100.0f),
                   glow->dQuality);

  threading::parallel_for(IndexRange(y), 64, [&](const IndexRange y_range) {
    size_t offset = y_range.first() * x;
    IMB_buffer_byte_from_float(out + offset * 4,
                               *(outbuf.data() + offset),
                               4,
                               0.0f,
                               IB_PROFILE_SRGB,
                               IB_PROFILE_SRGB,
                               true,
                               x,
                               y_range.size(),
                               x,
                               x);
  });
}

static void do_glow_effect_float(Strip *strip,
                                 int render_size,
                                 float fac,
                                 int x,
                                 int y,
                                 float *rect1,
                                 float * /*rect2*/,
                                 float *out)
{
  using namespace blender;
  float4 *outbuf = reinterpret_cast<float4 *>(out);
  float4 *inbuf = reinterpret_cast<float4 *>(rect1);
  GlowVars *glow = (GlowVars *)strip->effectdata;

  blur_isolate_highlights(
      inbuf, outbuf, x, y, glow->fMini * 3.0f, glow->fBoost * fac, glow->fClamp);
  glow_blur_bitmap(glow->bNoComp ? nullptr : inbuf,
                   outbuf,
                   x,
                   y,
                   glow->dDist * (render_size / 100.0f),
                   glow->dQuality);
}

static ImBuf *do_glow_effect(const RenderData *context,
                             SeqRenderState * /*state*/,
                             Strip *strip,
                             float /*timeline_frame*/,
                             float fac,
                             ImBuf *ibuf1,
                             ImBuf *ibuf2)
{
  ImBuf *out = prepare_effect_imbufs(context, ibuf1, ibuf2);

  int render_size = 100 * context->rectx / context->scene->r.xsch;

  if (out->float_buffer.data) {
    do_glow_effect_float(strip,
                         render_size,
                         fac,
                         context->rectx,
                         context->recty,
                         ibuf1->float_buffer.data,
                         nullptr,
                         out->float_buffer.data);
  }
  else {
    do_glow_effect_byte(strip,
                        render_size,
                        fac,
                        context->rectx,
                        context->recty,
                        ibuf1->byte_buffer.data,
                        nullptr,
                        out->byte_buffer.data);
  }

  return out;
}

void glow_effect_get_handle(EffectHandle &rval)
{
  rval.init = init_glow_effect;
  rval.num_inputs = num_inputs_glow;
  rval.execute = do_glow_effect;
}

}  // namespace blender::seq
