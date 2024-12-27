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
#include "SEQ_effects.hh"

struct ImBuf;
struct Scene;
struct Sequence;

SeqEffectHandle seq_effect_get_sequence_blend(Sequence *seq);
/**
 * Build frame map when speed in mode #SEQ_SPEED_MULTIPLY is animated.
 * This is, because `target_frame` value is integrated over time.
 */
void seq_effect_speed_rebuild_map(Scene *scene, Sequence *seq);
/**
 * Override timeline_frame when rendering speed effect input.
 */
float seq_speed_effect_target_frame_get(Scene *scene,
                                        Sequence *seq_speed,
                                        float timeline_frame,
                                        int input);

void slice_get_byte_buffers(const SeqRenderData *context,
                            const ImBuf *ibuf1,
                            const ImBuf *ibuf2,
                            const ImBuf *out,
                            int start_line,
                            uchar **rect1,
                            uchar **rect2,
                            uchar **rect_out);
void slice_get_float_buffers(const SeqRenderData *context,
                             const ImBuf *ibuf1,
                             const ImBuf *ibuf2,
                             const ImBuf *out,
                             int start_line,
                             float **rect1,
                             float **rect2,
                             float **rect_out);

ImBuf *prepare_effect_imbufs(const SeqRenderData *context,
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

StripEarlyOut early_out_mul_input1(const Sequence * /*seq*/, float fac);
StripEarlyOut early_out_mul_input2(const Sequence * /*seq*/, float fac);
StripEarlyOut early_out_fade(const Sequence * /*seq*/, float fac);
void get_default_fac_fade(const Scene *scene,
                          const Sequence *seq,
                          float timeline_frame,
                          float *fac);

SeqEffectHandle get_sequence_effect_impl(int seq_type);

void add_effect_get_handle(SeqEffectHandle &rval);
void adjustment_effect_get_handle(SeqEffectHandle &rval);
void alpha_over_effect_get_handle(SeqEffectHandle &rval);
void alpha_under_effect_get_handle(SeqEffectHandle &rval);
void blend_mode_effect_get_handle(SeqEffectHandle &rval);
void color_mix_effect_get_handle(SeqEffectHandle &rval);
void cross_effect_get_handle(SeqEffectHandle &rval);
void gamma_cross_effect_get_handle(SeqEffectHandle &rval);
void gaussian_blur_effect_get_handle(SeqEffectHandle &rval);
void glow_effect_get_handle(SeqEffectHandle &rval);
void mul_effect_get_handle(SeqEffectHandle &rval);
void multi_camera_effect_get_handle(SeqEffectHandle &rval);
void over_drop_effect_get_handle(SeqEffectHandle &rval);
void solid_color_effect_get_handle(SeqEffectHandle &rval);
void speed_effect_get_handle(SeqEffectHandle &rval);
void sub_effect_get_handle(SeqEffectHandle &rval);
void text_effect_get_handle(SeqEffectHandle &rval);
void transform_effect_get_handle(SeqEffectHandle &rval);
void wipe_effect_get_handle(SeqEffectHandle &rval);
