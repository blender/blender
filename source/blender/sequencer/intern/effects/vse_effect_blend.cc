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

namespace blender::seq {

/* -------------------------------------------------------------------- */
/* Alpha Over Effect */

static void init_alpha_over_or_under(Strip *strip)
{
  Strip *input1 = strip->input1;
  Strip *input2 = strip->input2;

  strip->input2 = input1;
  strip->input1 = input2;
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

static ImBuf *do_alphaover_effect(const RenderData *context,
                                  SeqRenderState * /*state*/,
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

static ImBuf *do_alphaunder_effect(const RenderData *context,
                                   SeqRenderState * /*state*/,
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

static void do_blend_effect_float(float fac,
                                  int64_t size,
                                  const float *rect1,
                                  const float *rect2,
                                  StripBlendMode btype,
                                  float *out)
{
  switch (btype) {
    case STRIP_BLEND_ADD:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_add_float);
      break;
    case STRIP_BLEND_SUB:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_sub_float);
      break;
    case STRIP_BLEND_MUL:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_mul_float);
      break;
    case STRIP_BLEND_DARKEN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_darken_float);
      break;
    case STRIP_BLEND_COLOR_BURN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_burn_float);
      break;
    case STRIP_BLEND_LINEAR_BURN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_linearburn_float);
      break;
    case STRIP_BLEND_SCREEN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_screen_float);
      break;
    case STRIP_BLEND_LIGHTEN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_lighten_float);
      break;
    case STRIP_BLEND_DODGE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_dodge_float);
      break;
    case STRIP_BLEND_OVERLAY:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_overlay_float);
      break;
    case STRIP_BLEND_SOFT_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_softlight_float);
      break;
    case STRIP_BLEND_HARD_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_hardlight_float);
      break;
    case STRIP_BLEND_PIN_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_pinlight_float);
      break;
    case STRIP_BLEND_LIN_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_linearlight_float);
      break;
    case STRIP_BLEND_VIVID_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_vividlight_float);
      break;
    case STRIP_BLEND_BLEND_COLOR:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_color_float);
      break;
    case STRIP_BLEND_HUE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_hue_float);
      break;
    case STRIP_BLEND_SATURATION:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_saturation_float);
      break;
    case STRIP_BLEND_VALUE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_luminosity_float);
      break;
    case STRIP_BLEND_DIFFERENCE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_difference_float);
      break;
    case STRIP_BLEND_EXCLUSION:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_exclusion_float);
      break;
    default:
      break;
  }
}

static void do_blend_effect_byte(float fac,
                                 int64_t size,
                                 const uchar *rect1,
                                 const uchar *rect2,
                                 StripBlendMode btype,
                                 uchar *out)
{
  switch (btype) {
    case STRIP_BLEND_ADD:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_add_byte);
      break;
    case STRIP_BLEND_SUB:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_sub_byte);
      break;
    case STRIP_BLEND_MUL:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_mul_byte);
      break;
    case STRIP_BLEND_DARKEN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_darken_byte);
      break;
    case STRIP_BLEND_COLOR_BURN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_burn_byte);
      break;
    case STRIP_BLEND_LINEAR_BURN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_linearburn_byte);
      break;
    case STRIP_BLEND_SCREEN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_screen_byte);
      break;
    case STRIP_BLEND_LIGHTEN:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_lighten_byte);
      break;
    case STRIP_BLEND_DODGE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_dodge_byte);
      break;
    case STRIP_BLEND_OVERLAY:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_overlay_byte);
      break;
    case STRIP_BLEND_SOFT_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_softlight_byte);
      break;
    case STRIP_BLEND_HARD_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_hardlight_byte);
      break;
    case STRIP_BLEND_PIN_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_pinlight_byte);
      break;
    case STRIP_BLEND_LIN_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_linearlight_byte);
      break;
    case STRIP_BLEND_VIVID_LIGHT:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_vividlight_byte);
      break;
    case STRIP_BLEND_BLEND_COLOR:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_color_byte);
      break;
    case STRIP_BLEND_HUE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_hue_byte);
      break;
    case STRIP_BLEND_SATURATION:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_saturation_byte);
      break;
    case STRIP_BLEND_VALUE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_luminosity_byte);
      break;
    case STRIP_BLEND_DIFFERENCE:
      apply_blend_function(fac, size, rect1, rect2, out, blend_color_difference_byte);
      break;
    case STRIP_BLEND_EXCLUSION:
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
  StripBlendMode blend_mode;
  float factor;
};

static ImBuf *do_blend_mode_effect(const RenderData *context,
                                   SeqRenderState * /*state*/,
                                   Strip *strip,
                                   float /*timeline_frame*/,
                                   float fac,
                                   ImBuf *src1,
                                   ImBuf *src2)
{
  ImBuf *dst = prepare_effect_imbufs(context, src1, src2);
  BlendModeEffectOp op;
  op.factor = fac;
  op.blend_mode = StripBlendMode(strip->blend_mode);
  apply_effect_op(op, src1, src2, dst);
  return dst;
}

/* -------------------------------------------------------------------- */
/* Color Mix Effect */

static void init_colormix_effect(Strip *strip)
{
  MEM_SAFE_FREE(strip->effectdata);
  ColorMixVars *data = MEM_callocN<ColorMixVars>("colormixvars");
  strip->effectdata = data;
  data->blend_effect = STRIP_BLEND_OVERLAY;
  data->factor = 1.0f;
}

static ImBuf *do_colormix_effect(const RenderData *context,
                                 SeqRenderState * /*state*/,
                                 Strip *strip,
                                 float /*timeline_frame*/,
                                 float /*fac*/,
                                 ImBuf *src1,
                                 ImBuf *src2)
{
  ImBuf *dst = prepare_effect_imbufs(context, src1, src2);
  const ColorMixVars *data = static_cast<const ColorMixVars *>(strip->effectdata);
  BlendModeEffectOp op;
  op.blend_mode = StripBlendMode(data->blend_effect);
  op.factor = data->factor;
  apply_effect_op(op, src1, src2, dst);
  return dst;
}

void blend_mode_effect_get_handle(EffectHandle &rval)
{
  rval.execute = do_blend_mode_effect;
  rval.early_out = early_out_mul_input2;
}

void color_mix_effect_get_handle(EffectHandle &rval)
{
  rval.init = init_colormix_effect;
  rval.execute = do_colormix_effect;
  rval.early_out = early_out_mul_input2;
}

void alpha_over_effect_get_handle(EffectHandle &rval)
{
  rval.init = init_alpha_over_or_under;
  rval.execute = do_alphaover_effect;
  rval.early_out = early_out_mul_input1;
}

void alpha_under_effect_get_handle(EffectHandle &rval)
{
  rval.init = init_alpha_over_or_under;
  rval.execute = do_alphaunder_effect;
}

}  // namespace blender::seq
