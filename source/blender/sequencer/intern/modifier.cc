/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstddef>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BKE_colortools.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "SEQ_modifier.h"
#include "SEQ_render.h"

#include "BLO_read_write.h"

#include "render.h"

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
      seq_render_state_init(&state);

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

static StripColorBalance calc_cb_lgg(StripColorBalance *cb_)
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

static StripColorBalance calc_cb_sop(StripColorBalance *cb_)
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

static StripColorBalance calc_cb(StripColorBalance *cb_)
{
  if (cb_->method == SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN) {
    return calc_cb_lgg(cb_);
  }
  /* `cb_->method == SEQ_COLOR_BALANCE_METHOD_SLOPEOFFSETPOWER`. */
  return calc_cb_sop(cb_);
}

/* NOTE: lift is actually 2-lift. */
MINLINE float color_balance_fl_lgg(
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

MINLINE float color_balance_fl_sop(float in,
                                   const float slope,
                                   const float offset,
                                   const float power,
                                   const float pivot,
                                   float mul)
{
  float x = in * slope + offset;

  /* prevent NaN */
  if (x < 0.0f) {
    x = 0.0f;
  }

  x = powf(x / pivot, power) * pivot;
  x *= mul;
  CLAMP(x, FLT_MIN, FLT_MAX);
  return x;
}

static void make_cb_table_float_lgg(float lift, float gain, float gamma, float *table, float mul)
{
  for (int y = 0; y < 256; y++) {
    float v = color_balance_fl_lgg(float(y) * (1.0f / 255.0f), lift, gain, gamma, mul);

    table[y] = v;
  }
}

static void make_cb_table_float_sop(
    float slope, float offset, float power, float pivot, float *table, float mul)
{
  for (int y = 0; y < 256; y++) {
    float v = color_balance_fl_sop(float(y) * (1.0f / 255.0f), slope, offset, power, pivot, mul);

    table[y] = v;
  }
}

static void color_balance_byte_byte(
    StripColorBalance *cb_, uchar *rect, uchar *mask_rect, int width, int height, float mul)
{
  // uchar cb_tab[3][256];
  uchar *cp = rect;
  uchar *e = cp + width * 4 * height;
  uchar *m = mask_rect;

  StripColorBalance cb = calc_cb(cb_);

  while (cp < e) {
    float p[4];
    int c;

    straight_uchar_to_premul_float(p, cp);

    for (c = 0; c < 3; c++) {
      float t;
      if (cb.method == SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN) {
        t = color_balance_fl_lgg(p[c], cb.lift[c], cb.gain[c], cb.gamma[c], mul);
      }
      else {
        t = color_balance_fl_sop(p[c], cb.slope[c], cb.offset[c], cb.power[c], 1.0, mul);
      }

      if (m) {
        float m_normal = float(m[c]) / 255.0f;

        p[c] = p[c] * (1.0f - m_normal) + t * m_normal;
      }
      else {
        p[c] = t;
      }
    }

    premul_float_to_straight_uchar(cp, p);

    cp += 4;
    if (m) {
      m += 4;
    }
  }
}

static void color_balance_byte_float(StripColorBalance *cb_,
                                     uchar *rect,
                                     float *rect_float,
                                     uchar *mask_rect,
                                     int width,
                                     int height,
                                     float mul)
{
  float cb_tab[4][256];
  int c, i;
  uchar *p = rect;
  uchar *e = p + width * 4 * height;
  uchar *m = mask_rect;
  float *o;
  StripColorBalance cb;

  o = rect_float;

  cb = calc_cb(cb_);

  for (c = 0; c < 3; c++) {
    if (cb.method == SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN) {
      make_cb_table_float_lgg(cb.lift[c], cb.gain[c], cb.gamma[c], cb_tab[c], mul);
    }
    else {
      make_cb_table_float_sop(cb.slope[c], cb.offset[c], cb.power[c], 1.0, cb_tab[c], mul);
    }
  }

  for (i = 0; i < 256; i++) {
    cb_tab[3][i] = float(i) * (1.0f / 255.0f);
  }

  while (p < e) {
    if (m) {
      const float t[3] = {m[0] / 255.0f, m[1] / 255.0f, m[2] / 255.0f};

      p[0] = p[0] * (1.0f - t[0]) + t[0] * cb_tab[0][p[0]];
      p[1] = p[1] * (1.0f - t[1]) + t[1] * cb_tab[1][p[1]];
      p[2] = p[2] * (1.0f - t[2]) + t[2] * cb_tab[2][p[2]];

      m += 4;
    }
    else {
      o[0] = cb_tab[0][p[0]];
      o[1] = cb_tab[1][p[1]];
      o[2] = cb_tab[2][p[2]];
    }

    o[3] = cb_tab[3][p[3]];

    p += 4;
    o += 4;
  }
}

static void color_balance_float_float(StripColorBalance *cb_,
                                      float *rect_float,
                                      const float *mask_rect_float,
                                      int width,
                                      int height,
                                      float mul)
{
  float *p = rect_float;
  const float *e = rect_float + width * 4 * height;
  const float *m = mask_rect_float;
  StripColorBalance cb = calc_cb(cb_);

  while (p < e) {
    int c;
    for (c = 0; c < 3; c++) {
      float t;
      if (cb_->method == SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN) {
        t = color_balance_fl_lgg(p[c], cb.lift[c], cb.gain[c], cb.gamma[c], mul);
      }
      else {
        t = color_balance_fl_sop(p[c], cb.slope[c], cb.offset[c], cb.power[c], 1.0, mul);
      }

      if (m) {
        p[c] = p[c] * (1.0f - m[c]) + t * m[c];
      }
      else {
        p[c] = t;
      }
    }

    p += 4;
    if (m) {
      m += 4;
    }
  }
}

struct ColorBalanceInitData {
  StripColorBalance *cb;
  ImBuf *ibuf;
  float mul;
  ImBuf *mask;
  bool make_float;
};

struct ColorBalanceThread {
  StripColorBalance *cb;
  float mul;

  int width, height;

  uchar *rect, *mask_rect;
  float *rect_float, *mask_rect_float;

  bool make_float;
};

static void color_balance_init_handle(void *handle_v,
                                      int start_line,
                                      int tot_line,
                                      void *init_data_v)
{
  ColorBalanceThread *handle = (ColorBalanceThread *)handle_v;
  ColorBalanceInitData *init_data = (ColorBalanceInitData *)init_data_v;
  ImBuf *ibuf = init_data->ibuf;
  ImBuf *mask = init_data->mask;

  int offset = 4 * start_line * ibuf->x;

  memset(handle, 0, sizeof(ColorBalanceThread));

  handle->cb = init_data->cb;
  handle->mul = init_data->mul;
  handle->width = ibuf->x;
  handle->height = tot_line;
  handle->make_float = init_data->make_float;

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

static void *color_balance_do_thread(void *thread_data_v)
{
  ColorBalanceThread *thread_data = (ColorBalanceThread *)thread_data_v;
  StripColorBalance *cb = thread_data->cb;
  int width = thread_data->width, height = thread_data->height;
  uchar *rect = thread_data->rect;
  uchar *mask_rect = thread_data->mask_rect;
  float *rect_float = thread_data->rect_float;
  float *mask_rect_float = thread_data->mask_rect_float;
  float mul = thread_data->mul;

  if (rect_float) {
    color_balance_float_float(cb, rect_float, mask_rect_float, width, height, mul);
  }
  else if (thread_data->make_float) {
    color_balance_byte_float(cb, rect, rect_float, mask_rect, width, height, mul);
  }
  else {
    color_balance_byte_byte(cb, rect, mask_rect, width, height, mul);
  }

  return nullptr;
}

static void colorBalance_init_data(SequenceModifierData *smd)
{
  ColorBalanceModifierData *cbmd = (ColorBalanceModifierData *)smd;
  int c;

  cbmd->color_multiply = 1.0f;
  cbmd->color_balance.method = 0;

  for (c = 0; c < 3; c++) {
    cbmd->color_balance.lift[c] = 1.0f;
    cbmd->color_balance.gamma[c] = 1.0f;
    cbmd->color_balance.gain[c] = 1.0f;
    cbmd->color_balance.slope[c] = 1.0f;
    cbmd->color_balance.offset[c] = 1.0f;
    cbmd->color_balance.power[c] = 1.0f;
  }
}

static void modifier_color_balance_apply(
    StripColorBalance *cb, ImBuf *ibuf, float mul, bool make_float, ImBuf *mask_input)
{
  ColorBalanceInitData init_data;

  if (!ibuf->float_buffer.data && make_float) {
    imb_addrectfloatImBuf(ibuf, 4);
  }

  init_data.cb = cb;
  init_data.ibuf = ibuf;
  init_data.mul = mul;
  init_data.make_float = make_float;
  init_data.mask = mask_input;

  IMB_processor_apply_threaded(ibuf->y,
                               sizeof(ColorBalanceThread),
                               &init_data,
                               color_balance_init_handle,
                               color_balance_do_thread);

  /* color balance either happens on float buffer or byte buffer, but never on both,
   * free byte buffer if there's float buffer since float buffer would be used for
   * color balance in favor of byte buffer
   */
  if (ibuf->float_buffer.data && ibuf->byte_buffer.data) {
    imb_freerectImBuf(ibuf);
  }
}

static void colorBalance_apply(SequenceModifierData *smd, ImBuf *ibuf, ImBuf *mask)
{
  ColorBalanceModifierData *cbmd = (ColorBalanceModifierData *)smd;

  modifier_color_balance_apply(&cbmd->color_balance, ibuf, cbmd->color_multiply, false, mask);
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

static void whiteBalance_apply(SequenceModifierData *smd, ImBuf *ibuf, ImBuf *mask)
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

  BKE_curvemapping_set_defaults(&cmd->curve_mapping, 4, 0.0f, 0.0f, 1.0f, 1.0f);
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

static void curves_apply(SequenceModifierData *smd, ImBuf *ibuf, ImBuf *mask)
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

  BKE_curvemapping_set_defaults(&hcmd->curve_mapping, 1, 0.0f, 0.0f, 1.0f, 1.0f);
  hcmd->curve_mapping.preset = CURVE_PRESET_MID9;

  for (c = 0; c < 3; c++) {
    CurveMap *cuma = &hcmd->curve_mapping.cm[c];

    BKE_curvemap_reset(
        cuma, &hcmd->curve_mapping.clipr, hcmd->curve_mapping.preset, CURVEMAP_SLOPE_POSITIVE);
  }

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

static void hue_correct_apply(SequenceModifierData *smd, ImBuf *ibuf, ImBuf *mask)
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
  float brightness = data->bright / 100.0f;
  float contrast = data->contrast;
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
            uchar *m = mask_rect + pixel_index;
            float t = float(m[c]) / 255.0f;

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

static void brightcontrast_apply(SequenceModifierData *smd, ImBuf *ibuf, ImBuf *mask)
{
  BrightContrastModifierData *bcmd = (BrightContrastModifierData *)smd;
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
      int pixel_index = (y * width + x) * 4;

      if (rect) {
        uchar *pixel = rect + pixel_index;
        uchar *mask_pixel = mask_rect + pixel_index;
        uchar mask = min_iii(mask_pixel[0], mask_pixel[1], mask_pixel[2]);

        /* byte buffer is straight, so only affect on alpha itself,
         * this is the only way to alpha-over byte strip after
         * applying mask modifier.
         */
        pixel[3] = float(pixel[3] * mask) / 255.0f;
      }
      else if (rect_float) {
        int c;
        float *pixel = rect_float + pixel_index;
        const float *mask_pixel = mask_rect_float + pixel_index;
        float mask = min_fff(mask_pixel[0], mask_pixel[1], mask_pixel[2]);

        /* float buffers are premultiplied, so need to premul color
         * as well to make it easy to alpha-over masted strip.
         */
        for (c = 0; c < 4; c++) {
          pixel[c] = pixel[c] * mask;
        }
      }
    }
  }
}

static void maskmodifier_apply(SequenceModifierData * /*smd*/, ImBuf *ibuf, ImBuf *mask)
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
  SequencerTonemapModifierData *tmmd;
  ColorSpace *colorspace;
  float al;
  float auto_key;
  float lav;
  float cav[4];
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

static void tonemapmodifier_apply_threaded_simple(int width,
                                                  int height,
                                                  uchar *rect,
                                                  float *rect_float,
                                                  uchar *mask_rect,
                                                  const float *mask_rect_float,
                                                  void *data_v)
{
  AvgLogLum *avg = (AvgLogLum *)data_v;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int pixel_index = (y * width + x) * 4;
      float input[4], output[4], mask[3] = {1.0f, 1.0f, 1.0f};
      /* Get input value. */
      if (rect_float) {
        copy_v4_v4(input, &rect_float[pixel_index]);
      }
      else {
        straight_uchar_to_premul_float(input, &rect[pixel_index]);
      }
      IMB_colormanagement_colorspace_to_scene_linear_v3(input, avg->colorspace);
      copy_v4_v4(output, input);
      /* Get mask value. */
      if (mask_rect_float) {
        copy_v3_v3(mask, mask_rect_float + pixel_index);
      }
      else if (mask_rect) {
        rgb_uchar_to_float(mask, mask_rect + pixel_index);
      }
      /* Apply correction. */
      mul_v3_fl(output, avg->al);
      float dr = output[0] + avg->tmmd->offset;
      float dg = output[1] + avg->tmmd->offset;
      float db = output[2] + avg->tmmd->offset;
      output[0] /= ((dr == 0.0f) ? 1.0f : dr);
      output[1] /= ((dg == 0.0f) ? 1.0f : dg);
      output[2] /= ((db == 0.0f) ? 1.0f : db);
      const float igm = avg->igm;
      if (igm != 0.0f) {
        output[0] = powf(max_ff(output[0], 0.0f), igm);
        output[1] = powf(max_ff(output[1], 0.0f), igm);
        output[2] = powf(max_ff(output[2], 0.0f), igm);
      }
      /* Apply mask. */
      output[0] = input[0] * (1.0f - mask[0]) + output[0] * mask[0];
      output[1] = input[1] * (1.0f - mask[1]) + output[1] * mask[1];
      output[2] = input[2] * (1.0f - mask[2]) + output[2] * mask[2];
      /* Copy result back. */
      IMB_colormanagement_scene_linear_to_colorspace_v3(output, avg->colorspace);
      if (rect_float) {
        copy_v4_v4(&rect_float[pixel_index], output);
      }
      else {
        premul_float_to_straight_uchar(&rect[pixel_index], output);
      }
    }
  }
}

static void tonemapmodifier_apply_threaded_photoreceptor(int width,
                                                         int height,
                                                         uchar *rect,
                                                         float *rect_float,
                                                         uchar *mask_rect,
                                                         const float *mask_rect_float,
                                                         void *data_v)
{
  AvgLogLum *avg = (AvgLogLum *)data_v;
  const float f = expf(-avg->tmmd->intensity);
  const float m = (avg->tmmd->contrast > 0.0f) ? avg->tmmd->contrast :
                                                 (0.3f + 0.7f * powf(avg->auto_key, 1.4f));
  const float ic = 1.0f - avg->tmmd->correction, ia = 1.0f - avg->tmmd->adaptation;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int pixel_index = (y * width + x) * 4;
      float input[4], output[4], mask[3] = {1.0f, 1.0f, 1.0f};
      /* Get input value. */
      if (rect_float) {
        copy_v4_v4(input, &rect_float[pixel_index]);
      }
      else {
        straight_uchar_to_premul_float(input, &rect[pixel_index]);
      }
      IMB_colormanagement_colorspace_to_scene_linear_v3(input, avg->colorspace);
      copy_v4_v4(output, input);
      /* Get mask value. */
      if (mask_rect_float) {
        copy_v3_v3(mask, mask_rect_float + pixel_index);
      }
      else if (mask_rect) {
        rgb_uchar_to_float(mask, mask_rect + pixel_index);
      }
      /* Apply correction. */
      const float L = IMB_colormanagement_get_luminance(output);
      float I_l = output[0] + ic * (L - output[0]);
      float I_g = avg->cav[0] + ic * (avg->lav - avg->cav[0]);
      float I_a = I_l + ia * (I_g - I_l);
      output[0] /= (output[0] + powf(f * I_a, m));
      I_l = output[1] + ic * (L - output[1]);
      I_g = avg->cav[1] + ic * (avg->lav - avg->cav[1]);
      I_a = I_l + ia * (I_g - I_l);
      output[1] /= (output[1] + powf(f * I_a, m));
      I_l = output[2] + ic * (L - output[2]);
      I_g = avg->cav[2] + ic * (avg->lav - avg->cav[2]);
      I_a = I_l + ia * (I_g - I_l);
      output[2] /= (output[2] + powf(f * I_a, m));
      /* Apply mask. */
      output[0] = input[0] * (1.0f - mask[0]) + output[0] * mask[0];
      output[1] = input[1] * (1.0f - mask[1]) + output[1] * mask[1];
      output[2] = input[2] * (1.0f - mask[2]) + output[2] * mask[2];
      /* Copy result back. */
      IMB_colormanagement_scene_linear_to_colorspace_v3(output, avg->colorspace);
      if (rect_float) {
        copy_v4_v4(&rect_float[pixel_index], output);
      }
      else {
        premul_float_to_straight_uchar(&rect[pixel_index], output);
      }
    }
  }
}

static void tonemapmodifier_apply(SequenceModifierData *smd, ImBuf *ibuf, ImBuf *mask)
{
  SequencerTonemapModifierData *tmmd = (SequencerTonemapModifierData *)smd;
  AvgLogLum data;
  data.tmmd = tmmd;
  data.colorspace = (ibuf->float_buffer.data != nullptr) ? ibuf->float_buffer.colorspace :
                                                           ibuf->byte_buffer.colorspace;
  float lsum = 0.0f;
  int p = ibuf->x * ibuf->y;
  float *fp = ibuf->float_buffer.data;
  uchar *cp = ibuf->byte_buffer.data;
  float avl, maxl = -FLT_MAX, minl = FLT_MAX;
  const float sc = 1.0f / p;
  float Lav = 0.0f;
  float cav[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  while (p--) {
    float pixel[4];
    if (fp != nullptr) {
      copy_v4_v4(pixel, fp);
    }
    else {
      straight_uchar_to_premul_float(pixel, cp);
    }
    IMB_colormanagement_colorspace_to_scene_linear_v3(pixel, data.colorspace);
    float L = IMB_colormanagement_get_luminance(pixel);
    Lav += L;
    add_v3_v3(cav, pixel);
    lsum += logf(max_ff(L, 0.0f) + 1e-5f);
    maxl = (L > maxl) ? L : maxl;
    minl = (L < minl) ? L : minl;
    if (fp != nullptr) {
      fp += 4;
    }
    else {
      cp += 4;
    }
  }
  data.lav = Lav * sc;
  mul_v3_v3fl(data.cav, cav, sc);
  maxl = logf(maxl + 1e-5f);
  minl = logf(minl + 1e-5f);
  avl = lsum * sc;
  data.auto_key = (maxl > minl) ? ((maxl - avl) / (maxl - minl)) : 1.0f;
  float al = expf(avl);
  data.al = (al == 0.0f) ? 0.0f : (tmmd->key / al);
  data.igm = (tmmd->gamma == 0.0f) ? 1.0f : (1.0f / tmmd->gamma);

  if (tmmd->type == SEQ_TONEMAP_RD_PHOTORECEPTOR) {
    modifier_apply_threaded(ibuf, mask, tonemapmodifier_apply_threaded_photoreceptor, &data);
  }
  else /* if (tmmd->type == SEQ_TONEMAP_RD_SIMPLE) */ {
    modifier_apply_threaded(ibuf, mask, tonemapmodifier_apply_threaded_simple, &data);
  }
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

ImBuf *SEQ_modifier_apply_stack(const SeqRenderData *context,
                                Sequence *seq,
                                ImBuf *ibuf,
                                int timeline_frame)
{
  ImBuf *processed_ibuf = ibuf;

  if (seq->modifiers.first && (seq->flag & SEQ_USE_LINEAR_MODIFIERS)) {
    processed_ibuf = IMB_dupImBuf(ibuf);
    SEQ_render_imbuf_from_sequencer_space(context->scene, processed_ibuf);
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

    if (smti->apply) {
      int frame_offset;
      if (smd->mask_time == SEQUENCE_MASK_TIME_RELATIVE) {
        frame_offset = seq->start;
      }
      else /* if (smd->mask_time == SEQUENCE_MASK_TIME_ABSOLUTE) */ {
        frame_offset = smd->mask_id ? ((Mask *)smd->mask_id)->sfra : 0;
      }

      ImBuf *mask = modifier_mask_get(
          smd, context, timeline_frame, frame_offset, ibuf->float_buffer.data != nullptr);

      if (processed_ibuf == ibuf) {
        processed_ibuf = IMB_dupImBuf(ibuf);
      }

      smti->apply(smd, processed_ibuf, mask);

      if (mask) {
        IMB_freeImBuf(mask);
      }
    }
  }

  if (seq->modifiers.first && (seq->flag & SEQ_USE_LINEAR_MODIFIERS)) {
    seq_imbuf_to_sequencer_space(context->scene, processed_ibuf, false);
  }

  return processed_ibuf;
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

    smdn->next = smdn->prev = nullptr;
    BLI_addtail(&seqn->modifiers, smdn);
  }
}

int SEQ_sequence_supports_modifiers(Sequence *seq)
{
  return !ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD);
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
    }
    else {
      BLO_write_struct(writer, SequenceModifierData, smd);
    }
  }
}

void SEQ_modifier_blend_read_data(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_list(reader, lb);

  LISTBASE_FOREACH (SequenceModifierData *, smd, lb) {
    if (smd->mask_sequence) {
      BLO_read_data_address(reader, &smd->mask_sequence);
    }

    if (smd->type == seqModifierType_Curves) {
      CurvesModifierData *cmd = (CurvesModifierData *)smd;

      BKE_curvemapping_blend_read(reader, &cmd->curve_mapping);
    }
    else if (smd->type == seqModifierType_HueCorrect) {
      HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;

      BKE_curvemapping_blend_read(reader, &hcmd->curve_mapping);
    }
  }
}

void SEQ_modifier_blend_read_lib(BlendLibReader *reader, Scene *scene, ListBase *lb)
{
  LISTBASE_FOREACH (SequenceModifierData *, smd, lb) {
    if (smd->mask_id) {
      BLO_read_id_address(reader, &scene->id, &smd->mask_id);
    }
  }
}

/** \} */
