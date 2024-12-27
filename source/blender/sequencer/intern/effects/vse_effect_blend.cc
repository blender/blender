/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_math_color_blend.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "IMB_imbuf.hh"

#include "SEQ_render.hh"

#include "effects.hh"

using namespace blender;

/* -------------------------------------------------------------------- */
/* Alpha Over Effect */

static void init_alpha_over_or_under(Sequence *seq)
{
  Sequence *seq1 = seq->seq1;
  Sequence *seq2 = seq->seq2;

  seq->seq2 = seq1;
  seq->seq1 = seq2;
}

static bool alpha_opaque(uchar alpha)
{
  return alpha == 255;
}

static bool alpha_opaque(float alpha)
{
  return alpha >= 1.0f;
}

/* dst = src1 over src2 (alpha from src1) */
template<typename T>
static void do_alphaover_effect(
    float fac, int width, int height, const T *src1, const T *src2, T *dst)
{
  if (fac <= 0.0f) {
    memcpy(dst, src2, sizeof(T) * 4 * width * height);
    return;
  }

  for (int pixel_idx = 0; pixel_idx < width * height; pixel_idx++) {
    if (src1[3] <= 0.0f) {
      /* Alpha of zero. No color addition will happen as the colors are pre-multiplied. */
      memcpy(dst, src2, sizeof(T) * 4);
    }
    else if (fac == 1.0f && alpha_opaque(src1[3])) {
      /* No change to `src1` as `fac == 1` and fully opaque. */
      memcpy(dst, src1, sizeof(T) * 4);
    }
    else {
      float4 col1 = load_premul_pixel(src1);
      float mfac = 1.0f - fac * col1.w;
      float4 col2 = load_premul_pixel(src2);
      float4 col = fac * col1 + mfac * col2;
      store_premul_pixel(col, dst);
    }
    src1 += 4;
    src2 += 4;
    dst += 4;
  }
}

static void do_alphaover_effect(const SeqRenderData *context,
                                Sequence * /*seq*/,
                                float /*timeline_frame*/,
                                float fac,
                                const ImBuf *ibuf1,
                                const ImBuf *ibuf2,
                                int start_line,
                                int total_lines,
                                ImBuf *out)
{
  if (out->float_buffer.data) {
    float *rect1 = nullptr, *rect2 = nullptr, *rect_out = nullptr;

    slice_get_float_buffers(context, ibuf1, ibuf2, out, start_line, &rect1, &rect2, &rect_out);

    do_alphaover_effect(fac, context->rectx, total_lines, rect1, rect2, rect_out);
  }
  else {
    uchar *rect1 = nullptr, *rect2 = nullptr, *rect_out = nullptr;

    slice_get_byte_buffers(context, ibuf1, ibuf2, out, start_line, &rect1, &rect2, &rect_out);

    do_alphaover_effect(fac, context->rectx, total_lines, rect1, rect2, rect_out);
  }
}

/* -------------------------------------------------------------------- */
/* Alpha Under Effect */

/* dst = src1 under src2 (alpha from src2) */
template<typename T>
static void do_alphaunder_effect(
    float fac, int width, int height, const T *src1, const T *src2, T *dst)
{
  if (fac <= 0.0f) {
    memcpy(dst, src2, sizeof(T) * 4 * width * height);
    return;
  }

  for (int pixel_idx = 0; pixel_idx < width * height; pixel_idx++) {
    if (src2[3] <= 0.0f && fac >= 1.0f) {
      memcpy(dst, src1, sizeof(T) * 4);
    }
    else if (alpha_opaque(src2[3])) {
      memcpy(dst, src2, sizeof(T) * 4);
    }
    else {
      float4 col2 = load_premul_pixel(src2);
      float mfac = fac * (1.0f - col2.w);
      float4 col1 = load_premul_pixel(src1);
      float4 col = mfac * col1 + col2;
      store_premul_pixel(col, dst);
    }
    src1 += 4;
    src2 += 4;
    dst += 4;
  }
}

static void do_alphaunder_effect(const SeqRenderData *context,
                                 Sequence * /*seq*/,
                                 float /*timeline_frame*/,
                                 float fac,
                                 const ImBuf *ibuf1,
                                 const ImBuf *ibuf2,
                                 int start_line,
                                 int total_lines,
                                 ImBuf *out)
{
  if (out->float_buffer.data) {
    float *rect1 = nullptr, *rect2 = nullptr, *rect_out = nullptr;

    slice_get_float_buffers(context, ibuf1, ibuf2, out, start_line, &rect1, &rect2, &rect_out);

    do_alphaunder_effect(fac, context->rectx, total_lines, rect1, rect2, rect_out);
  }
  else {
    uchar *rect1 = nullptr, *rect2 = nullptr, *rect_out = nullptr;

    slice_get_byte_buffers(context, ibuf1, ibuf2, out, start_line, &rect1, &rect2, &rect_out);

    do_alphaunder_effect(fac, context->rectx, total_lines, rect1, rect2, rect_out);
  }
}

/* -------------------------------------------------------------------- */
/* Drop Effect */

/* Must be > 0 or add pre-copy, etc to the function. */
#define XOFF 8
#define YOFF 8

static void do_drop_effect_byte(float fac, int x, int y, uchar *rect2i, uchar *rect1i, uchar *outi)
{
  const int xoff = min_ii(XOFF, x);
  const int yoff = min_ii(YOFF, y);

  int temp_fac = int(70.0f * fac);

  uchar *rt2 = rect2i + yoff * 4 * x;
  uchar *rt1 = rect1i;
  uchar *out = outi;
  for (int i = 0; i < y - yoff; i++) {
    memcpy(out, rt1, sizeof(*out) * xoff * 4);
    rt1 += xoff * 4;
    out += xoff * 4;

    for (int j = xoff; j < x; j++) {
      int temp_fac2 = ((temp_fac * rt2[3]) >> 8);

      *(out++) = std::max(0, *rt1 - temp_fac2);
      rt1++;
      *(out++) = std::max(0, *rt1 - temp_fac2);
      rt1++;
      *(out++) = std::max(0, *rt1 - temp_fac2);
      rt1++;
      *(out++) = std::max(0, *rt1 - temp_fac2);
      rt1++;
      rt2 += 4;
    }
    rt2 += xoff * 4;
  }
  memcpy(out, rt1, sizeof(*out) * yoff * 4 * x);
}

static void do_drop_effect_float(
    float fac, int x, int y, float *rect2i, float *rect1i, float *outi)
{
  const int xoff = min_ii(XOFF, x);
  const int yoff = min_ii(YOFF, y);

  float temp_fac = 70.0f * fac;

  float *rt2 = rect2i + yoff * 4 * x;
  float *rt1 = rect1i;
  float *out = outi;
  for (int i = 0; i < y - yoff; i++) {
    memcpy(out, rt1, sizeof(*out) * xoff * 4);
    rt1 += xoff * 4;
    out += xoff * 4;

    for (int j = xoff; j < x; j++) {
      float temp_fac2 = temp_fac * rt2[3];

      *(out++) = std::max(0.0f, *rt1 - temp_fac2);
      rt1++;
      *(out++) = std::max(0.0f, *rt1 - temp_fac2);
      rt1++;
      *(out++) = std::max(0.0f, *rt1 - temp_fac2);
      rt1++;
      *(out++) = std::max(0.0f, *rt1 - temp_fac2);
      rt1++;
      rt2 += 4;
    }
    rt2 += xoff * 4;
  }
  memcpy(out, rt1, sizeof(*out) * yoff * 4 * x);
}

/* -------------------------------------------------------------------- */
/* Blend Mode Effect */

/* blend_function has to be: void (T* dst, const T *src1, const T *src2) */
template<typename T, typename Func>
static void apply_blend_function(
    float fac, int width, int height, const T *src1, T *src2, T *dst, Func blend_function)
{
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      T achannel = src2[3];
      src2[3] = T(achannel * fac);
      blend_function(dst, src1, src2);
      src2[3] = achannel;
      dst[3] = src1[3];
      src1 += 4;
      src2 += 4;
      dst += 4;
    }
  }
}

static void do_blend_effect_float(
    float fac, int x, int y, const float *rect1, float *rect2, int btype, float *out)
{
  switch (btype) {
    case SEQ_TYPE_ADD:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_add_float);
      break;
    case SEQ_TYPE_SUB:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_sub_float);
      break;
    case SEQ_TYPE_MUL:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_mul_float);
      break;
    case SEQ_TYPE_DARKEN:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_darken_float);
      break;
    case SEQ_TYPE_COLOR_BURN:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_burn_float);
      break;
    case SEQ_TYPE_LINEAR_BURN:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_linearburn_float);
      break;
    case SEQ_TYPE_SCREEN:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_screen_float);
      break;
    case SEQ_TYPE_LIGHTEN:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_lighten_float);
      break;
    case SEQ_TYPE_DODGE:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_dodge_float);
      break;
    case SEQ_TYPE_OVERLAY:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_overlay_float);
      break;
    case SEQ_TYPE_SOFT_LIGHT:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_softlight_float);
      break;
    case SEQ_TYPE_HARD_LIGHT:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_hardlight_float);
      break;
    case SEQ_TYPE_PIN_LIGHT:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_pinlight_float);
      break;
    case SEQ_TYPE_LIN_LIGHT:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_linearlight_float);
      break;
    case SEQ_TYPE_VIVID_LIGHT:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_vividlight_float);
      break;
    case SEQ_TYPE_BLEND_COLOR:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_color_float);
      break;
    case SEQ_TYPE_HUE:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_hue_float);
      break;
    case SEQ_TYPE_SATURATION:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_saturation_float);
      break;
    case SEQ_TYPE_VALUE:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_luminosity_float);
      break;
    case SEQ_TYPE_DIFFERENCE:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_difference_float);
      break;
    case SEQ_TYPE_EXCLUSION:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_exclusion_float);
      break;
    default:
      break;
  }
}

static void do_blend_effect_byte(
    float fac, int x, int y, const uchar *rect1, uchar *rect2, int btype, uchar *out)
{
  switch (btype) {
    case SEQ_TYPE_ADD:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_add_byte);
      break;
    case SEQ_TYPE_SUB:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_sub_byte);
      break;
    case SEQ_TYPE_MUL:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_mul_byte);
      break;
    case SEQ_TYPE_DARKEN:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_darken_byte);
      break;
    case SEQ_TYPE_COLOR_BURN:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_burn_byte);
      break;
    case SEQ_TYPE_LINEAR_BURN:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_linearburn_byte);
      break;
    case SEQ_TYPE_SCREEN:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_screen_byte);
      break;
    case SEQ_TYPE_LIGHTEN:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_lighten_byte);
      break;
    case SEQ_TYPE_DODGE:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_dodge_byte);
      break;
    case SEQ_TYPE_OVERLAY:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_overlay_byte);
      break;
    case SEQ_TYPE_SOFT_LIGHT:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_softlight_byte);
      break;
    case SEQ_TYPE_HARD_LIGHT:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_hardlight_byte);
      break;
    case SEQ_TYPE_PIN_LIGHT:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_pinlight_byte);
      break;
    case SEQ_TYPE_LIN_LIGHT:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_linearlight_byte);
      break;
    case SEQ_TYPE_VIVID_LIGHT:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_vividlight_byte);
      break;
    case SEQ_TYPE_BLEND_COLOR:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_color_byte);
      break;
    case SEQ_TYPE_HUE:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_hue_byte);
      break;
    case SEQ_TYPE_SATURATION:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_saturation_byte);
      break;
    case SEQ_TYPE_VALUE:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_luminosity_byte);
      break;
    case SEQ_TYPE_DIFFERENCE:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_difference_byte);
      break;
    case SEQ_TYPE_EXCLUSION:
      apply_blend_function(fac, x, y, rect1, rect2, out, blend_color_exclusion_byte);
      break;
    default:
      break;
  }
}

static void do_blend_mode_effect(const SeqRenderData *context,
                                 Sequence *seq,
                                 float /*timeline_frame*/,
                                 float fac,
                                 const ImBuf *ibuf1,
                                 const ImBuf *ibuf2,
                                 int start_line,
                                 int total_lines,
                                 ImBuf *out)
{
  if (out->float_buffer.data) {
    float *rect1 = nullptr, *rect2 = nullptr, *rect_out = nullptr;
    slice_get_float_buffers(context, ibuf1, ibuf2, out, start_line, &rect1, &rect2, &rect_out);
    do_blend_effect_float(
        fac, context->rectx, total_lines, rect1, rect2, seq->blend_mode, rect_out);
  }
  else {
    uchar *rect1 = nullptr, *rect2 = nullptr, *rect_out = nullptr;
    slice_get_byte_buffers(context, ibuf1, ibuf2, out, start_line, &rect1, &rect2, &rect_out);
    do_blend_effect_byte(
        fac, context->rectx, total_lines, rect1, rect2, seq->blend_mode, rect_out);
  }
}

/* -------------------------------------------------------------------- */
/* Color Mix Effect */

static void init_colormix_effect(Sequence *seq)
{
  if (seq->effectdata) {
    MEM_freeN(seq->effectdata);
  }
  seq->effectdata = MEM_callocN(sizeof(ColorMixVars), "colormixvars");
  ColorMixVars *data = (ColorMixVars *)seq->effectdata;
  data->blend_effect = SEQ_TYPE_OVERLAY;
  data->factor = 1.0f;
}

static void do_colormix_effect(const SeqRenderData *context,
                               Sequence *seq,
                               float /*timeline_frame*/,
                               float /*fac*/,
                               const ImBuf *ibuf1,
                               const ImBuf *ibuf2,
                               int start_line,
                               int total_lines,
                               ImBuf *out)
{
  float fac;

  ColorMixVars *data = static_cast<ColorMixVars *>(seq->effectdata);
  fac = data->factor;

  if (out->float_buffer.data) {
    float *rect1 = nullptr, *rect2 = nullptr, *rect_out = nullptr;
    slice_get_float_buffers(context, ibuf1, ibuf2, out, start_line, &rect1, &rect2, &rect_out);
    do_blend_effect_float(
        fac, context->rectx, total_lines, rect1, rect2, data->blend_effect, rect_out);
  }
  else {
    uchar *rect1 = nullptr, *rect2 = nullptr, *rect_out = nullptr;
    slice_get_byte_buffers(context, ibuf1, ibuf2, out, start_line, &rect1, &rect2, &rect_out);
    do_blend_effect_byte(
        fac, context->rectx, total_lines, rect1, rect2, data->blend_effect, rect_out);
  }
}

/* -------------------------------------------------------------------- */
/* Over-Drop Effect */

static void do_overdrop_effect(const SeqRenderData *context,
                               Sequence * /*seq*/,
                               float /*timeline_frame*/,
                               float fac,
                               const ImBuf *ibuf1,
                               const ImBuf *ibuf2,
                               int start_line,
                               int total_lines,
                               ImBuf *out)
{
  int x = context->rectx;
  int y = total_lines;

  if (out->float_buffer.data) {
    float *rect1 = nullptr, *rect2 = nullptr, *rect_out = nullptr;

    slice_get_float_buffers(context, ibuf1, ibuf2, out, start_line, &rect1, &rect2, &rect_out);

    do_drop_effect_float(fac, x, y, rect1, rect2, rect_out);
    do_alphaover_effect(fac, x, y, rect1, rect2, rect_out);
  }
  else {
    uchar *rect1 = nullptr, *rect2 = nullptr, *rect_out = nullptr;

    slice_get_byte_buffers(context, ibuf1, ibuf2, out, start_line, &rect1, &rect2, &rect_out);

    do_drop_effect_byte(fac, x, y, rect1, rect2, rect_out);
    do_alphaover_effect(fac, x, y, rect1, rect2, rect_out);
  }
}

static void copy_effect_default(Sequence *dst, const Sequence *src, const int /*flag*/)
{
  dst->effectdata = MEM_dupallocN(src->effectdata);
}

static void free_effect_default(Sequence *seq, const bool /*do_id_user*/)
{
  MEM_SAFE_FREE(seq->effectdata);
}

void blend_mode_effect_get_handle(SeqEffectHandle &rval)
{
  rval.multithreaded = true;
  rval.execute_slice = do_blend_mode_effect;
  rval.early_out = early_out_mul_input2;
}

void color_mix_effect_get_handle(SeqEffectHandle &rval)
{
  rval.multithreaded = true;
  rval.init = init_colormix_effect;
  rval.free = free_effect_default;
  rval.copy = copy_effect_default;
  rval.execute_slice = do_colormix_effect;
  rval.early_out = early_out_mul_input2;
}

void alpha_over_effect_get_handle(SeqEffectHandle &rval)
{
  rval.multithreaded = true;
  rval.init = init_alpha_over_or_under;
  rval.execute_slice = do_alphaover_effect;
  rval.early_out = early_out_mul_input1;
}

void over_drop_effect_get_handle(SeqEffectHandle &rval)
{
  rval.multithreaded = true;
  rval.execute_slice = do_overdrop_effect;
}

void alpha_under_effect_get_handle(SeqEffectHandle &rval)
{
  rval.multithreaded = true;
  rval.init = init_alpha_over_or_under;
  rval.execute_slice = do_alphaunder_effect;
}
