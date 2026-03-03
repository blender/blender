/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "DNA_sequence_types.h"

#include "IMB_imbuf.hh"

#include "SEQ_render.hh"

#include "effects.hh"

namespace blender::seq {

/* -------------------------------------------------------------------- */
/* Color Add Effect */

struct AddEffectOp {
  template<typename T> void apply(const T *src1, const T *src2, T *dst, int64_t size) const
  {
    const float fac = this->factor;
    int ifac = int(256.0f * fac);
    for (int64_t idx = 0; idx < size; idx++) {
      if constexpr (std::is_same_v<T, uchar>) {
        const int f = ifac * int(src2[3]);
        dst[0] = min_ii(src1[0] + ((f * src2[0]) >> 16), 255);
        dst[1] = min_ii(src1[1] + ((f * src2[1]) >> 16), 255);
        dst[2] = min_ii(src1[2] + ((f * src2[2]) >> 16), 255);
      }
      else {
        const float f = (1.0f - (src1[3] * (1.0f - fac))) * src2[3];
        dst[0] = src1[0] + f * src2[0];
        dst[1] = src1[1] + f * src2[1];
        dst[2] = src1[2] + f * src2[2];
      }
      dst[3] = src1[3];
      src1 += 4;
      src2 += 4;
      dst += 4;
    }
  }
  float factor;
};

static ImBuf *do_add_effect(const RenderData *context,
                            SeqRenderState * /*state*/,
                            Strip * /*strip*/,
                            float /*timeline_frame*/,
                            float fac,
                            ImBuf *src1,
                            ImBuf *src2)
{
  ImBuf *dst = prepare_effect_imbufs(context, src1, src2);
  AddEffectOp op;
  op.factor = fac;
  apply_effect_op(op, src1, src2, dst);
  return dst;
}

/* -------------------------------------------------------------------- */
/* Color Subtract Effect */

struct SubEffectOp {
  template<typename T> void apply(const T *src1, const T *src2, T *dst, int64_t size) const
  {
    const float fac = this->factor;
    int ifac = int(256.0f * fac);
    for (int64_t idx = 0; idx < size; idx++) {
      if constexpr (std::is_same_v<T, uchar>) {
        const int f = ifac * int(src2[3]);
        dst[0] = max_ii(src1[0] - ((f * src2[0]) >> 16), 0);
        dst[1] = max_ii(src1[1] - ((f * src2[1]) >> 16), 0);
        dst[2] = max_ii(src1[2] - ((f * src2[2]) >> 16), 0);
      }
      else {
        const float f = (1.0f - (src1[3] * (1.0f - fac))) * src2[3];
        dst[0] = max_ff(src1[0] - f * src2[0], 0.0f);
        dst[1] = max_ff(src1[1] - f * src2[1], 0.0f);
        dst[2] = max_ff(src1[2] - f * src2[2], 0.0f);
      }
      dst[3] = src1[3];
      src1 += 4;
      src2 += 4;
      dst += 4;
    }
  }
  float factor;
};

static ImBuf *do_sub_effect(const RenderData *context,
                            SeqRenderState * /*state*/,
                            Strip * /*strip*/,
                            float /*timeline_frame*/,
                            float fac,
                            ImBuf *src1,
                            ImBuf *src2)
{
  ImBuf *dst = prepare_effect_imbufs(context, src1, src2);
  SubEffectOp op;
  op.factor = fac;
  apply_effect_op(op, src1, src2, dst);
  return dst;
}

/* -------------------------------------------------------------------- */
/* Multiply Effect */

struct MulEffectOp {
  template<typename T> void apply(const T *src1, const T *src2, T *dst, int64_t size) const
  {
    const float fac = this->factor;
    int ifac = int(256.0f * fac);
    for (int64_t idx = 0; idx < size; idx++) {
      /* Formula: `fac * (a * b) + (1-fac) * a => fac * a * (b - 1) + a` */
      if constexpr (std::is_same_v<T, uchar>) {
        dst[0] = src1[0] + ((ifac * src1[0] * (src2[0] - 255)) >> 16);
        dst[1] = src1[1] + ((ifac * src1[1] * (src2[1] - 255)) >> 16);
        dst[2] = src1[2] + ((ifac * src1[2] * (src2[2] - 255)) >> 16);
        dst[3] = src1[3] + ((ifac * src1[3] * (src2[3] - 255)) >> 16);
      }
      else {
        dst[0] = src1[0] + fac * src1[0] * (src2[0] - 1.0f);
        dst[1] = src1[1] + fac * src1[1] * (src2[1] - 1.0f);
        dst[2] = src1[2] + fac * src1[2] * (src2[2] - 1.0f);
        dst[3] = src1[3] + fac * src1[3] * (src2[3] - 1.0f);
      }
      src1 += 4;
      src2 += 4;
      dst += 4;
    }
  }
  float factor;
};

static ImBuf *do_mul_effect(const RenderData *context,
                            SeqRenderState * /*state*/,
                            Strip * /*strip*/,
                            float /*timeline_frame*/,
                            float fac,
                            ImBuf *src1,
                            ImBuf *src2)
{
  ImBuf *dst = prepare_effect_imbufs(context, src1, src2);
  MulEffectOp op;
  op.factor = fac;
  apply_effect_op(op, src1, src2, dst);
  return dst;
}

void add_effect_get_handle(EffectHandle &rval)
{
  rval.execute = do_add_effect;
  rval.early_out = early_out_mul_input2;
}

void sub_effect_get_handle(EffectHandle &rval)
{
  rval.execute = do_sub_effect;
  rval.early_out = early_out_mul_input2;
}

void mul_effect_get_handle(EffectHandle &rval)
{
  rval.execute = do_mul_effect;
  rval.early_out = early_out_mul_input2;
}

}  // namespace blender::seq
