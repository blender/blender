/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_math_base.hh"
#include "BLI_task.hh"

#include "DNA_sequence_types.h"

#include "IMB_imbuf.hh"

#include "SEQ_render.hh"

#include "effects.hh"

namespace blender::seq {

static void init_gaussian_blur_effect(Strip *strip)
{
  MEM_SAFE_FREE(strip->effectdata);
  GaussianBlurVars *data = MEM_callocN<GaussianBlurVars>("gaussianblurvars");
  strip->effectdata = data;
  data->size_x = 9.0f;
  data->size_y = 9.0f;
}

static int num_inputs_gaussian_blur()
{
  return 1;
}

static StripEarlyOut early_out_gaussian_blur(const Strip *strip, float /*fac*/)
{
  GaussianBlurVars *data = static_cast<GaussianBlurVars *>(strip->effectdata);
  if (data->size_x == 0.0f && data->size_y == 0) {
    return StripEarlyOut::UseInput1;
  }
  return StripEarlyOut::DoEffect;
}

template<typename T>
static void gaussian_blur_x(const Span<float> gaussian,
                            int half_size,
                            int start_line,
                            int width,
                            int height,
                            int /*frame_height*/,
                            const T *rect,
                            T *dst)
{
  dst += int64_t(start_line) * width * 4;
  for (int y = start_line; y < start_line + height; y++) {
    for (int x = 0; x < width; x++) {
      float4 accum(0.0f);
      float accum_weight = 0.0f;

      int xmin = math::max(x - half_size, 0);
      int xmax = math::min(x + half_size, width - 1);
      for (int nx = xmin, index = (xmin - x) + half_size; nx <= xmax; nx++, index++) {
        float weight = gaussian[index];
        int offset = (y * width + nx) * 4;
        accum += float4(rect + offset) * weight;
        accum_weight += weight;
      }
      accum *= (1.0f / accum_weight);
      if constexpr (math::is_math_float_type<T>) {
        dst[0] = accum[0];
        dst[1] = accum[1];
        dst[2] = accum[2];
        dst[3] = accum[3];
      }
      else {
        dst[0] = accum[0] + 0.5f;
        dst[1] = accum[1] + 0.5f;
        dst[2] = accum[2] + 0.5f;
        dst[3] = accum[3] + 0.5f;
      }
      dst += 4;
    }
  }
}

template<typename T>
static void gaussian_blur_y(const Span<float> gaussian,
                            int half_size,
                            int start_line,
                            int width,
                            int height,
                            int frame_height,
                            const T *rect,
                            T *dst)
{
  dst += int64_t(start_line) * width * 4;
  for (int y = start_line; y < start_line + height; y++) {
    for (int x = 0; x < width; x++) {
      float4 accum(0.0f);
      float accum_weight = 0.0f;
      int ymin = math::max(y - half_size, 0);
      int ymax = math::min(y + half_size, frame_height - 1);
      for (int ny = ymin, index = (ymin - y) + half_size; ny <= ymax; ny++, index++) {
        float weight = gaussian[index];
        int offset = (ny * width + x) * 4;
        accum += float4(rect + offset) * weight;
        accum_weight += weight;
      }
      accum *= (1.0f / accum_weight);
      if constexpr (math::is_math_float_type<T>) {
        dst[0] = accum[0];
        dst[1] = accum[1];
        dst[2] = accum[2];
        dst[3] = accum[3];
      }
      else {
        dst[0] = accum[0] + 0.5f;
        dst[1] = accum[1] + 0.5f;
        dst[2] = accum[2] + 0.5f;
        dst[3] = accum[3] + 0.5f;
      }
      dst += 4;
    }
  }
}

static ImBuf *do_gaussian_blur_effect(const RenderData *context,
                                      SeqRenderState * /*state*/,
                                      Strip *strip,
                                      float /*timeline_frame*/,
                                      float /*fac*/,
                                      ImBuf *ibuf1,
                                      ImBuf * /*ibuf2*/)
{
  using namespace blender;

  /* Create blur kernel weights. */
  const GaussianBlurVars *data = static_cast<const GaussianBlurVars *>(strip->effectdata);

  const float size_scale = seq::get_render_scale_factor(*context);
  const float size_x = data->size_x * size_scale;
  const float size_y = data->size_y * size_scale;

  const int half_size_x = int(size_x + 0.5f);
  const int half_size_y = int(size_y + 0.5f);
  Array<float> gaussian_x = make_gaussian_blur_kernel(size_x, half_size_x);
  Array<float> gaussian_y = make_gaussian_blur_kernel(size_y, half_size_y);

  const int width = context->rectx;
  const int height = context->recty;
  const bool is_float = ibuf1->float_buffer.data;

  /* Horizontal blur: create output, blur ibuf1 into it. */
  ImBuf *out = prepare_effect_imbufs(context, ibuf1, nullptr);
  threading::parallel_for(IndexRange(context->recty), 32, [&](const IndexRange y_range) {
    const int y_first = y_range.first();
    const int y_size = y_range.size();
    if (is_float) {
      gaussian_blur_x(gaussian_x,
                      half_size_x,
                      y_first,
                      width,
                      y_size,
                      height,
                      ibuf1->float_buffer.data,
                      out->float_buffer.data);
    }
    else {
      gaussian_blur_x(gaussian_x,
                      half_size_x,
                      y_first,
                      width,
                      y_size,
                      height,
                      ibuf1->byte_buffer.data,
                      out->byte_buffer.data);
    }
  });

  /* Vertical blur: create output, blur previous output into it. */
  ibuf1 = out;
  out = prepare_effect_imbufs(context, ibuf1, nullptr);
  threading::parallel_for(IndexRange(context->recty), 32, [&](const IndexRange y_range) {
    const int y_first = y_range.first();
    const int y_size = y_range.size();
    if (is_float) {
      gaussian_blur_y(gaussian_y,
                      half_size_y,
                      y_first,
                      width,
                      y_size,
                      height,
                      ibuf1->float_buffer.data,
                      out->float_buffer.data);
    }
    else {
      gaussian_blur_y(gaussian_y,
                      half_size_y,
                      y_first,
                      width,
                      y_size,
                      height,
                      ibuf1->byte_buffer.data,
                      out->byte_buffer.data);
    }
  });

  /* Free the first output. */
  IMB_freeImBuf(ibuf1);

  return out;
}

void gaussian_blur_effect_get_handle(EffectHandle &rval)
{
  rval.init = init_gaussian_blur_effect;
  rval.num_inputs = num_inputs_gaussian_blur;
  rval.early_out = early_out_gaussian_blur;
  rval.execute = do_gaussian_blur_effect;
}

}  // namespace blender::seq
