/* SPDX-FileCopyrightText: 2012-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.hh"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_task.hh"

#include "BLT_translation.hh"

#include "DNA_mask_types.h"
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

#include "modifier.hh"
#include "render.hh"

namespace blender::seq {

/* -------------------------------------------------------------------- */

static bool modifier_has_persistent_uid(const Strip &strip, int uid)
{
  LISTBASE_FOREACH (StripModifierData *, smd, &strip.modifiers) {
    if (smd->persistent_uid == uid) {
      return true;
    }
  }
  return false;
}

void modifier_persistent_uid_init(const Strip &strip, StripModifierData &smd)
{
  uint64_t hash = blender::get_default_hash(blender::StringRef(smd.name));
  blender::RandomNumberGenerator rng{uint32_t(hash)};
  while (true) {
    const int new_uid = rng.get_int32();
    if (new_uid <= 0) {
      continue;
    }
    if (modifier_has_persistent_uid(strip, new_uid)) {
      continue;
    }
    smd.persistent_uid = new_uid;
    break;
  }
}

bool modifier_persistent_uids_are_valid(const Strip &strip)
{
  Set<int> uids;
  int modifiers_num = 0;
  LISTBASE_FOREACH (StripModifierData *, smd, &strip.modifiers) {
    if (smd->persistent_uid <= 0) {
      return false;
    }
    uids.add(smd->persistent_uid);
    modifiers_num++;
  }
  if (uids.size() != modifiers_num) {
    return false;
  }
  return true;
}

static float4 load_pixel_premul(const uchar *ptr)
{
  float4 res;
  straight_uchar_to_premul_float(res, ptr);
  return res;
}

static float4 load_pixel_premul(const float *ptr)
{
  return float4(ptr);
}

static void store_pixel_premul(float4 pix, uchar *ptr)
{
  premul_float_to_straight_uchar(ptr, pix);
}

static void store_pixel_premul(float4 pix, float *ptr)
{
  *reinterpret_cast<float4 *>(ptr) = pix;
}

static float4 load_pixel_raw(const uchar *ptr)
{
  float4 res;
  rgba_uchar_to_float(res, ptr);
  return res;
}

static float4 load_pixel_raw(const float *ptr)
{
  return float4(ptr);
}

static void store_pixel_raw(float4 pix, uchar *ptr)
{
  rgba_float_to_uchar(ptr, pix);
}

static void store_pixel_raw(float4 pix, float *ptr)
{
  *reinterpret_cast<float4 *>(ptr) = pix;
}

/* Byte mask */
static void apply_and_advance_mask(float4 input, float4 &result, const uchar *&mask)
{
  float3 m;
  rgb_uchar_to_float(m, mask);
  result.x = math::interpolate(input.x, result.x, m.x);
  result.y = math::interpolate(input.y, result.y, m.y);
  result.z = math::interpolate(input.z, result.z, m.z);
  mask += 4;
}

/* Float mask */
static void apply_and_advance_mask(float4 input, float4 &result, const float *&mask)
{
  float3 m(mask);
  result.x = math::interpolate(input.x, result.x, m.x);
  result.y = math::interpolate(input.y, result.y, m.y);
  result.z = math::interpolate(input.z, result.z, m.z);
  mask += 4;
}

/* No mask */
static void apply_and_advance_mask(float4 /*input*/, float4 & /*result*/, const void *& /*mask*/)
{
}

/* Given `T` that implements an `apply` function:
 *
 *    template <typename ImageT, typename MaskT>
 *    void apply(ImageT* image, const MaskT* mask, IndexRange size);
 *
 * this function calls the apply() function in parallel
 * chunks of the image to process, and with needed
 * uchar, float or void types (void is used for mask, when there is
 * no masking). Both input and mask images are expected to have
 * 4 (RGBA) color channels. Input is modified. */
template<typename T> static void apply_modifier_op(T &op, ImBuf *ibuf, const ImBuf *mask)
{
  if (ibuf == nullptr) {
    return;
  }
  BLI_assert_msg(ibuf->channels == 0 || ibuf->channels == 4,
                 "Sequencer only supports 4 channel images");
  BLI_assert_msg(mask == nullptr || mask->channels == 0 || mask->channels == 4,
                 "Sequencer only supports 4 channel images");

  threading::parallel_for(IndexRange(size_t(ibuf->x) * ibuf->y), 32 * 1024, [&](IndexRange range) {
    uchar *image_byte = ibuf->byte_buffer.data;
    float *image_float = ibuf->float_buffer.data;
    const uchar *mask_byte = mask ? mask->byte_buffer.data : nullptr;
    const float *mask_float = mask ? mask->float_buffer.data : nullptr;
    const void *mask_none = nullptr;
    int64_t offset = range.first() * 4;

    /* Instantiate the needed processing function based on image/mask
     * data types. */
    if (image_byte) {
      if (mask_byte) {
        op.apply(image_byte + offset, mask_byte + offset, range);
      }
      else if (mask_float) {
        op.apply(image_byte + offset, mask_float + offset, range);
      }
      else {
        op.apply(image_byte + offset, mask_none, range);
      }
    }
    else if (image_float) {
      if (mask_byte) {
        op.apply(image_float + offset, mask_byte + offset, range);
      }
      else if (mask_float) {
        op.apply(image_float + offset, mask_float + offset, range);
      }
      else {
        op.apply(image_float + offset, mask_none, range);
      }
    }
  });
}

/**
 * \a timeline_frame is offset by \a fra_offset only in case we are using a real mask.
 */
static ImBuf *modifier_render_mask_input(const RenderData *context,
                                         int mask_input_type,
                                         Strip *mask_strip,
                                         Mask *mask_id,
                                         int timeline_frame,
                                         int fra_offset)
{
  ImBuf *mask_input = nullptr;

  if (mask_input_type == SEQUENCE_MASK_INPUT_STRIP) {
    if (mask_strip) {
      SeqRenderState state;
      mask_input = seq_render_strip(context, &state, mask_strip, timeline_frame);
    }
  }
  else if (mask_input_type == SEQUENCE_MASK_INPUT_ID) {
    /* Note that we do not request mask to be float image: if it is that is
     * fine, but if it is a byte image then we also just take that without
     * extra memory allocations or conversions. All modifiers are expected
     * to handle mask being either type. */
    mask_input = seq_render_mask(context, mask_id, timeline_frame - fra_offset, false);
  }

  return mask_input;
}

static ImBuf *modifier_mask_get(StripModifierData *smd,
                                const RenderData *context,
                                int timeline_frame,
                                int fra_offset)
{
  return modifier_render_mask_input(
      context, smd->mask_input_type, smd->mask_strip, smd->mask_id, timeline_frame, fra_offset);
}

/* -------------------------------------------------------------------- */
/** \name Color Balance Modifier
 * \{ */

/* Lift-Gamma-Gain math. NOTE: lift is actually (2-lift). */
static float color_balance_lgg(
    float in, const float lift, const float gain, const float gamma, const float mul)
{
  float x = (((in - 1.0f) * lift) + 1.0f) * gain;

  /* prevent NaN */
  x = std::max(x, 0.0f);

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
  x = std::max(x, 0.0f);

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

struct ColorBalanceApplyOp {
  int method;
  float3 lift, gain, gamma;
  float3 slope, offset, power;
  float multiplier;
  float lut[3][CB_TABLE_SIZE];

  /* Apply on a byte image via a table lookup. */
  template<typename MaskT> void apply(uchar *image, const MaskT *mask, IndexRange size)
  {
    for ([[maybe_unused]] int64_t i : size) {
      float4 input = load_pixel_premul(image);

      float4 result;
      int p0 = int(input.x * (CB_TABLE_SIZE - 1.0f) + 0.5f);
      int p1 = int(input.y * (CB_TABLE_SIZE - 1.0f) + 0.5f);
      int p2 = int(input.z * (CB_TABLE_SIZE - 1.0f) + 0.5f);
      result.x = this->lut[0][p0];
      result.y = this->lut[1][p1];
      result.z = this->lut[2][p2];
      result.w = input.w;

      apply_and_advance_mask(input, result, mask);
      store_pixel_premul(result, image);
      image += 4;
    }
  }

  /* Apply on a float image by doing full math. */
  template<typename MaskT> void apply(float *image, const MaskT *mask, IndexRange size)
  {
    if (this->method == SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN) {
      /* Lift/Gamma/Gain */
      for ([[maybe_unused]] int64_t i : size) {
        float4 input = load_pixel_premul(image);

        float4 result;
        result.x = color_balance_lgg(
            input.x, this->lift.x, this->gain.x, this->gamma.x, this->multiplier);
        result.y = color_balance_lgg(
            input.y, this->lift.y, this->gain.y, this->gamma.y, this->multiplier);
        result.z = color_balance_lgg(
            input.z, this->lift.z, this->gain.z, this->gamma.z, this->multiplier);
        result.w = input.w;

        apply_and_advance_mask(input, result, mask);
        store_pixel_premul(result, image);
        image += 4;
      }
    }
    else if (this->method == SEQ_COLOR_BALANCE_METHOD_SLOPEOFFSETPOWER) {
      /* Slope/Offset/Power */
      for ([[maybe_unused]] int64_t i : size) {
        float4 input = load_pixel_premul(image);

        float4 result;
        result.x = color_balance_sop(
            input.x, this->slope.x, this->offset.x, this->power.x, this->multiplier);
        result.y = color_balance_sop(
            input.y, this->slope.y, this->offset.y, this->power.y, this->multiplier);
        result.z = color_balance_sop(
            input.z, this->slope.z, this->offset.z, this->power.z, this->multiplier);
        result.w = input.w;

        apply_and_advance_mask(input, result, mask);
        store_pixel_premul(result, image);
        image += 4;
      }
    }
    else {
      BLI_assert_unreachable();
    }
  }

  void init_lgg(const StripColorBalance &data)
  {
    BLI_assert(data.method == SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN);

    this->lift = 2.0f - float3(data.lift);
    if (data.flag & SEQ_COLOR_BALANCE_INVERSE_LIFT) {
      for (int c = 0; c < 3; c++) {
        /* tweak to give more subtle results
         * values above 1.0 are scaled */
        if (this->lift[c] > 1.0f) {
          this->lift[c] = powf(this->lift[c] - 1.0f, 2.0f) + 1.0f;
        }
        this->lift[c] = 2.0f - this->lift[c];
      }
    }

    this->gain = float3(data.gain);
    if (data.flag & SEQ_COLOR_BALANCE_INVERSE_GAIN) {
      this->gain = math::rcp(math::max(this->gain, float3(1.0e-6f)));
    }

    this->gamma = float3(data.gamma);
    if (!(data.flag & SEQ_COLOR_BALANCE_INVERSE_GAMMA)) {
      this->gamma = math::rcp(math::max(this->gamma, float3(1.0e-6f)));
    }
  }

  void init_sop(const StripColorBalance &data)
  {
    BLI_assert(data.method == SEQ_COLOR_BALANCE_METHOD_SLOPEOFFSETPOWER);

    this->slope = float3(data.slope);
    if (data.flag & SEQ_COLOR_BALANCE_INVERSE_SLOPE) {
      this->slope = math::rcp(math::max(this->slope, float3(1.0e-6f)));
    }

    this->offset = float3(data.offset) - 1.0f;
    if (data.flag & SEQ_COLOR_BALANCE_INVERSE_OFFSET) {
      this->offset = -this->offset;
    }

    this->power = float3(data.power);
    if (!(data.flag & SEQ_COLOR_BALANCE_INVERSE_POWER)) {
      this->power = math::rcp(math::max(this->power, float3(1.0e-6f)));
    }
  }

  void init(const ColorBalanceModifierData &data, bool byte_image)
  {
    this->multiplier = data.color_multiply;
    this->method = data.color_balance.method;

    if (this->method == SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN) {
      init_lgg(data.color_balance);
      if (byte_image) {
        for (int c = 0; c < 3; c++) {
          make_cb_table_lgg(
              this->lift[c], this->gain[c], this->gamma[c], this->multiplier, this->lut[c]);
        }
      }
    }
    else if (this->method == SEQ_COLOR_BALANCE_METHOD_SLOPEOFFSETPOWER) {
      init_sop(data.color_balance);
      if (byte_image) {
        for (int c = 0; c < 3; c++) {
          make_cb_table_sop(
              this->slope[c], this->offset[c], this->power[c], this->multiplier, this->lut[c]);
        }
      }
    }
    else {
      BLI_assert_unreachable();
    }
  }
};

static void colorBalance_init_data(StripModifierData *smd)
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
                               StripModifierData *smd,
                               ImBuf *ibuf,
                               ImBuf *mask)
{
  const ColorBalanceModifierData *cbmd = (const ColorBalanceModifierData *)smd;

  ColorBalanceApplyOp op;
  op.init(*cbmd, ibuf->byte_buffer.data != nullptr);
  apply_modifier_op(op, ibuf, mask);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name White Balance Modifier
 * \{ */

static void whiteBalance_init_data(StripModifierData *smd)
{
  WhiteBalanceModifierData *cbmd = (WhiteBalanceModifierData *)smd;
  copy_v3_fl(cbmd->white_value, 1.0f);
}

struct WhiteBalanceApplyOp {
  float multiplier[3];

  template<typename ImageT, typename MaskT>
  void apply(ImageT *image, const MaskT *mask, IndexRange size)
  {
    for ([[maybe_unused]] int64_t i : size) {
      float4 input = load_pixel_premul(image);

      float4 result;
      result.w = input.w;
#if 0
      mul_v3_v3(result, multiplier);
#else
      /* similar to division without the clipping */
      for (int i = 0; i < 3; i++) {
        /* Prevent pow argument from being negative. This whole math
         * breaks down overall with any HDR colors; would be good to
         * revisit and do something more proper. */
        float f = max_ff(1.0f - input[i], 0.0f);
        result[i] = 1.0f - powf(f, this->multiplier[i]);
      }
#endif

      apply_and_advance_mask(input, result, mask);
      store_pixel_premul(result, image);
      image += 4;
    }
  }
};

static void whiteBalance_apply(const StripScreenQuad & /*quad*/,
                               StripModifierData *smd,
                               ImBuf *ibuf,
                               ImBuf *mask)
{
  const WhiteBalanceModifierData *data = (const WhiteBalanceModifierData *)smd;

  WhiteBalanceApplyOp op;
  op.multiplier[0] = (data->white_value[0] != 0.0f) ? 1.0f / data->white_value[0] : FLT_MAX;
  op.multiplier[1] = (data->white_value[1] != 0.0f) ? 1.0f / data->white_value[1] : FLT_MAX;
  op.multiplier[2] = (data->white_value[2] != 0.0f) ? 1.0f / data->white_value[2] : FLT_MAX;
  apply_modifier_op(op, ibuf, mask);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curves Modifier
 * \{ */

static void curves_init_data(StripModifierData *smd)
{
  CurvesModifierData *cmd = (CurvesModifierData *)smd;

  BKE_curvemapping_set_defaults(&cmd->curve_mapping, 4, 0.0f, 0.0f, 1.0f, 1.0f, HD_AUTO);
}

static void curves_free_data(StripModifierData *smd)
{
  CurvesModifierData *cmd = (CurvesModifierData *)smd;

  BKE_curvemapping_free_data(&cmd->curve_mapping);
}

static void curves_copy_data(StripModifierData *target, StripModifierData *smd)
{
  CurvesModifierData *cmd = (CurvesModifierData *)smd;
  CurvesModifierData *cmd_target = (CurvesModifierData *)target;

  BKE_curvemapping_copy_data(&cmd_target->curve_mapping, &cmd->curve_mapping);
}

struct CurvesApplyOp {
  const CurveMapping *curve_mapping;

  template<typename ImageT, typename MaskT>
  void apply(ImageT *image, const MaskT *mask, IndexRange size)
  {
    for ([[maybe_unused]] int64_t i : size) {
      float4 input = load_pixel_premul(image);

      float4 result;
      BKE_curvemapping_evaluate_premulRGBF(this->curve_mapping, result, input);
      result.w = input.w;

      apply_and_advance_mask(input, result, mask);
      store_pixel_premul(result, image);
      image += 4;
    }
  }
};

static void curves_apply(const StripScreenQuad & /*quad*/,
                         StripModifierData *smd,
                         ImBuf *ibuf,
                         ImBuf *mask)
{
  CurvesModifierData *cmd = (CurvesModifierData *)smd;

  const float black[3] = {0.0f, 0.0f, 0.0f};
  const float white[3] = {1.0f, 1.0f, 1.0f};

  BKE_curvemapping_init(&cmd->curve_mapping);

  BKE_curvemapping_premultiply(&cmd->curve_mapping, false);
  BKE_curvemapping_set_black_white(&cmd->curve_mapping, black, white);

  CurvesApplyOp op;
  op.curve_mapping = &cmd->curve_mapping;
  apply_modifier_op(op, ibuf, mask);

  BKE_curvemapping_premultiply(&cmd->curve_mapping, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hue Correct Modifier
 * \{ */

static void hue_correct_init_data(StripModifierData *smd)
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

static void hue_correct_free_data(StripModifierData *smd)
{
  HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;

  BKE_curvemapping_free_data(&hcmd->curve_mapping);
}

static void hue_correct_copy_data(StripModifierData *target, StripModifierData *smd)
{
  HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;
  HueCorrectModifierData *hcmd_target = (HueCorrectModifierData *)target;

  BKE_curvemapping_copy_data(&hcmd_target->curve_mapping, &hcmd->curve_mapping);
}

struct HueCorrectApplyOp {
  const CurveMapping *curve_mapping;

  template<typename ImageT, typename MaskT>
  void apply(ImageT *image, const MaskT *mask, IndexRange size)
  {
    for ([[maybe_unused]] int64_t i : size) {
      /* NOTE: arguably incorrect usage of "raw" values, should be un-premultiplied.
       * Not changing behavior for now, but would be good to fix someday. */
      float4 input = load_pixel_raw(image);
      float4 result;
      result.w = input.w;

      float3 hsv;
      rgb_to_hsv(input.x, input.y, input.z, &hsv.x, &hsv.y, &hsv.z);

      /* adjust hue, scaling returned default 0.5 up to 1 */
      float f;
      f = BKE_curvemapping_evaluateF(this->curve_mapping, 0, hsv.x);
      hsv.x += f - 0.5f;

      /* adjust saturation, scaling returned default 0.5 up to 1 */
      f = BKE_curvemapping_evaluateF(this->curve_mapping, 1, hsv.x);
      hsv.y *= (f * 2.0f);

      /* adjust value, scaling returned default 0.5 up to 1 */
      f = BKE_curvemapping_evaluateF(this->curve_mapping, 2, hsv.x);
      hsv.z *= (f * 2.0f);

      hsv.x = hsv.x - floorf(hsv.x); /* mod 1.0 */
      hsv.y = math::clamp(hsv.y, 0.0f, 1.0f);

      /* convert back to rgb */
      hsv_to_rgb(hsv.x, hsv.y, hsv.z, &result.x, &result.y, &result.z);

      apply_and_advance_mask(input, result, mask);
      store_pixel_raw(result, image);
      image += 4;
    }
  }
};

static void hue_correct_apply(const StripScreenQuad & /*quad*/,
                              StripModifierData *smd,
                              ImBuf *ibuf,
                              ImBuf *mask)
{
  HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;

  BKE_curvemapping_init(&hcmd->curve_mapping);

  HueCorrectApplyOp op;
  op.curve_mapping = &hcmd->curve_mapping;
  apply_modifier_op(op, ibuf, mask);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Brightness/Contrast Modifier
 * \{ */

struct BrightContrastApplyOp {
  float mul;
  float add;

  template<typename ImageT, typename MaskT>
  void apply(ImageT *image, const MaskT *mask, IndexRange size)
  {
    for ([[maybe_unused]] int64_t i : size) {
      /* NOTE: arguably incorrect usage of "raw" values, should be un-premultiplied.
       * Not changing behavior for now, but would be good to fix someday. */
      float4 input = load_pixel_raw(image);

      float4 result;
      result = input * this->mul + this->add;
      result.w = input.w;

      apply_and_advance_mask(input, result, mask);
      store_pixel_raw(result, image);
      image += 4;
    }
  }
};

static void brightcontrast_apply(const StripScreenQuad & /*quad*/,
                                 StripModifierData *smd,
                                 ImBuf *ibuf,
                                 ImBuf *mask)
{
  const BrightContrastModifierData *bcmd = (BrightContrastModifierData *)smd;

  BrightContrastApplyOp op;

  /* The algorithm is by Werner D. Streidt
   * (http://visca.com/ffactory/archives/5-99/msg00021.html)
   * Extracted from OpenCV `demhist.cpp`. */
  const float brightness = bcmd->bright / 100.0f;
  const float contrast = bcmd->contrast;
  float delta = contrast / 200.0f;

  if (contrast > 0) {
    op.mul = 1.0f - delta * 2.0f;
    op.mul = 1.0f / max_ff(op.mul, FLT_EPSILON);
    op.add = op.mul * (brightness - delta);
  }
  else {
    delta *= -1;
    op.mul = max_ff(1.0f - delta * 2.0f, 0.0f);
    op.add = op.mul * brightness + delta;
  }

  apply_modifier_op(op, ibuf, mask);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mask Modifier
 * \{ */

static float load_mask_min(const uchar *&mask)
{
  float m = float(min_iii(mask[0], mask[1], mask[2])) * (1.0f / 255.0f);
  mask += 4;
  return m;
}
static float load_mask_min(const float *&mask)
{
  float m = min_fff(mask[0], mask[1], mask[2]);
  mask += 4;
  return m;
}
static float load_mask_min(const void *& /*mask*/)
{
  return 1.0f;
}

struct MaskApplyOp {
  template<typename ImageT, typename MaskT>
  void apply(ImageT *image, const MaskT *mask, IndexRange size)
  {
    for ([[maybe_unused]] int64_t i : size) {
      float m = load_mask_min(mask);

      if constexpr (std::is_same_v<ImageT, uchar>) {
        /* Byte buffer is straight, so only affect on alpha itself, this is
         * the only way to alpha-over byte strip after applying mask modifier. */
        image[3] = uchar(image[3] * m);
      }
      else if constexpr (std::is_same_v<ImageT, float>) {
        /* Float buffers are premultiplied, so need to premul color as well to make it
         * easy to alpha-over masked strip. */
        float4 pix(image);
        pix *= m;
        *reinterpret_cast<float4 *>(image) = pix;
      }
      image += 4;
    }
  }
};

static void maskmodifier_apply(const StripScreenQuad & /*quad*/,
                               StripModifierData * /*smd*/,
                               ImBuf *ibuf,
                               ImBuf *mask)
{
  if (mask == nullptr || (mask->byte_buffer.data == nullptr && mask->float_buffer.data == nullptr))
  {
    return;
  }

  MaskApplyOp op;
  apply_modifier_op(op, ibuf, mask);

  /* Image has gained transparency. */
  ibuf->planes = R_IMF_PLANES_RGBA;
}

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

static void tonemapmodifier_init_data(StripModifierData *smd)
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
static void pixels_to_scene_linear_float(const ColorSpace *colorspace,
                                         float4 *pixels,
                                         int64_t count)
{
  IMB_colormanagement_colorspace_to_scene_linear(
      (float *)(pixels), int(count), 1, 4, colorspace, false);
}

/* Convert chunk of byte image pixels to scene linear space, into a destination array. */
static void pixels_to_scene_linear_byte(const ColorSpace *colorspace,
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
  const ColorSpace *colorspace = ibuf->float_buffer.colorspace;
  float4 *fptr = reinterpret_cast<float4 *>(ibuf->float_buffer.data);
  IMB_colormanagement_scene_linear_to_colorspace(
      (float *)(fptr + range.first()), int(range.size()), 1, 4, colorspace);
}

static void scene_linear_to_image_chunk_byte(float4 *src, ImBuf *ibuf, IndexRange range)
{
  const ColorSpace *colorspace = ibuf->byte_buffer.colorspace;
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
      if (mask_byte != nullptr) {
        rgb_uchar_to_float(msk, mask_byte[pixel_index]);
      }
      else if (mask_float != nullptr) {
        msk = mask_float[pixel_index].xyz();
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
      if (mask_byte != nullptr) {
        rgb_uchar_to_float(msk, mask_byte[pixel_index]);
      }
      else if (mask_float != nullptr) {
        msk = mask_float[pixel_index].xyz();
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
                                  StripModifierData *smd,
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Modifier Functions
 * \{ */

static StripModifierTypeInfo modifiersTypes[NUM_SEQUENCE_MODIFIER_TYPES] = {
    {}, /* First entry is unused. */
    {
        /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Color Balance"),
        /*struct_name*/ "ColorBalanceModifierData",
        /*struct_size*/ sizeof(ColorBalanceModifierData),
        /*init_data*/ colorBalance_init_data,
        /*free_data*/ nullptr,
        /*copy_data*/ nullptr,
        /*apply*/ colorBalance_apply,
    },
    {
        /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Curves"),
        /*struct_name*/ "CurvesModifierData",
        /*struct_size*/ sizeof(CurvesModifierData),
        /*init_data*/ curves_init_data,
        /*free_data*/ curves_free_data,
        /*copy_data*/ curves_copy_data,
        /*apply*/ curves_apply,
    },
    {
        /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Hue Correct"),
        /*struct_name*/ "HueCorrectModifierData",
        /*struct_size*/ sizeof(HueCorrectModifierData),
        /*init_data*/ hue_correct_init_data,
        /*free_data*/ hue_correct_free_data,
        /*copy_data*/ hue_correct_copy_data,
        /*apply*/ hue_correct_apply,
    },
    {
        /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Brightness/Contrast"),
        /*struct_name*/ "BrightContrastModifierData",
        /*struct_size*/ sizeof(BrightContrastModifierData),
        /*init_data*/ nullptr,
        /*free_data*/ nullptr,
        /*copy_data*/ nullptr,
        /*apply*/ brightcontrast_apply,
    },
    {
        /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Mask"),
        /*struct_name*/ "SequencerMaskModifierData",
        /*struct_size*/ sizeof(SequencerMaskModifierData),
        /*init_data*/ nullptr,
        /*free_data*/ nullptr,
        /*copy_data*/ nullptr,
        /*apply*/ maskmodifier_apply,
    },
    {
        /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "White Balance"),
        /*struct_name*/ "WhiteBalanceModifierData",
        /*struct_size*/ sizeof(WhiteBalanceModifierData),
        /*init_data*/ whiteBalance_init_data,
        /*free_data*/ nullptr,
        /*copy_data*/ nullptr,
        /*apply*/ whiteBalance_apply,
    },
    {
        /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Tonemap"),
        /*struct_name*/ "SequencerTonemapModifierData",
        /*struct_size*/ sizeof(SequencerTonemapModifierData),
        /*init_data*/ tonemapmodifier_init_data,
        /*free_data*/ nullptr,
        /*copy_data*/ nullptr,
        /*apply*/ tonemapmodifier_apply,
    },
    {
        /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Equalizer"),
        /*struct_name*/ "SoundEqualizerModifierData",
        /*struct_size*/ sizeof(SoundEqualizerModifierData),
        /*init_data*/ sound_equalizermodifier_init_data,
        /*free_data*/ sound_equalizermodifier_free,
        /*copy_data*/ sound_equalizermodifier_copy_data,
        /*apply*/ nullptr,
    },
};

const StripModifierTypeInfo *modifier_type_info_get(int type)
{
  if (type <= 0 || type >= NUM_SEQUENCE_MODIFIER_TYPES) {
    return nullptr;
  }
  return &modifiersTypes[type];
}

StripModifierData *modifier_new(Strip *strip, const char *name, int type)
{
  StripModifierData *smd;
  const StripModifierTypeInfo *smti = modifier_type_info_get(type);

  smd = static_cast<StripModifierData *>(MEM_callocN(smti->struct_size, "sequence modifier"));

  smd->type = type;
  smd->flag |= SEQUENCE_MODIFIER_EXPANDED;

  if (!name || !name[0]) {
    STRNCPY_UTF8(smd->name, CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, smti->name));
  }
  else {
    STRNCPY_UTF8(smd->name, name);
  }

  BLI_addtail(&strip->modifiers, smd);

  modifier_unique_name(strip, smd);

  if (smti->init_data) {
    smti->init_data(smd);
  }

  return smd;
}

bool modifier_remove(Strip *strip, StripModifierData *smd)
{
  if (BLI_findindex(&strip->modifiers, smd) == -1) {
    return false;
  }

  BLI_remlink(&strip->modifiers, smd);
  modifier_free(smd);

  return true;
}

void modifier_clear(Strip *strip)
{
  StripModifierData *smd, *smd_next;

  for (smd = static_cast<StripModifierData *>(strip->modifiers.first); smd; smd = smd_next) {
    smd_next = smd->next;
    modifier_free(smd);
  }

  BLI_listbase_clear(&strip->modifiers);
}

void modifier_free(StripModifierData *smd)
{
  const StripModifierTypeInfo *smti = modifier_type_info_get(smd->type);

  if (smti && smti->free_data) {
    smti->free_data(smd);
  }

  MEM_freeN(smd);
}

void modifier_unique_name(Strip *strip, StripModifierData *smd)
{
  const StripModifierTypeInfo *smti = modifier_type_info_get(smd->type);

  BLI_uniquename(&strip->modifiers,
                 smd,
                 CTX_DATA_(BLT_I18NCONTEXT_ID_SEQUENCE, smti->name),
                 '.',
                 offsetof(StripModifierData, name),
                 sizeof(smd->name));
}

StripModifierData *modifier_find_by_name(Strip *strip, const char *name)
{
  return static_cast<StripModifierData *>(
      BLI_findstring(&(strip->modifiers), name, offsetof(StripModifierData, name)));
}

static bool skip_modifier(Scene *scene, const StripModifierData *smd, int timeline_frame)
{
  using namespace blender::seq;

  if (smd->mask_strip == nullptr) {
    return false;
  }
  const bool strip_has_ended_skip = smd->mask_input_type == SEQUENCE_MASK_INPUT_STRIP &&
                                    smd->mask_time == SEQUENCE_MASK_TIME_RELATIVE &&
                                    !time_strip_intersects_frame(
                                        scene, smd->mask_strip, timeline_frame);
  const bool missing_data_skip = !strip_has_valid_data(smd->mask_strip) ||
                                 media_presence_is_missing(scene, smd->mask_strip);

  return strip_has_ended_skip || missing_data_skip;
}

void modifier_apply_stack(const RenderData *context,
                          const Strip *strip,
                          ImBuf *ibuf,
                          int timeline_frame)
{
  const StripScreenQuad quad = get_strip_screen_quad(context, strip);

  if (strip->modifiers.first && (strip->flag & SEQ_USE_LINEAR_MODIFIERS)) {
    render_imbuf_from_sequencer_space(context->scene, ibuf);
  }

  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    const StripModifierTypeInfo *smti = modifier_type_info_get(smd->type);

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
        frame_offset = strip->start;
      }
      else /* if (smd->mask_time == SEQUENCE_MASK_TIME_ABSOLUTE) */ {
        frame_offset = smd->mask_id ? ((Mask *)smd->mask_id)->sfra : 0;
      }

      ImBuf *mask = modifier_mask_get(smd, context, timeline_frame, frame_offset);
      smti->apply(quad, smd, ibuf, mask);
      if (mask) {
        IMB_freeImBuf(mask);
      }
    }
  }

  if (strip->modifiers.first && (strip->flag & SEQ_USE_LINEAR_MODIFIERS)) {
    seq_imbuf_to_sequencer_space(context->scene, ibuf, false);
  }
}

StripModifierData *modifier_copy(Strip &strip_dst, StripModifierData *mod_src)
{
  const StripModifierTypeInfo *smti = modifier_type_info_get(mod_src->type);
  StripModifierData *mod_new = static_cast<StripModifierData *>(MEM_dupallocN(mod_src));

  if (smti && smti->copy_data) {
    smti->copy_data(mod_new, mod_src);
  }

  BLI_addtail(&strip_dst.modifiers, mod_new);
  BLI_uniquename(&strip_dst.modifiers,
                 mod_new,
                 "Strip Modifier",
                 '.',
                 offsetof(StripModifierData, name),
                 sizeof(StripModifierData::name));
  return mod_new;
}

void modifier_list_copy(Strip *strip_new, Strip *strip)
{
  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    modifier_copy(*strip_new, smd);
  }
}

int sequence_supports_modifiers(Strip *strip)
{
  return (strip->type != STRIP_TYPE_SOUND_RAM);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name .blend File I/O
 * \{ */

void modifier_blend_write(BlendWriter *writer, ListBase *modbase)
{
  LISTBASE_FOREACH (StripModifierData *, smd, modbase) {
    const StripModifierTypeInfo *smti = modifier_type_info_get(smd->type);

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
      BLO_write_struct(writer, StripModifierData, smd);
    }
  }
}

void modifier_blend_read_data(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_struct_list(reader, StripModifierData, lb);

  LISTBASE_FOREACH (StripModifierData *, smd, lb) {
    if (smd->mask_strip) {
      BLO_read_struct(reader, Strip, &smd->mask_strip);
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

}  // namespace blender::seq
