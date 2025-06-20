/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_array.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"
#include "IMB_imbuf_types.hh"
#include "SEQ_effects.hh"

struct ImBuf;
struct Scene;
struct Strip;

namespace blender::seq {

EffectHandle strip_effect_get_sequence_blend(Strip *strip);
/**
 * Build frame map when speed in mode #SEQ_SPEED_MULTIPLY is animated.
 * This is, because `target_frame` value is integrated over time.
 */
void strip_effect_speed_rebuild_map(Scene *scene, Strip *strip);
/**
 * Override timeline_frame when rendering speed effect input.
 */
float strip_speed_effect_target_frame_get(Scene *scene,
                                          Strip *strip_speed,
                                          float timeline_frame,
                                          int input);

ImBuf *prepare_effect_imbufs(const RenderData *context,
                             ImBuf *ibuf1,
                             ImBuf *ibuf2,
                             bool uninitialized_pixels = true);

blender::Array<float> make_gaussian_blur_kernel(float rad, int size);

inline blender::float4 load_premul_pixel(const uchar *ptr)
{
  blender::float4 res;
  straight_uchar_to_premul_float(res, ptr);
  return res;
}

inline blender::float4 load_premul_pixel(const float *ptr)
{
  return blender::float4(ptr);
}

inline void store_premul_pixel(const blender::float4 &pix, uchar *dst)
{
  premul_float_to_straight_uchar(dst, pix);
}

inline void store_premul_pixel(const blender::float4 &pix, float *dst)
{
  *reinterpret_cast<blender::float4 *>(dst) = pix;
}

inline void store_opaque_black_pixel(uchar *dst)
{
  dst[0] = 0;
  dst[1] = 0;
  dst[2] = 0;
  dst[3] = 255;
}

inline void store_opaque_black_pixel(float *dst)
{
  dst[0] = 0.0f;
  dst[1] = 0.0f;
  dst[2] = 0.0f;
  dst[3] = 1.0f;
}

StripEarlyOut early_out_mul_input1(const Strip * /*seq*/, float fac);
StripEarlyOut early_out_mul_input2(const Strip * /*seq*/, float fac);
StripEarlyOut early_out_fade(const Strip * /*seq*/, float fac);
void get_default_fac_fade(const Scene *scene,
                          const Strip *strip,
                          float timeline_frame,
                          float *fac);

EffectHandle effect_handle_get(int strip_type);

void add_effect_get_handle(EffectHandle &rval);
void adjustment_effect_get_handle(EffectHandle &rval);
void alpha_over_effect_get_handle(EffectHandle &rval);
void alpha_under_effect_get_handle(EffectHandle &rval);
void blend_mode_effect_get_handle(EffectHandle &rval);
void color_mix_effect_get_handle(EffectHandle &rval);
void cross_effect_get_handle(EffectHandle &rval);
void gamma_cross_effect_get_handle(EffectHandle &rval);
void gaussian_blur_effect_get_handle(EffectHandle &rval);
void glow_effect_get_handle(EffectHandle &rval);
void mul_effect_get_handle(EffectHandle &rval);
void multi_camera_effect_get_handle(EffectHandle &rval);
void solid_color_effect_get_handle(EffectHandle &rval);
void speed_effect_get_handle(EffectHandle &rval);
void sub_effect_get_handle(EffectHandle &rval);
void text_effect_get_handle(EffectHandle &rval);
void transform_effect_get_handle(EffectHandle &rval);
void wipe_effect_get_handle(EffectHandle &rval);

/* Given `OpT` that implements an `apply` function:
 *
 *    template <typename T>
 *    void apply(const T *src1, const T *src2, T *dst, int64_t size) const;
 *
 * this function calls the apply() function in parallel
 * chunks of the image to process, and with uchar or float types
 * All images are expected to have 4 (RGBA) color channels. */
template<typename OpT>
static void apply_effect_op(const OpT &op, const ImBuf *src1, const ImBuf *src2, ImBuf *dst)
{
  BLI_assert_msg(src1->channels == 0 || src1->channels == 4,
                 "Sequencer only supports 4 channel images");
  BLI_assert_msg(src2->channels == 0 || src2->channels == 4,
                 "Sequencer only supports 4 channel images");
  BLI_assert_msg(dst->channels == 0 || dst->channels == 4,
                 "Sequencer only supports 4 channel images");
  blender::threading::parallel_for(
      blender::IndexRange(size_t(dst->x) * dst->y), 32 * 1024, [&](blender::IndexRange range) {
        int64_t offset = range.first() * 4;
        if (dst->float_buffer.data) {
          const float *src1_ptr = src1->float_buffer.data + offset;
          const float *src2_ptr = src2->float_buffer.data + offset;
          float *dst_ptr = dst->float_buffer.data + offset;
          op.apply(src1_ptr, src2_ptr, dst_ptr, range.size());
        }
        else {
          const uchar *src1_ptr = src1->byte_buffer.data + offset;
          const uchar *src2_ptr = src2->byte_buffer.data + offset;
          uchar *dst_ptr = dst->byte_buffer.data + offset;
          op.apply(src1_ptr, src2_ptr, dst_ptr, range.size());
        }
      });
}

TextVarsRuntime *text_effect_calc_runtime(const Strip *strip, int font, const int2 image_size);
int text_effect_font_init(const RenderData *context, const Strip *strip, int font_flags);

}  // namespace blender::seq
