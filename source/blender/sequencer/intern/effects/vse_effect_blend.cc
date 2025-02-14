/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_math_color_blend.h"

#include "DNA_sequence_types.h"

#include "IMB_imbuf.hh"

#include "SEQ_render.hh"

#include "effects.hh"

using namespace blender;

/* -------------------------------------------------------------------- */
/* Alpha Over Effect */

static void init_alpha_over_or_under(Strip *strip)
{
  Strip *seq1 = strip->seq1;
  Strip *seq2 = strip->seq2;

  strip->seq2 = seq1;
  strip->seq1 = seq2;
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
struct AlphaOverEffectOp {
  template<typename T> void apply(const T *src1, const T *src2, T *dst, int64_t size) const
  {
    const float fac = this->factor;
    if (fac <= 0.0f) {
      memcpy(dst, src2, sizeof(T) * 4 * size);
      return;
    }

    for (int64_t idx = 0; idx < size; idx++) {
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

  float factor;
};

static ImBuf *do_alphaover_effect(const SeqRenderData *context,
                                  Strip * /*strip*/,
                                  float /*timeline_frame*/,
                                  float fac,
                                  ImBuf *src1,
                                  ImBuf *src2)
{
  ImBuf *dst = prepare_effect_imbufs(context, src1, src2);
  AlphaOverEffectOp op;
  op.factor = fac;
  apply_effect_op(op, src1, src2, dst);
  return dst;
}

/* -------------------------------------------------------------------- */
/* Alpha Under Effect */

/* dst = src1 under src2 (alpha from src2) */
struct AlphaUnderEffectOp {
  template<typename T> void apply(const T *src1, const T *src2, T *dst, int64_t size) const
  {
    const float fac = this->factor;
    if (fac <= 0.0f) {
      memcpy(dst, src2, sizeof(T) * 4 * size);
      return;
    }

    for (int64_t idx = 0; idx < size; idx++) {
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
  float factor;
};

static ImBuf *do_alphaunder_effect(const SeqRenderData *context,
                                   Strip * /*strip*/,
                                   float /*timeline_frame*/,
                                   float fac,
                                   ImBuf *src1,
                                   ImBuf *src2)
{
  ImBuf *dst = prepare_effect_imbufs(context, src1, src2);
  AlphaUnderEffectOp op;
  op.factor = fac;
  apply_effect_op(op, src1, src2, dst);
  return dst;
}

/* -------------------------------------------------------------------- */
/* Blend Mode Effect */

/* blend_function has to be: void (T* dst, const T *src1, const T *src2) */
template<typename T, typename Func>
static void apply_blend_function(
    float fac, int64_t size, const T *src1, const T *src2, T *dst, Func blend_function)
{
  for (int64_t i = 0; i < size; i++) {
    T achannel = src2[3];
    ((T *)src2)[3] = T(achannel * fac);
    blend_function(dst, src1, src2);
    ((T *)src2)[3] = achannel;
    dst[3] = src1[3];
    src1 += 4;
    src2 += 4;
    dst += 4;
  }
}

static void do_blend_effect_float(
    float fac, int64_t size, const float *rect1, const float *rect2, int btype, float *out)
{
  switch (btype) {
    case STRIP_TYPE_ADD:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_add_float);
      break;
    case STRIP_TYPE_SUB:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_sub_float);
      break;
    case STRIP_TYPE_MUL:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_mul_float);
      break;
    case STRIP_TYPE_DARKEN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_darken_float);
      break;
    case STRIP_TYPE_COLOR_BURN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_burn_float);
      break;
    case STRIP_TYPE_LINEAR_BURN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_linearburn_float);
      break;
    case STRIP_TYPE_SCREEN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_screen_float);
      break;
    case STRIP_TYPE_LIGHTEN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_lighten_float);
      break;
    case STRIP_TYPE_DODGE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_dodge_float);
      break;
    case STRIP_TYPE_OVERLAY:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_overlay_float);
      break;
    case STRIP_TYPE_SOFT_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_softlight_float);
      break;
    case STRIP_TYPE_HARD_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_hardlight_float);
      break;
    case STRIP_TYPE_PIN_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_pinlight_float);
      break;
    case STRIP_TYPE_LIN_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_linearlight_float);
      break;
    case STRIP_TYPE_VIVID_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_vividlight_float);
      break;
    case STRIP_TYPE_BLEND_COLOR:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_color_float);
      break;
    case STRIP_TYPE_HUE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_hue_float);
      break;
    case STRIP_TYPE_SATURATION:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_saturation_float);
      break;
    case STRIP_TYPE_VALUE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_luminosity_float);
      break;
    case STRIP_TYPE_DIFFERENCE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_difference_float);
      break;
    case STRIP_TYPE_EXCLUSION:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_exclusion_float);
      break;
    default:
      break;
  }
}

static void do_blend_effect_byte(
    float fac, int64_t size, const uchar *rect1, const uchar *rect2, int btype, uchar *out)
{
  switch (btype) {
    case STRIP_TYPE_ADD:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_add_byte);
      break;
    case STRIP_TYPE_SUB:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_sub_byte);
      break;
    case STRIP_TYPE_MUL:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_mul_byte);
      break;
    case STRIP_TYPE_DARKEN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_darken_byte);
      break;
    case STRIP_TYPE_COLOR_BURN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_burn_byte);
      break;
    case STRIP_TYPE_LINEAR_BURN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_linearburn_byte);
      break;
    case STRIP_TYPE_SCREEN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_screen_byte);
      break;
    case STRIP_TYPE_LIGHTEN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_lighten_byte);
      break;
    case STRIP_TYPE_DODGE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_dodge_byte);
      break;
    case STRIP_TYPE_OVERLAY:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_overlay_byte);
      break;
    case STRIP_TYPE_SOFT_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_softlight_byte);
      break;
    case STRIP_TYPE_HARD_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_hardlight_byte);
      break;
    case STRIP_TYPE_PIN_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_pinlight_byte);
      break;
    case STRIP_TYPE_LIN_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_linearlight_byte);
      break;
    case STRIP_TYPE_VIVID_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_vividlight_byte);
      break;
    case STRIP_TYPE_BLEND_COLOR:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_color_byte);
      break;
    case STRIP_TYPE_HUE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_hue_byte);
      break;
    case STRIP_TYPE_SATURATION:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_saturation_byte);
      break;
    case STRIP_TYPE_VALUE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_luminosity_byte);
      break;
    case STRIP_TYPE_DIFFERENCE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_difference_byte);
      break;
    case STRIP_TYPE_EXCLUSION:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_exclusion_byte);
      break;
    default:
      break;
  }
}

struct BlendModeEffectOp {
  template<typename T> void apply(const T *src1, const T *src2, T *dst, int64_t size) const
  {
    if constexpr (std::is_same_v<T, float>) {
      do_blend_effect_float(this->factor, size, src1, src2, this->blend_mode, dst);
    }
    else {
      do_blend_effect_byte(this->factor, size, src1, src2, this->blend_mode, dst);
    }
  }
  int blend_mode; /* STRIP_TYPE_ */
  float factor;
};

static ImBuf *do_blend_mode_effect(const SeqRenderData *context,
                                   Strip *strip,
                                   float /*timeline_frame*/,
                                   float fac,
                                   ImBuf *src1,
                                   ImBuf *src2)
{
  ImBuf *dst = prepare_effect_imbufs(context, src1, src2);
  BlendModeEffectOp op;
  op.factor = fac;
  op.blend_mode = strip->blend_mode;
  apply_effect_op(op, src1, src2, dst);
  return dst;
}

/* -------------------------------------------------------------------- */
/* Color Mix Effect */

static void init_colormix_effect(Strip *strip)
{
  if (strip->effectdata) {
    MEM_freeN(strip->effectdata);
  }
  strip->effectdata = MEM_callocN(sizeof(ColorMixVars), "colormixvars");
  ColorMixVars *data = (ColorMixVars *)strip->effectdata;
  data->blend_effect = STRIP_TYPE_OVERLAY;
  data->factor = 1.0f;
}

static ImBuf *do_colormix_effect(const SeqRenderData *context,
                                 Strip *strip,
                                 float /*timeline_frame*/,
                                 float /*fac*/,
                                 ImBuf *src1,
                                 ImBuf *src2)
{
  ImBuf *dst = prepare_effect_imbufs(context, src1, src2);
  const ColorMixVars *data = static_cast<const ColorMixVars *>(strip->effectdata);
  BlendModeEffectOp op;
  op.blend_mode = data->blend_effect;
  op.factor = data->factor;
  apply_effect_op(op, src1, src2, dst);
  return dst;
}

/* -------------------------------------------------------------------- */
/* Over-Drop Effect */

/* Before Blender 2.42 (2006), over-drop effect used to have some
 * sort of drop shadow with itself blended on top. However since then
 * (commit 327d413eb3c0c), it is effectively just alpha-over with swapped
 * inputs and thus the effect "fade" factor controlling the other input. */

static ImBuf *do_overdrop_effect(const SeqRenderData *context,
                                 Strip *strip,
                                 float timeline_frame,
                                 float fac,
                                 ImBuf *src1,
                                 ImBuf *src2)
{
  return do_alphaover_effect(context, strip, timeline_frame, fac, src1, src2);
}

static void copy_effect_default(Strip *dst, const Strip *src, const int /*flag*/)
{
  dst->effectdata = MEM_dupallocN(src->effectdata);
}

static void free_effect_default(Strip *strip, const bool /*do_id_user*/)
{
  MEM_SAFE_FREE(strip->effectdata);
}

void blend_mode_effect_get_handle(SeqEffectHandle &rval)
{
  rval.execute = do_blend_mode_effect;
  rval.early_out = early_out_mul_input2;
}

void color_mix_effect_get_handle(SeqEffectHandle &rval)
{
  rval.init = init_colormix_effect;
  rval.free = free_effect_default;
  rval.copy = copy_effect_default;
  rval.execute = do_colormix_effect;
  rval.early_out = early_out_mul_input2;
}

void alpha_over_effect_get_handle(SeqEffectHandle &rval)
{
  rval.init = init_alpha_over_or_under;
  rval.execute = do_alphaover_effect;
  rval.early_out = early_out_mul_input1;
}

void over_drop_effect_get_handle(SeqEffectHandle &rval)
{
  rval.execute = do_overdrop_effect;
}

void alpha_under_effect_get_handle(SeqEffectHandle &rval)
{
  rval.init = init_alpha_over_or_under;
  rval.execute = do_alphaunder_effect;
}
