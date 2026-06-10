/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "DNA_sequence_types.h"

#include "IMB_imbuf.hh"

#include "PRF_profile.hh"

#include "SEQ_render.hh"

#include "effects.hh"

namespace blender::seq {

struct CrossEffectOp {
  template<typename T> void apply(const T *src1, const T *src2, T *dst, int64_t size) const
  {
    const float fac = this->factor;
    const float mfac = 1.0f - fac;
    const int ifac = int(256.0f * fac);
    const int imfac = 256 - ifac;
    for (int64_t idx = 0; idx < size; idx++) {
      if constexpr (std::is_same_v<T, uchar>) {
        dst[0] = (imfac * src1[0] + ifac * src2[0]) >> 8;
        dst[1] = (imfac * src1[1] + ifac * src2[1]) >> 8;
        dst[2] = (imfac * src1[2] + ifac * src2[2]) >> 8;
        dst[3] = (imfac * src1[3] + ifac * src2[3]) >> 8;
      }
      else {
        dst[0] = mfac * src1[0] + fac * src2[0];
        dst[1] = mfac * src1[1] + fac * src2[1];
        dst[2] = mfac * src1[2] + fac * src2[2];
        dst[3] = mfac * src1[3] + fac * src2[3];
      }
      src1 += 4;
      src2 += 4;
      dst += 4;
    }
  }
  float factor;
};

static SeqResult do_cross_effect(const RenderData *context,
                                 SeqRenderState * /*state*/,
                                 Strip * /*strip*/,
                                 float /*timeline_frame*/,
                                 float fac,
                                 const SeqResult &src1,
                                 const SeqResult &src2)
{
  PRF_scope_with_name("SeqFxCross", ProfileCategory::Draw);
  SeqResult dst = prepare_effect_imbufs(context, src1, src2);
  CrossEffectOp op;
  op.factor = fac;
  apply_effect_op(op, src1.image, src2.image, dst.image);
  dst.is_opaque_before_transform = !src1.image->can_contain_alpha() &&
                                   !src2.image->can_contain_alpha();
  return dst;
}

/* One could argue that gamma cross should not be hardcoded to 2.0 gamma,
 * but instead either do proper input->linear conversion (often sRGB). Or
 * maybe not even that, but do interpolation in some perceptual color space
 * like OKLAB. But currently it is fixed to just 2.0 gamma. */

static float gammaCorrect(float c)
{
  if (c < 0) [[unlikely]] {
    return -(c * c);
  }
  return c * c;
}

static float invGammaCorrect(float c)
{
  return sqrtf_signed(c);
}

struct GammaCrossEffectOp {
  template<typename T> void apply(const T *src1, const T *src2, T *dst, int64_t size) const
  {
    const float fac = this->factor;
    const float mfac = 1.0f - fac;
    for (int64_t idx = 0; idx < size; idx++) {
      float4 col1 = load_premul_pixel(src1);
      float4 col2 = load_premul_pixel(src2);
      float4 col;
      for (int c = 0; c < 4; ++c) {
        col[c] = gammaCorrect(mfac * invGammaCorrect(col1[c]) + fac * invGammaCorrect(col2[c]));
      }
      store_premul_pixel(col, dst);
      src1 += 4;
      src2 += 4;
      dst += 4;
    }
  }
  float factor;
};

static SeqResult do_gammacross_effect(const RenderData *context,
                                      SeqRenderState * /*state*/,
                                      Strip * /*strip*/,
                                      float /*timeline_frame*/,
                                      float fac,
                                      const SeqResult &src1,
                                      const SeqResult &src2)
{
  PRF_scope_with_name("SeqFxGammaCross", ProfileCategory::Draw);
  SeqResult dst = prepare_effect_imbufs(context, src1, src2);
  GammaCrossEffectOp op;
  op.factor = fac;
  apply_effect_op(op, src1.image, src2.image, dst.image);
  dst.is_opaque_before_transform = !src1.image->can_contain_alpha() &&
                                   !src2.image->can_contain_alpha();
  return dst;
}

void cross_effect_get_handle(EffectHandle &rval)
{
  rval.execute = do_cross_effect;
  rval.early_out = early_out_fade;
}

void gamma_cross_effect_get_handle(EffectHandle &rval)
{
  rval.early_out = early_out_fade;
  rval.execute = do_gammacross_effect;
}

}  // namespace blender::seq
