/* SPDX-FileCopyrightText: 2012-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstddef>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.hh"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BKE_colortools.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "SEQ_modifier.hh"
#include "SEQ_render.hh"
#include "SEQ_sound.hh"
#include "SEQ_time.hh"
#include "SEQ_utils.hh"

#include "BLO_read_write.hh"

#include "render.hh"

using namespace blender;

static SequenceModifierTypeInfo *modifiersTypes[NUM_SEQUENCE_MODIFIER_TYPES];
static bool modifierTypesInit = false;

/* -------------------------------------------------------------------- */
/** \name Modifier Multi-Threading Utilities
 * \{ */

using modifier_apply_threaded_cb = void (*)(int width,
                                            int height,
                                            uchar *rect,
                                            float *rect_float,
                                            uchar *mask_rect,
                                            const float *mask_rect_float,
                                            void *data_v);

struct ModifierInitData {
  ImBuf *ibuf;
  ImBuf *mask;
  void *user_data;

  modifier_apply_threaded_cb apply_callback;
};

struct ModifierThread {
  int width, height;

  uchar *rect, *mask_rect;
  float *rect_float, *mask_rect_float;

  void *user_data;

  modifier_apply_threaded_cb apply_callback;
};

/**
 * \a timeline_frame is offset by \a fra_offset only in case we are using a real mask.
 */
static ImBuf *modifier_render_mask_input(const SeqRenderData *context,
                                         int mask_input_type,
                                         Sequence *mask_sequence,
                                         Mask *mask_id,
                                         int timeline_frame,
                                         int fra_offset,
                                         bool make_float)
{
  ImBuf *mask_input = nullptr;

  if (mask_input_type == SEQUENCE_MASK_INPUT_STRIP) {
    if (mask_sequence) {
      SeqRenderState state;

      mask_input = seq_render_strip(context, &state, mask_sequence, timeline_frame);

      if (make_float) {
        if (!mask_input->float_buffer.data) {
          IMB_float_from_rect(mask_input);
        }
      }
      else {
        if (!mask_input->byte_buffer.data) {
          IMB_rect_from_float(mask_input);
        }
      }
    }
  }
  else if (mask_input_type == SEQUENCE_MASK_INPUT_ID) {
    mask_input = seq_render_mask(context, mask_id, timeline_frame - fra_offset, make_float);
  }

  return mask_input;
}

static ImBuf *modifier_mask_get(SequenceModifierData *smd,
                                const SeqRenderData *context,
                                int timeline_frame,
                                int fra_offset,
                                bool make_float)
{
  return modifier_render_mask_input(context,
                                    smd->mask_input_type,
                                    smd->mask_sequence,
                                    smd->mask_id,
                                    timeline_frame,
                                    fra_offset,
                                    make_float);
}

static void modifier_init_handle(void *handle_v, int start_line, int tot_line, void *init_data_v)
{
  ModifierThread *handle = (ModifierThread *)handle_v;
  ModifierInitData *init_data = (ModifierInitData *)init_data_v;
  ImBuf *ibuf = init_data->ibuf;
  ImBuf *mask = init_data->mask;

  int offset = 4 * start_line * ibuf->x;

  memset(handle, 0, sizeof(ModifierThread));

  handle->width = ibuf->x;
  handle->height = tot_line;
  handle->apply_callback = init_data->apply_callback;
  handle->user_data = init_data->user_data;

  if (ibuf->byte_buffer.data) {
    handle->rect = ibuf->byte_buffer.data + offset;
  }

  if (ibuf->float_buffer.data) {
    handle->rect_float = ibuf->float_buffer.data + offset;
  }

  if (mask) {
    if (mask->byte_buffer.data) {
      handle->mask_rect = mask->byte_buffer.data + offset;
    }

    if (mask->float_buffer.data) {
      handle->mask_rect_float = mask->float_buffer.data + offset;
    }
  }
  else {
    handle->mask_rect = nullptr;
    handle->mask_rect_float = nullptr;
  }
}

static void *modifier_do_thread(void *thread_data_v)
{
  ModifierThread *td = (ModifierThread *)thread_data_v;

  td->apply_callback(td->width,
                     td->height,
                     td->rect,
                     td->rect_float,
                     td->mask_rect,
                     td->mask_rect_float,
                     td->user_data);

  return nullptr;
}

static void modifier_apply_threaded(ImBuf *ibuf,
                                    ImBuf *mask,
                                    modifier_apply_threaded_cb apply_callback,
                                    void *user_data)
{
  ModifierInitData init_data;

  init_data.ibuf = ibuf;
  init_data.mask = mask;
  init_data.user_data = user_data;

  init_data.apply_callback = apply_callback;

  IMB_processor_apply_threaded(
      ibuf->y, sizeof(ModifierThread), &init_data, modifier_init_handle, modifier_do_thread);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Balance Modifier
 * \{ */

static StripColorBalance calc_cb_lgg(const StripColorBalance *cb_)
{
  StripColorBalance cb = *cb_;
  int c;

  for (c = 0; c < 3; c++) {
    cb.lift[c] = 2.0f - cb.lift[c];
  }

  if (cb.flag & SEQ_COLOR_BALANCE_INVERSE_LIFT) {
    for (c = 0; c < 3; c++) {
      /* tweak to give more subtle results
       * values above 1.0 are scaled */
      if (cb.lift[c] > 1.0f) {
        cb.lift[c] = pow(cb.lift[c] - 1.0f, 2.0) + 1.0;
      }

      cb.lift[c] = 2.0f - cb.lift[c];
    }
  }

  if (cb.flag & SEQ_COLOR_BALANCE_INVERSE_GAIN) {
    for (c = 0; c < 3; c++) {
      if (cb.gain[c] != 0.0f) {
        cb.gain[c] = 1.0f / cb.gain[c];
      }
      else {
        cb.gain[c] = 1000000; /* should be enough :) */
      }
    }
  }

  if (!(cb.flag & SEQ_COLOR_BALANCE_INVERSE_GAMMA)) {
    for (c = 0; c < 3; c++) {
      if (cb.gamma[c] != 0.0f) {
        cb.gamma[c] = 1.0f / cb.gamma[c];
      }
      else {
        cb.gamma[c] = 1000000; /* should be enough :) */
      }
    }
  }

  return cb;
}

static StripColorBalance calc_cb_sop(const StripColorBalance *cb_)
{
  StripColorBalance cb = *cb_;
  int c;

  for (c = 0; c < 3; c++) {
    if (cb.flag & SEQ_COLOR_BALANCE_INVERSE_SLOPE) {
      if (cb.slope[c] != 0.0f) {
        cb.slope[c] = 1.0f / cb.slope[c];
      }
      else {
        cb.slope[c] = 1000000;
      }
    }

    if (cb.flag & SEQ_COLOR_BALANCE_INVERSE_OFFSET) {
      cb.offset[c] = -1.0f * (cb.offset[c] - 1.0f);
    }
    else {
      cb.offset[c] = cb.offset[c] - 1.0f;
    }

    if (!(cb.flag & SEQ_COLOR_BALANCE_INVERSE_POWER)) {
      if (cb.power[c] != 0.0f) {
        cb.power[c] = 1.0f / cb.power[c];
      }
      else {
        cb.power[c] = 1000000;
      }
    }
  }

  return cb;
}

static StripColorBalance calc_cb(const StripColorBalance *cb_)
{
  if (cb_->method == SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN) {
    return calc_cb_lgg(cb_);
  }
  /* `cb_->method == SEQ_COLOR_BALANCE_METHOD_SLOPEOFFSETPOWER`. */
  return calc_cb_sop(cb_);
}

/* Lift-Gamma-Gain math. NOTE: lift is actually (2-lift). */
static float color_balance_lgg(
    float in, const float lift, const float gain, const float gamma, const float mul)
{
  float x = (((in - 1.0f) * lift) + 1.0f) * gain;

  /* prevent NaN */
  if (x < 0.0f) {
    x = 0.0f;
  }

  x = powf(x, gamma) * mul;
  CLAMP(x, FLT_MIN, FLT_MAX);
  return x;
}

/* Slope-Offset-Power (ASC CDL) math, see https://en.wikipedia.org/wiki/ASC_CDL */
static float color_balance_sop(
    float in, const float slope, const float offset, const float power, float mul)
{
  float x = in * slope + offset;

  /* prevent NaN */
  if (x < 0.0f) {
    x = 0.0f;
  }

  x = powf(x, power);
  x *= mul;
  CLAMP(x, FLT_MIN, FLT_MAX);
  return x;
}

/**
 * Use a larger lookup table than 256 possible byte values: due to alpha
 * pre-multiplication, dark values with low alphas might need more precision.
 */
static constexpr int CB_TABLE_SIZE = 1024;

static void make_cb_table_lgg(
    float lift, float gain, float gamma, float mul, float r_table[CB_TABLE_SIZE])
{
  for (int i = 0; i < CB_TABLE_SIZE; i++) {
    float x = float(i) * (1.0f / (CB_TABLE_SIZE - 1.0f));
    r_table[i] = color_balance_lgg(x, lift, gain, gamma, mul);
  }
}

static void make_cb_table_sop(
    float slope, float offset, float power, float mul, float r_table[CB_TABLE_SIZE])
{
  for (int i = 0; i < CB_TABLE_SIZE; i++) {
    float x = float(i) * (1.0f / (CB_TABLE_SIZE - 1.0f));
    r_table[i] = color_balance_sop(x, slope, offset, power, mul);
  }
}

static void color_balance_byte(const float cb_tab[3][CB_TABLE_SIZE],
                               uchar *rect,
                               const uchar *mask_rect,
                               int width,
                               int height)
{
  uchar *ptr = rect;
  const uchar *ptr_end = ptr + int64_t(width) * height * 4;
  const uchar *mask_ptr = mask_rect;

  if (mask_ptr != nullptr) {
    /* Mask is used. */
    while (ptr < ptr_end) {
      float pix[4];
      straight_uchar_to_premul_float(pix, ptr);

      int p0 = int(pix[0] * (CB_TABLE_SIZE - 1.0f) + 0.5f);
      int p1 = int(pix[1] * (CB_TABLE_SIZE - 1.0f) + 0.5f);
      int p2 = int(pix[2] * (CB_TABLE_SIZE - 1.0f) + 0.5f);
      const float t[3] = {mask_ptr[0] / 255.0f, mask_ptr[1] / 255.0f, mask_ptr[2] / 255.0f};

      pix[0] = pix[0] * (1.0f - t[0]) + t[0] * cb_tab[0][p0];
      pix[1] = pix[1] * (1.0f - t[1]) + t[1] * cb_tab[1][p1];
      pix[2] = pix[2] * (1.0f - t[2]) + t[2] * cb_tab[2][p2];

      premul_float_to_straight_uchar(ptr, pix);
      ptr += 4;
      mask_ptr += 4;
    }
  }
  else {
    /* No mask. */
    while (ptr < ptr_end) {
      float pix[4];
      straight_uchar_to_premul_float(pix, ptr);

      int p0 = int(pix[0] * (CB_TABLE_SIZE - 1.0f) + 0.5f);
      int p1 = int(pix[1] * (CB_TABLE_SIZE - 1.0f) + 0.5f);
      int p2 = int(pix[2] * (CB_TABLE_SIZE - 1.0f) + 0.5f);
      pix[0] = cb_tab[0][p0];
      pix[1] = cb_tab[1][p1];
      pix[2] = cb_tab[2][p2];
      premul_float_to_straight_uchar(ptr, pix);
      ptr += 4;
    }
  }
}

static void color_balance_float(const StripColorBalance *cb,
                                float *rect_float,
                                const float *mask_rect_float,
                                int width,
                                int height,
                                float mul)
{
  float *ptr = rect_float;
  const float *ptr_end = rect_float + int64_t(width) * height * 4;
  const float *mask_ptr = mask_rect_float;

  if (cb->method == SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN) {
    /* Lift/Gamma/Gain */
    const float3 lift = cb->lift;
    const float3 gain = cb->gain;
    const float3 gamma = cb->gamma;
    while (ptr < ptr_end) {
      float t0 = color_balance_lgg(ptr[0], lift.x, gain.x, gamma.x, mul);
      float t1 = color_balance_lgg(ptr[1], lift.y, gain.y, gamma.y, mul);
      float t2 = color_balance_lgg(ptr[2], lift.z, gain.z, gamma.z, mul);
      if (mask_ptr) {
        ptr[0] = ptr[0] * (1.0f - mask_ptr[0]) + t0 * mask_ptr[0];
        ptr[1] = ptr[1] * (1.0f - mask_ptr[1]) + t1 * mask_ptr[1];
        ptr[2] = ptr[2] * (1.0f - mask_ptr[2]) + t2 * mask_ptr[2];
      }
      else {
        ptr[0] = t0;
        ptr[1] = t1;
        ptr[2] = t2;
      }
      ptr += 4;
      if (mask_ptr) {
        mask_ptr += 4;
      }
    }
  }
  else {
    /* Slope/Offset/Power */
    const float3 slope = cb->slope;
    const float3 offset = cb->offset;
    const float3 power = cb->power;
    while (ptr < ptr_end) {
      float t0 = color_balance_sop(ptr[0], slope.x, offset.x, power.x, mul);
      float t1 = color_balance_sop(ptr[1], slope.y, offset.y, power.y, mul);
      float t2 = color_balance_sop(ptr[2], slope.z, offset.z, power.z, mul);
      if (mask_ptr) {
        ptr[0] = ptr[0] * (1.0f - mask_ptr[0]) + t0 * mask_ptr[0];
        ptr[1] = ptr[1] * (1.0f - mask_ptr[1]) + t1 * mask_ptr[1];
        ptr[2] = ptr[2] * (1.0f - mask_ptr[2]) + t2 * mask_ptr[2];
      }
      else {
        ptr[0] = t0;
        ptr[1] = t1;
        ptr[2] = t2;
      }
      ptr += 4;
      if (mask_ptr) {
        mask_ptr += 4;
      }
    }
  }
}

static void colorBalance_init_data(SequenceModifierData *smd)
{
  ColorBalanceModifierData *cbmd = (ColorBalanceModifierData *)smd;

  cbmd->color_multiply = 1.0f;
  cbmd->color_balance.method = 0;

  for (int c = 0; c < 3; c++) {
    cbmd->color_balance.lift[c] = 1.0f;
    cbmd->color_balance.gamma[c] = 1.0f;
    cbmd->color_balance.gain[c] = 1.0f;
    cbmd->color_balance.slope[c] = 1.0f;
    cbmd->color_balance.offset[c] = 1.0f;
    cbmd->color_balance.power[c] = 1.0f;
  }
}

static void colorBalance_apply(const StripScreenQuad & /*quad*/,
                               SequenceModifierData *smd,
                               ImBuf *ibuf,
                               ImBuf *mask)
{
  const ColorBalanceModifierData *cbmd = (const ColorBalanceModifierData *)smd;

  const StripColorBalance cb = calc_cb(&cbmd->color_balance);
  const float mul = cbmd->color_multiply;

  /* When working on non-float image, precalculate CB LUTs. */
  float cb_tab[3][CB_TABLE_SIZE];
  if (ibuf->float_buffer.data == nullptr) {
    for (int c = 0; c < 3; c++) {
      if (cb.method == SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN) {
        make_cb_table_lgg(cb.lift[c], cb.gain[c], cb.gamma[c], mul, cb_tab[c]);
      }
      else {
        make_cb_table_sop(cb.slope[c], cb.offset[c], cb.power[c], mul, cb_tab[c]);
      }
    }
  }

  threading::parallel_for(IndexRange(ibuf->y), 32, [&](const IndexRange y_range) {
    const int64_t offset = y_range.first() * ibuf->x * 4;
    const int y_size = int(y_range.size());
    if (ibuf->float_buffer.data != nullptr) {
      /* Float pixels. */
      color_balance_float(&cb,
                          ibuf->float_buffer.data + offset,
                          mask ? mask->float_buffer.data + offset : nullptr,
                          ibuf->x,
                          y_size,
                          mul);
    }
    else {
      /* Byte pixels. */
      color_balance_byte(cb_tab,
                         ibuf->byte_buffer.data + offset,
                         mask ? mask->byte_buffer.data + offset : nullptr,
                         ibuf->x,
                         y_size);
    }
  });
}

static SequenceModifierTypeInfo seqModifier_ColorBalance = {
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Color Balance"),
    /*struct_name*/ "ColorBalanceModifierData",
    /*struct_size*/ sizeof(ColorBalanceModifierData),
    /*init_data*/ colorBalance_init_data,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*apply*/ colorBalance_apply,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name White Balance Modifier
 * \{ */

static void whiteBalance_init_data(SequenceModifierData *smd)
{
  WhiteBalanceModifierData *cbmd = (WhiteBalanceModifierData *)smd;
  copy_v3_fl(cbmd->white_value, 1.0f);
}

struct WhiteBalanceThreadData {
  float white[3];
};

static void whiteBalance_apply_threaded(int width,
                                        int height,
                                        uchar *rect,
                                        float *rect_float,
                                        uchar *mask_rect,
                                        const float *mask_rect_float,
                                        void *data_v)
{
  int x, y;
  float multiplier[3];

  WhiteBalanceThreadData *data = (WhiteBalanceThreadData *)data_v;

  multiplier[0] = (data->white[0] != 0.0f) ? 1.0f / data->white[0] : FLT_MAX;
  multiplier[1] = (data->white[1] != 0.0f) ? 1.0f / data->white[1] : FLT_MAX;
  multiplier[2] = (data->white[2] != 0.0f) ? 1.0f / data->white[2] : FLT_MAX;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      int pixel_index = (y * width + x) * 4;
      float rgba[4], result[4], mask[3] = {1.0f, 1.0f, 1.0f};

      if (rect_float) {
        copy_v3_v3(rgba, rect_float + pixel_index);
      }
      else {
        straight_uchar_to_premul_float(rgba, rect + pixel_index);
      }

      copy_v4_v4(result, rgba);
#if 0
      mul_v3_v3(result, multiplier);
#else
      /* similar to division without the clipping */
      for (int i = 0; i < 3; i++) {
        result[i] = 1.0f - powf(1.0f - rgba[i], multiplier[i]);
      }
#endif

      if (mask_rect_float) {
        copy_v3_v3(mask, mask_rect_float + pixel_index);
      }
      else if (mask_rect) {
        rgb_uchar_to_float(mask, mask_rect + pixel_index);
      }

      result[0] = rgba[0] * (1.0f - mask[0]) + result[0] * mask[0];
      result[1] = rgba[1] * (1.0f - mask[1]) + result[1] * mask[1];
      result[2] = rgba[2] * (1.0f - mask[2]) + result[2] * mask[2];

      if (rect_float) {
        copy_v3_v3(rect_float + pixel_index, result);
      }
      else {
        premul_float_to_straight_uchar(rect + pixel_index, result);
      }
    }
  }
}

static void whiteBalance_apply(const StripScreenQuad & /*quad*/,
                               SequenceModifierData *smd,
                               ImBuf *ibuf,
                               ImBuf *mask)
{
  WhiteBalanceThreadData data;
  WhiteBalanceModifierData *wbmd = (WhiteBalanceModifierData *)smd;

  copy_v3_v3(data.white, wbmd->white_value);

  modifier_apply_threaded(ibuf, mask, whiteBalance_apply_threaded, &data);
}

static SequenceModifierTypeInfo seqModifier_WhiteBalance = {
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "White Balance"),
    /*struct_name*/ "WhiteBalanceModifierData",
    /*struct_size*/ sizeof(WhiteBalanceModifierData),
    /*init_data*/ whiteBalance_init_data,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*apply*/ whiteBalance_apply,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curves Modifier
 * \{ */

static void curves_init_data(SequenceModifierData *smd)
{
  CurvesModifierData *cmd = (CurvesModifierData *)smd;

  BKE_curvemapping_set_defaults(&cmd->curve_mapping, 4, 0.0f, 0.0f, 1.0f, 1.0f, HD_AUTO);
}

static void curves_free_data(SequenceModifierData *smd)
{
  CurvesModifierData *cmd = (CurvesModifierData *)smd;

  BKE_curvemapping_free_data(&cmd->curve_mapping);
}

static void curves_copy_data(SequenceModifierData *target, SequenceModifierData *smd)
{
  CurvesModifierData *cmd = (CurvesModifierData *)smd;
  CurvesModifierData *cmd_target = (CurvesModifierData *)target;

  BKE_curvemapping_copy_data(&cmd_target->curve_mapping, &cmd->curve_mapping);
}

static void curves_apply_threaded(int width,
                                  int height,
                                  uchar *rect,
                                  float *rect_float,
                                  uchar *mask_rect,
                                  const float *mask_rect_float,
                                  void *data_v)
{
  CurveMapping *curve_mapping = (CurveMapping *)data_v;
  int x, y;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      int pixel_index = (y * width + x) * 4;

      if (rect_float) {
        float *pixel = rect_float + pixel_index;
        float result[3];

        BKE_curvemapping_evaluate_premulRGBF(curve_mapping, result, pixel);

        if (mask_rect_float) {
          const float *m = mask_rect_float + pixel_index;

          pixel[0] = pixel[0] * (1.0f - m[0]) + result[0] * m[0];
          pixel[1] = pixel[1] * (1.0f - m[1]) + result[1] * m[1];
          pixel[2] = pixel[2] * (1.0f - m[2]) + result[2] * m[2];
        }
        else {
          pixel[0] = result[0];
          pixel[1] = result[1];
          pixel[2] = result[2];
        }
      }
      if (rect) {
        uchar *pixel = rect + pixel_index;
        float result[3], tempc[4];

        straight_uchar_to_premul_float(tempc, pixel);

        BKE_curvemapping_evaluate_premulRGBF(curve_mapping, result, tempc);

        if (mask_rect) {
          float t[3];

          rgb_uchar_to_float(t, mask_rect + pixel_index);

          tempc[0] = tempc[0] * (1.0f - t[0]) + result[0] * t[0];
          tempc[1] = tempc[1] * (1.0f - t[1]) + result[1] * t[1];
          tempc[2] = tempc[2] * (1.0f - t[2]) + result[2] * t[2];
        }
        else {
          tempc[0] = result[0];
          tempc[1] = result[1];
          tempc[2] = result[2];
        }

        premul_float_to_straight_uchar(pixel, tempc);
      }
    }
  }
}

static void curves_apply(const StripScreenQuad & /*quad*/,
                         SequenceModifierData *smd,
                         ImBuf *ibuf,
                         ImBuf *mask)
{
  CurvesModifierData *cmd = (CurvesModifierData *)smd;

  const float black[3] = {0.0f, 0.0f, 0.0f};
  const float white[3] = {1.0f, 1.0f, 1.0f};

  BKE_curvemapping_init(&cmd->curve_mapping);

  BKE_curvemapping_premultiply(&cmd->curve_mapping, false);
  BKE_curvemapping_set_black_white(&cmd->curve_mapping, black, white);

  modifier_apply_threaded(ibuf, mask, curves_apply_threaded, &cmd->curve_mapping);

  BKE_curvemapping_premultiply(&cmd->curve_mapping, true);
}

static SequenceModifierTypeInfo seqModifier_Curves = {
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Curves"),
    /*struct_name*/ "CurvesModifierData",
    /*struct_size*/ sizeof(CurvesModifierData),
    /*init_data*/ curves_init_data,
    /*free_data*/ curves_free_data,
    /*copy_data*/ curves_copy_data,
    /*apply*/ curves_apply,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hue Correct Modifier
 * \{ */

static void hue_correct_init_data(SequenceModifierData *smd)
{
  HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;
  int c;

  BKE_curvemapping_set_defaults(&hcmd->curve_mapping, 1, 0.0f, 0.0f, 1.0f, 1.0f, HD_AUTO);
  hcmd->curve_mapping.preset = CURVE_PRESET_MID8;

  for (c = 0; c < 3; c++) {
    CurveMap *cuma = &hcmd->curve_mapping.cm[c];
    BKE_curvemap_reset(
        cuma, &hcmd->curve_mapping.clipr, hcmd->curve_mapping.preset, CURVEMAP_SLOPE_POSITIVE);
  }
  /* use wrapping for all hue correct modifiers */
  hcmd->curve_mapping.flag |= CUMA_USE_WRAPPING;
  /* default to showing Saturation */
  hcmd->curve_mapping.cur = 1;
}

static void hue_correct_free_data(SequenceModifierData *smd)
{
  HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;

  BKE_curvemapping_free_data(&hcmd->curve_mapping);
}

static void hue_correct_copy_data(SequenceModifierData *target, SequenceModifierData *smd)
{
  HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;
  HueCorrectModifierData *hcmd_target = (HueCorrectModifierData *)target;

  BKE_curvemapping_copy_data(&hcmd_target->curve_mapping, &hcmd->curve_mapping);
}

static void hue_correct_apply_threaded(int width,
                                       int height,
                                       uchar *rect,
                                       float *rect_float,
                                       uchar *mask_rect,
                                       const float *mask_rect_float,
                                       void *data_v)
{
  CurveMapping *curve_mapping = (CurveMapping *)data_v;
  int x, y;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      int pixel_index = (y * width + x) * 4;
      float pixel[3], result[3], mask[3] = {1.0f, 1.0f, 1.0f};
      float hsv[3], f;

      if (rect_float) {
        copy_v3_v3(pixel, rect_float + pixel_index);
      }
      else {
        rgb_uchar_to_float(pixel, rect + pixel_index);
      }

      rgb_to_hsv(pixel[0], pixel[1], pixel[2], hsv, hsv + 1, hsv + 2);

      /* adjust hue, scaling returned default 0.5 up to 1 */
      f = BKE_curvemapping_evaluateF(curve_mapping, 0, hsv[0]);
      hsv[0] += f - 0.5f;

      /* adjust saturation, scaling returned default 0.5 up to 1 */
      f = BKE_curvemapping_evaluateF(curve_mapping, 1, hsv[0]);
      hsv[1] *= (f * 2.0f);

      /* adjust value, scaling returned default 0.5 up to 1 */
      f = BKE_curvemapping_evaluateF(curve_mapping, 2, hsv[0]);
      hsv[2] *= (f * 2.0f);

      hsv[0] = hsv[0] - floorf(hsv[0]); /* mod 1.0 */
      CLAMP(hsv[1], 0.0f, 1.0f);

      /* convert back to rgb */
      hsv_to_rgb(hsv[0], hsv[1], hsv[2], result, result + 1, result + 2);

      if (mask_rect_float) {
        copy_v3_v3(mask, mask_rect_float + pixel_index);
      }
      else if (mask_rect) {
        rgb_uchar_to_float(mask, mask_rect + pixel_index);
      }

      result[0] = pixel[0] * (1.0f - mask[0]) + result[0] * mask[0];
      result[1] = pixel[1] * (1.0f - mask[1]) + result[1] * mask[1];
      result[2] = pixel[2] * (1.0f - mask[2]) + result[2] * mask[2];

      if (rect_float) {
        copy_v3_v3(rect_float + pixel_index, result);
      }
      else {
        rgb_float_to_uchar(rect + pixel_index, result);
      }
    }
  }
}

static void hue_correct_apply(const StripScreenQuad & /*quad*/,
                              SequenceModifierData *smd,
                              ImBuf *ibuf,
                              ImBuf *mask)
{
  HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;

  BKE_curvemapping_init(&hcmd->curve_mapping);

  modifier_apply_threaded(ibuf, mask, hue_correct_apply_threaded, &hcmd->curve_mapping);
}

static SequenceModifierTypeInfo seqModifier_HueCorrect = {
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Hue Correct"),
    /*struct_name*/ "HueCorrectModifierData",
    /*struct_size*/ sizeof(HueCorrectModifierData),
    /*init_data*/ hue_correct_init_data,
    /*free_data*/ hue_correct_free_data,
    /*copy_data*/ hue_correct_copy_data,
    /*apply*/ hue_correct_apply,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Brightness/Contrast Modifier
 * \{ */

struct BrightContrastThreadData {
  float bright;
  float contrast;
};

static void brightcontrast_apply_threaded(int width,
                                          int height,
                                          uchar *rect,
                                          float *rect_float,
                                          uchar *mask_rect,
                                          const float *mask_rect_float,
                                          void *data_v)
{
  BrightContrastThreadData *data = (BrightContrastThreadData *)data_v;
  int x, y;

  float i;
  int c;
  float a, b, v;
  const float brightness = data->bright / 100.0f;
  const float contrast = data->contrast;
  float delta = contrast / 200.0f;
  /*
   * The algorithm is by Werner D. Streidt
   * (http://visca.com/ffactory/archives/5-99/msg00021.html)
   * Extracted of OpenCV `demhist.c`.
   */
  if (contrast > 0) {
    a = 1.0f - delta * 2.0f;
    a = 1.0f / max_ff(a, FLT_EPSILON);
    b = a * (brightness - delta);
  }
  else {
    delta *= -1;
    a = max_ff(1.0f - delta * 2.0f, 0.0f);
    b = a * brightness + delta;
  }

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      int pixel_index = (y * width + x) * 4;

      if (rect) {
        uchar *pixel = rect + pixel_index;

        for (c = 0; c < 3; c++) {
          i = float(pixel[c]) / 255.0f;
          v = a * i + b;

          if (mask_rect) {
            const uchar *m = mask_rect + pixel_index;
            const float t = float(m[c]) / 255.0f;

            v = float(pixel[c]) / 255.0f * (1.0f - t) + v * t;
          }

          pixel[c] = unit_float_to_uchar_clamp(v);
        }
      }
      else if (rect_float) {
        float *pixel = rect_float + pixel_index;

        for (c = 0; c < 3; c++) {
          i = pixel[c];
          v = a * i + b;

          if (mask_rect_float) {
            const float *m = mask_rect_float + pixel_index;

            pixel[c] = pixel[c] * (1.0f - m[c]) + v * m[c];
          }
          else {
            pixel[c] = v;
          }
        }
      }
    }
  }
}

static void brightcontrast_apply(const StripScreenQuad & /*quad*/,
                                 SequenceModifierData *smd,
                                 ImBuf *ibuf,
                                 ImBuf *mask)
{
  const BrightContrastModifierData *bcmd = (BrightContrastModifierData *)smd;
  BrightContrastThreadData data;

  data.bright = bcmd->bright;
  data.contrast = bcmd->contrast;

  modifier_apply_threaded(ibuf, mask, brightcontrast_apply_threaded, &data);
}

static SequenceModifierTypeInfo seqModifier_BrightContrast = {
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Brightness/Contrast"),
    /*struct_name*/ "BrightContrastModifierData",
    /*struct_size*/ sizeof(BrightContrastModifierData),
    /*init_data*/ nullptr,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*apply*/ brightcontrast_apply,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mask Modifier
 * \{ */

static void maskmodifier_apply_threaded(int width,
                                        int height,
                                        uchar *rect,
                                        float *rect_float,
                                        uchar *mask_rect,
                                        const float *mask_rect_float,
                                        void * /*data_v*/)
{
  int x, y;

  if (rect && !mask_rect) {
    return;
  }

  if (rect_float && !mask_rect_float) {
    return;
  }

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      const int pixel_index = (y * width + x) * 4;

      if (rect) {
        const uchar *mask_pixel = mask_rect + pixel_index;
        const uchar mask = min_iii(mask_pixel[0], mask_pixel[1], mask_pixel[2]);
        uchar *pixel = rect + pixel_index;

        /* byte buffer is straight, so only affect on alpha itself,
         * this is the only way to alpha-over byte strip after
         * applying mask modifier.
         */
        pixel[3] = float(pixel[3] * mask) / 255.0f;
      }
      else if (rect_float) {
        const float *mask_pixel = mask_rect_float + pixel_index;
        const float mask = min_fff(mask_pixel[0], mask_pixel[1], mask_pixel[2]);
        float *pixel = rect_float + pixel_index;

        /* float buffers are premultiplied, so need to premul color
         * as well to make it easy to alpha-over masted strip.
         */
        for (int c = 0; c < 4; c++) {
          pixel[c] = pixel[c] * mask;
        }
      }
    }
  }
}

static void maskmodifier_apply(const StripScreenQuad & /*quad*/,
                               SequenceModifierData * /*smd*/,
                               ImBuf *ibuf,
                               ImBuf *mask)
{
  // SequencerMaskModifierData *bcmd = (SequencerMaskModifierData *)smd;

  modifier_apply_threaded(ibuf, mask, maskmodifier_apply_threaded, nullptr);
  ibuf->planes = R_IMF_PLANES_RGBA;
}

static SequenceModifierTypeInfo seqModifier_Mask = {
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Mask"),
    /*struct_name*/ "SequencerMaskModifierData",
    /*struct_size*/ sizeof(SequencerMaskModifierData),
    /*init_data*/ nullptr,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*apply*/ maskmodifier_apply,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tonemap Modifier
 * \{ */

struct AvgLogLum {
  const SequencerTonemapModifierData *tmmd;
  float al;
  float auto_key;
  float lav;
  float3 cav;
  float igm;
};

static void tonemapmodifier_init_data(SequenceModifierData *smd)
{
  SequencerTonemapModifierData *tmmd = (SequencerTonemapModifierData *)smd;
  /* Same as tone-map compositor node. */
  tmmd->type = SEQ_TONEMAP_RD_PHOTORECEPTOR;
  tmmd->key = 0.18f;
  tmmd->offset = 1.0f;
  tmmd->gamma = 1.0f;
  tmmd->intensity = 0.0f;
  tmmd->contrast = 0.0f;
  tmmd->adaptation = 1.0f;
  tmmd->correction = 0.0f;
}

/* Convert chunk of float image pixels to scene linear space, in-place. */
static void pixels_to_scene_linear_float(ColorSpace *colorspace, float4 *pixels, int64_t count)
{
  IMB_colormanagement_colorspace_to_scene_linear(
      (float *)(pixels), int(count), 1, 4, colorspace, false);
}

/* Convert chunk of byte image pixels to scene linear space, into a destination array. */
static void pixels_to_scene_linear_byte(ColorSpace *colorspace,
                                        const uchar *pixels,
                                        float4 *dst,
                                        int64_t count)
{
  const uchar *bptr = pixels;
  float4 *dst_ptr = dst;
  for (int64_t i = 0; i < count; i++) {
    straight_uchar_to_premul_float(*dst_ptr, bptr);
    bptr += 4;
    dst_ptr++;
  }
  IMB_colormanagement_colorspace_to_scene_linear(
      (float *)dst, int(count), 1, 4, colorspace, false);
}

static void scene_linear_to_image_chunk_float(ImBuf *ibuf, IndexRange range)
{
  ColorSpace *colorspace = ibuf->float_buffer.colorspace;
  float4 *fptr = reinterpret_cast<float4 *>(ibuf->float_buffer.data);
  IMB_colormanagement_scene_linear_to_colorspace(
      (float *)(fptr + range.first()), int(range.size()), 1, 4, colorspace);
}

static void scene_linear_to_image_chunk_byte(float4 *src, ImBuf *ibuf, IndexRange range)
{
  ColorSpace *colorspace = ibuf->byte_buffer.colorspace;
  IMB_colormanagement_scene_linear_to_colorspace(
      (float *)src, int(range.size()), 1, 4, colorspace);
  const float4 *src_ptr = src;
  uchar *bptr = ibuf->byte_buffer.data;
  for (const int64_t idx : range) {
    premul_float_to_straight_uchar(bptr + idx * 4, *src_ptr);
    src_ptr++;
  }
}

static void tonemap_simple(float4 *scene_linear,
                           ImBuf *mask,
                           IndexRange range,
                           const AvgLogLum &avg)
{
  const float4 *mask_float = mask != nullptr ? (const float4 *)mask->float_buffer.data : nullptr;
  const uchar4 *mask_byte = mask != nullptr ? (const uchar4 *)mask->byte_buffer.data : nullptr;

  int64_t index = 0;
  for (const int64_t pixel_index : range) {
    float4 input = scene_linear[index];

    /* Apply correction. */
    float3 pixel = input.xyz() * avg.al;
    float3 d = pixel + avg.tmmd->offset;
    pixel.x /= (d.x == 0.0f) ? 1.0f : d.x;
    pixel.y /= (d.y == 0.0f) ? 1.0f : d.y;
    pixel.z /= (d.z == 0.0f) ? 1.0f : d.z;
    const float igm = avg.igm;
    if (igm != 0.0f) {
      pixel.x = powf(math::max(pixel.x, 0.0f), igm);
      pixel.y = powf(math::max(pixel.y, 0.0f), igm);
      pixel.z = powf(math::max(pixel.z, 0.0f), igm);
    }

    /* Apply mask. */
    if (mask != nullptr) {
      float3 msk(1.0f);
      if (mask_float != nullptr) {
        msk = mask_float[pixel_index].xyz();
      }
      else if (mask_byte != nullptr) {
        rgb_uchar_to_float(msk, mask_byte[pixel_index]);
      }
      pixel = math::interpolate(input.xyz(), pixel, msk);
    }

    scene_linear[index] = float4(pixel.x, pixel.y, pixel.z, input.w);
    index++;
  }
}

static void tonemap_rd_photoreceptor(float4 *scene_linear,
                                     ImBuf *mask,
                                     IndexRange range,
                                     const AvgLogLum &avg)
{
  const float4 *mask_float = mask != nullptr ? (const float4 *)mask->float_buffer.data : nullptr;
  const uchar4 *mask_byte = mask != nullptr ? (const uchar4 *)mask->byte_buffer.data : nullptr;

  const float f = expf(-avg.tmmd->intensity);
  const float m = (avg.tmmd->contrast > 0.0f) ? avg.tmmd->contrast :
                                                (0.3f + 0.7f * powf(avg.auto_key, 1.4f));
  const float ic = 1.0f - avg.tmmd->correction, ia = 1.0f - avg.tmmd->adaptation;

  int64_t index = 0;
  for (const int64_t pixel_index : range) {
    float4 input = scene_linear[index];

    /* Apply correction. */
    float3 pixel = input.xyz();
    const float L = IMB_colormanagement_get_luminance(pixel);
    float I_l = pixel.x + ic * (L - pixel.x);
    float I_g = avg.cav.x + ic * (avg.lav - avg.cav.x);
    float I_a = I_l + ia * (I_g - I_l);
    pixel.x /= std::max(pixel.x + powf(f * I_a, m), 1.0e-30f);
    I_l = pixel.y + ic * (L - pixel.y);
    I_g = avg.cav.y + ic * (avg.lav - avg.cav.y);
    I_a = I_l + ia * (I_g - I_l);
    pixel.y /= std::max(pixel.y + powf(f * I_a, m), 1.0e-30f);
    I_l = pixel.z + ic * (L - pixel.z);
    I_g = avg.cav.z + ic * (avg.lav - avg.cav.z);
    I_a = I_l + ia * (I_g - I_l);
    pixel.z /= std::max(pixel.z + powf(f * I_a, m), 1.0e-30f);

    /* Apply mask. */
    if (mask != nullptr) {
      float3 msk(1.0f);
      if (mask_float != nullptr) {
        msk = mask_float[pixel_index].xyz();
      }
      else if (mask_byte != nullptr) {
        rgb_uchar_to_float(msk, mask_byte[pixel_index]);
      }
      pixel = math::interpolate(input.xyz(), pixel, msk);
    }

    scene_linear[index] = float4(pixel.x, pixel.y, pixel.z, input.w);
    index++;
  }
}

static bool is_point_inside_quad(const StripScreenQuad &quad, int x, int y)
{
  float2 pt(x + 0.5f, y + 0.5f);
  return isect_point_quad_v2(pt, quad.v0, quad.v1, quad.v2, quad.v3);
}

struct AreaLuminance {
  int64_t pixel_count = 0;
  double sum = 0.0f;
  float3 color_sum = {0, 0, 0};
  double log_sum = 0.0;
  float min = FLT_MAX;
  float max = -FLT_MAX;
};

static void tonemap_calc_chunk_luminance(const StripScreenQuad &quad,
                                         const bool all_pixels_inside_quad,
                                         const int width,
                                         const IndexRange y_range,
                                         const float4 *scene_linear,
                                         AreaLuminance &r_lum)
{
  for (const int y : y_range) {
    for (int x = 0; x < width; x++) {
      if (all_pixels_inside_quad || is_point_inside_quad(quad, x, y)) {
        float4 pixel = *scene_linear;
        r_lum.pixel_count++;
        float L = IMB_colormanagement_get_luminance(pixel);
        r_lum.sum += L;
        r_lum.color_sum.x += pixel.x;
        r_lum.color_sum.y += pixel.y;
        r_lum.color_sum.z += pixel.z;
        r_lum.log_sum += logf(math::max(L, 0.0f) + 1e-5f);
        r_lum.max = math::max(r_lum.max, L);
        r_lum.min = math::min(r_lum.min, L);
      }
      scene_linear++;
    }
  }
}

static AreaLuminance tonemap_calc_input_luminance(const StripScreenQuad &quad, const ImBuf *ibuf)
{
  /* Pixels outside the pre-transform strip area are ignored for luminance calculations.
   * If strip area covers whole image, we can trivially accept all pixels. */
  const bool all_pixels_inside_quad = is_point_inside_quad(quad, 0, 0) &&
                                      is_point_inside_quad(quad, ibuf->x - 1, 0) &&
                                      is_point_inside_quad(quad, 0, ibuf->y - 1) &&
                                      is_point_inside_quad(quad, ibuf->x - 1, ibuf->y - 1);

  AreaLuminance lum;
  lum = threading::parallel_reduce(
      IndexRange(ibuf->y),
      32,
      lum,
      /* Calculate luminance for a chunk. */
      [&](const IndexRange y_range, const AreaLuminance &init) {
        AreaLuminance lum = init;
        const int64_t chunk_size = y_range.size() * ibuf->x;
        /* For float images, convert to scene-linear in place. The rest
         * of tone-mapper can then continue with scene-linear values. */
        if (ibuf->float_buffer.data != nullptr) {
          float4 *fptr = reinterpret_cast<float4 *>(ibuf->float_buffer.data);
          fptr += y_range.first() * ibuf->x;
          pixels_to_scene_linear_float(ibuf->float_buffer.colorspace, fptr, chunk_size);
          tonemap_calc_chunk_luminance(quad, all_pixels_inside_quad, ibuf->x, y_range, fptr, lum);
        }
        else {
          const uchar *bptr = ibuf->byte_buffer.data + y_range.first() * ibuf->x * 4;
          Array<float4> scene_linear(chunk_size);
          pixels_to_scene_linear_byte(
              ibuf->byte_buffer.colorspace, bptr, scene_linear.data(), chunk_size);
          tonemap_calc_chunk_luminance(
              quad, all_pixels_inside_quad, ibuf->x, y_range, scene_linear.data(), lum);
        }
        return lum;
      },
      /* Reduce luminance results. */
      [&](const AreaLuminance &a, const AreaLuminance &b) {
        AreaLuminance res;
        res.pixel_count = a.pixel_count + b.pixel_count;
        res.sum = a.sum + b.sum;
        res.color_sum = a.color_sum + b.color_sum;
        res.log_sum = a.log_sum + b.log_sum;
        res.min = math::min(a.min, b.min);
        res.max = math::max(a.max, b.max);
        return res;
      });
  return lum;
}

static void tonemapmodifier_apply(const StripScreenQuad &quad,
                                  SequenceModifierData *smd,
                                  ImBuf *ibuf,
                                  ImBuf *mask)
{
  const SequencerTonemapModifierData *tmmd = (const SequencerTonemapModifierData *)smd;

  AreaLuminance lum = tonemap_calc_input_luminance(quad, ibuf);
  if (lum.pixel_count == 0) {
    return; /* Strip is zero size or off-screen. */
  }

  AvgLogLum data;
  data.tmmd = tmmd;
  data.lav = lum.sum / lum.pixel_count;
  data.cav.x = lum.color_sum.x / lum.pixel_count;
  data.cav.y = lum.color_sum.y / lum.pixel_count;
  data.cav.z = lum.color_sum.z / lum.pixel_count;
  float maxl = log(double(lum.max) + 1e-5f);
  float minl = log(double(lum.min) + 1e-5f);
  float avl = lum.log_sum / lum.pixel_count;
  data.auto_key = (maxl > minl) ? ((maxl - avl) / (maxl - minl)) : 1.0f;
  float al = exp(double(avl));
  data.al = (al == 0.0f) ? 0.0f : (tmmd->key / al);
  data.igm = (tmmd->gamma == 0.0f) ? 1.0f : (1.0f / tmmd->gamma);

  threading::parallel_for(
      IndexRange(int64_t(ibuf->x) * ibuf->y), 64 * 1024, [&](IndexRange range) {
        if (ibuf->float_buffer.data != nullptr) {
          /* Float pixels: no need for temporary storage. Luminance calculation already converted
           * data to scene linear. */
          float4 *pixels = (float4 *)(ibuf->float_buffer.data) + range.first();
          if (tmmd->type == SEQ_TONEMAP_RD_PHOTORECEPTOR) {
            tonemap_rd_photoreceptor(pixels, mask, range, data);
          }
          else {
            BLI_assert(tmmd->type == SEQ_TONEMAP_RH_SIMPLE);
            tonemap_simple(pixels, mask, range, data);
          }
          scene_linear_to_image_chunk_float(ibuf, range);
        }
        else {
          /* Byte pixels: temporary storage for scene linear pixel values. */
          Array<float4> scene_linear(range.size());
          pixels_to_scene_linear_byte(ibuf->byte_buffer.colorspace,
                                      ibuf->byte_buffer.data + range.first() * 4,
                                      scene_linear.data(),
                                      range.size());
          if (tmmd->type == SEQ_TONEMAP_RD_PHOTORECEPTOR) {
            tonemap_rd_photoreceptor(scene_linear.data(), mask, range, data);
          }
          else {
            BLI_assert(tmmd->type == SEQ_TONEMAP_RH_SIMPLE);
            tonemap_simple(scene_linear.data(), mask, range, data);
          }
          scene_linear_to_image_chunk_byte(scene_linear.data(), ibuf, range);
        }
      });
}

static SequenceModifierTypeInfo seqModifier_Tonemap = {
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Tonemap"),
    /*struct_name*/ "SequencerTonemapModifierData",
    /*struct_size*/ sizeof(SequencerTonemapModifierData),
    /*init_data*/ tonemapmodifier_init_data,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*apply*/ tonemapmodifier_apply,
};

static SequenceModifierTypeInfo seqModifier_SoundEqualizer = {
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Equalizer"),
    /*struct_name*/ "SoundEqualizerModifierData",
    /*struct_size*/ sizeof(SoundEqualizerModifierData),
    /*init_data*/ SEQ_sound_equalizermodifier_init_data,
    /*free_data*/ SEQ_sound_equalizermodifier_free,
    /*copy_data*/ SEQ_sound_equalizermodifier_copy_data,
    /*apply*/ nullptr,
};
/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Modifier Functions
 * \{ */

static void sequence_modifier_type_info_init()
{
#define INIT_TYPE(typeName) (modifiersTypes[seqModifierType_##typeName] = &seqModifier_##typeName)

  INIT_TYPE(ColorBalance);
  INIT_TYPE(Curves);
  INIT_TYPE(HueCorrect);
  INIT_TYPE(BrightContrast);
  INIT_TYPE(Mask);
  INIT_TYPE(WhiteBalance);
  INIT_TYPE(Tonemap);
  INIT_TYPE(SoundEqualizer);

#undef INIT_TYPE
}

const SequenceModifierTypeInfo *SEQ_modifier_type_info_get(int type)
{
  if (!modifierTypesInit) {
    sequence_modifier_type_info_init();
    modifierTypesInit = true;
  }

  return modifiersTypes[type];
}

SequenceModifierData *SEQ_modifier_new(Sequence *seq, const char *name, int type)
{
  SequenceModifierData *smd;
  const SequenceModifierTypeInfo *smti = SEQ_modifier_type_info_get(type);

  smd = static_cast<SequenceModifierData *>(MEM_callocN(smti->struct_size, "sequence modifier"));

  smd->type = type;
  smd->flag |= SEQUENCE_MODIFIER_EXPANDED;

  if (!name || !name[0]) {
    STRNCPY(smd->name, smti->name);
  }
  else {
    STRNCPY(smd->name, name);
  }

  BLI_addtail(&seq->modifiers, smd);

  SEQ_modifier_unique_name(seq, smd);

  if (smti->init_data) {
    smti->init_data(smd);
  }

  return smd;
}

bool SEQ_modifier_remove(Sequence *seq, SequenceModifierData *smd)
{
  if (BLI_findindex(&seq->modifiers, smd) == -1) {
    return false;
  }

  BLI_remlink(&seq->modifiers, smd);
  SEQ_modifier_free(smd);

  return true;
}

void SEQ_modifier_clear(Sequence *seq)
{
  SequenceModifierData *smd, *smd_next;

  for (smd = static_cast<SequenceModifierData *>(seq->modifiers.first); smd; smd = smd_next) {
    smd_next = smd->next;
    SEQ_modifier_free(smd);
  }

  BLI_listbase_clear(&seq->modifiers);
}

void SEQ_modifier_free(SequenceModifierData *smd)
{
  const SequenceModifierTypeInfo *smti = SEQ_modifier_type_info_get(smd->type);

  if (smti && smti->free_data) {
    smti->free_data(smd);
  }

  MEM_freeN(smd);
}

void SEQ_modifier_unique_name(Sequence *seq, SequenceModifierData *smd)
{
  const SequenceModifierTypeInfo *smti = SEQ_modifier_type_info_get(smd->type);

  BLI_uniquename(&seq->modifiers,
                 smd,
                 CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, smti->name),
                 '.',
                 offsetof(SequenceModifierData, name),
                 sizeof(smd->name));
}

SequenceModifierData *SEQ_modifier_find_by_name(Sequence *seq, const char *name)
{
  return static_cast<SequenceModifierData *>(
      BLI_findstring(&(seq->modifiers), name, offsetof(SequenceModifierData, name)));
}

static bool skip_modifier(Scene *scene, const SequenceModifierData *smd, int timeline_frame)
{
  using namespace blender::seq;

  if (smd->mask_sequence == nullptr) {
    return false;
  }
  const bool strip_has_ended_skip = smd->mask_input_type == SEQUENCE_MASK_INPUT_STRIP &&
                                    smd->mask_time == SEQUENCE_MASK_TIME_RELATIVE &&
                                    !SEQ_time_strip_intersects_frame(
                                        scene, smd->mask_sequence, timeline_frame);
  const bool missing_data_skip = !SEQ_sequence_has_valid_data(smd->mask_sequence) ||
                                 media_presence_is_missing(scene, smd->mask_sequence);

  return strip_has_ended_skip || missing_data_skip;
}

void SEQ_modifier_apply_stack(const SeqRenderData *context,
                              const Sequence *seq,
                              ImBuf *ibuf,
                              int timeline_frame)
{
  const StripScreenQuad quad = get_strip_screen_quad(context, seq);

  if (seq->modifiers.first && (seq->flag & SEQ_USE_LINEAR_MODIFIERS)) {
    SEQ_render_imbuf_from_sequencer_space(context->scene, ibuf);
  }

  LISTBASE_FOREACH (SequenceModifierData *, smd, &seq->modifiers) {
    const SequenceModifierTypeInfo *smti = SEQ_modifier_type_info_get(smd->type);

    /* could happen if modifier is being removed or not exists in current version of blender */
    if (!smti) {
      continue;
    }

    /* modifier is muted, do nothing */
    if (smd->flag & SEQUENCE_MODIFIER_MUTE) {
      continue;
    }

    if (smti->apply && !skip_modifier(context->scene, smd, timeline_frame)) {
      int frame_offset;
      if (smd->mask_time == SEQUENCE_MASK_TIME_RELATIVE) {
        frame_offset = seq->start;
      }
      else /* if (smd->mask_time == SEQUENCE_MASK_TIME_ABSOLUTE) */ {
        frame_offset = smd->mask_id ? ((Mask *)smd->mask_id)->sfra : 0;
      }

      ImBuf *mask = modifier_mask_get(
          smd, context, timeline_frame, frame_offset, ibuf->float_buffer.data != nullptr);

      smti->apply(quad, smd, ibuf, mask);

      if (mask) {
        IMB_freeImBuf(mask);
      }
    }
  }

  if (seq->modifiers.first && (seq->flag & SEQ_USE_LINEAR_MODIFIERS)) {
    seq_imbuf_to_sequencer_space(context->scene, ibuf, false);
  }
}

void SEQ_modifier_list_copy(Sequence *seqn, Sequence *seq)
{
  LISTBASE_FOREACH (SequenceModifierData *, smd, &seq->modifiers) {
    SequenceModifierData *smdn;
    const SequenceModifierTypeInfo *smti = SEQ_modifier_type_info_get(smd->type);

    smdn = static_cast<SequenceModifierData *>(MEM_dupallocN(smd));

    if (smti && smti->copy_data) {
      smti->copy_data(smdn, smd);
    }

    BLI_addtail(&seqn->modifiers, smdn);
    BLI_uniquename(&seqn->modifiers,
                   smdn,
                   "Strip Modifier",
                   '.',
                   offsetof(SequenceModifierData, name),
                   sizeof(SequenceModifierData::name));
  }
}

int SEQ_sequence_supports_modifiers(Sequence *seq)
{
  return (seq->type != SEQ_TYPE_SOUND_RAM);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name .blend File I/O
 * \{ */

void SEQ_modifier_blend_write(BlendWriter *writer, ListBase *modbase)
{
  LISTBASE_FOREACH (SequenceModifierData *, smd, modbase) {
    const SequenceModifierTypeInfo *smti = SEQ_modifier_type_info_get(smd->type);

    if (smti) {
      BLO_write_struct_by_name(writer, smti->struct_name, smd);

      if (smd->type == seqModifierType_Curves) {
        CurvesModifierData *cmd = (CurvesModifierData *)smd;

        BKE_curvemapping_blend_write(writer, &cmd->curve_mapping);
      }
      else if (smd->type == seqModifierType_HueCorrect) {
        HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;

        BKE_curvemapping_blend_write(writer, &hcmd->curve_mapping);
      }
      else if (smd->type == seqModifierType_SoundEqualizer) {
        SoundEqualizerModifierData *semd = (SoundEqualizerModifierData *)smd;
        LISTBASE_FOREACH (EQCurveMappingData *, eqcmd, &semd->graphics) {
          BLO_write_struct_by_name(writer, "EQCurveMappingData", eqcmd);
          BKE_curvemapping_blend_write(writer, &eqcmd->curve_mapping);
        }
      }
    }
    else {
      BLO_write_struct(writer, SequenceModifierData, smd);
    }
  }
}

void SEQ_modifier_blend_read_data(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_struct_list(reader, SequenceModifierData, lb);

  LISTBASE_FOREACH (SequenceModifierData *, smd, lb) {
    if (smd->mask_sequence) {
      BLO_read_struct(reader, Sequence, &smd->mask_sequence);
    }

    if (smd->type == seqModifierType_Curves) {
      CurvesModifierData *cmd = (CurvesModifierData *)smd;

      BKE_curvemapping_blend_read(reader, &cmd->curve_mapping);
    }
    else if (smd->type == seqModifierType_HueCorrect) {
      HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;

      BKE_curvemapping_blend_read(reader, &hcmd->curve_mapping);
    }
    else if (smd->type == seqModifierType_SoundEqualizer) {
      SoundEqualizerModifierData *semd = (SoundEqualizerModifierData *)smd;
      BLO_read_struct_list(reader, EQCurveMappingData, &semd->graphics);
      LISTBASE_FOREACH (EQCurveMappingData *, eqcmd, &semd->graphics) {
        BKE_curvemapping_blend_read(reader, &eqcmd->curve_mapping);
      }
    }
  }
}

/** \} */
