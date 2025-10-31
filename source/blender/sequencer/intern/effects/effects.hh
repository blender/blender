/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLF_enums.hh"

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

struct SeqRenderState;
struct RenderData;

enum class StripEarlyOut {
  NoInput = -1,  /* No input needed. */
  DoEffect = 0,  /* No early out (do the effect). */
  UseInput1 = 1, /* Output = input1. */
  UseInput2 = 2, /* Output = input2. */
};

struct EffectHandle {
  /* constructors & destructor */
  /* init is _only_ called on first creation */
  void (*init)(Strip *strip);

  /* number of input strips needed
   * (called directly after construction) */
  int (*num_inputs)();

  /* load is called first time after readblenfile in
   * get_sequence_effect automatically */
  void (*load)(Strip *seqconst);

  /* duplicate */
  void (*copy)(Strip *dst, const Strip *src, int flag);

  /* destruct */
  void (*free)(Strip *strip, bool do_id_user);

  StripEarlyOut (*early_out)(const Strip *strip, float fac);

  /* execute the effect */
  ImBuf *(*execute)(const RenderData *context,
                    SeqRenderState *state,
                    Strip *strip,
                    float timeline_frame,
                    float fac,
                    ImBuf *ibuf1,
                    ImBuf *ibuf2);
};

/** Get the effect handle for a given strip, and load the strip if it has not been loaded already.
 * If `strip` is not an effect strip, returns empty `EffectHandle`. */
EffectHandle strip_effect_handle_get(Strip *strip);

EffectHandle strip_blend_mode_handle_get(Strip *strip);
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

Array<float> make_gaussian_blur_kernel(float rad, int size);

inline float4 load_premul_pixel(const uchar *ptr)
{
  float4 res;
  straight_uchar_to_premul_float(res, ptr);
  return res;
}

inline float4 load_premul_pixel(const float *ptr)
{
  return float4(ptr);
}

inline void store_premul_pixel(const float4 &pix, uchar *dst)
{
  premul_float_to_straight_uchar(dst, pix);
}

inline void store_premul_pixel(const float4 &pix, float *dst)
{
  *reinterpret_cast<float4 *>(dst) = pix;
}

StripEarlyOut early_out_mul_input1(const Strip * /*strip*/, float fac);
StripEarlyOut early_out_mul_input2(const Strip * /*strip*/, float fac);
StripEarlyOut early_out_fade(const Strip * /*strip*/, float fac);

EffectHandle effect_handle_get(StripType strip_type);

float effect_fader_calc(Scene *scene, Strip *strip, float timeline_frame);

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
  threading::parallel_for(IndexRange(size_t(dst->x) * dst->y), 32 * 1024, [&](IndexRange range) {
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
int text_effect_font_init(const RenderData *context, const Strip *strip, FontFlags font_flags);

}  // namespace blender::seq
