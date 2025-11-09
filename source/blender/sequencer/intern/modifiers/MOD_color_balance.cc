/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_math_base.h"

#include "BLT_translation.hh"

#include "DNA_sequence_types.h"

#include "SEQ_modifier.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "RNA_access.hh"

#include "modifier.hh"

namespace blender::seq {

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
  template<typename MaskSampler>
  void apply(uchar *image, MaskSampler &mask, int image_x, IndexRange y_range)
  {
    image += y_range.first() * image_x * 4;
    for (int64_t y : y_range) {
      mask.begin_row(y);
      for ([[maybe_unused]] int64_t x : IndexRange(image_x)) {
        float4 input = load_pixel_premul(image);

        float4 result;
        int p0 = int(input.x * (CB_TABLE_SIZE - 1.0f) + 0.5f);
        int p1 = int(input.y * (CB_TABLE_SIZE - 1.0f) + 0.5f);
        int p2 = int(input.z * (CB_TABLE_SIZE - 1.0f) + 0.5f);
        result.x = this->lut[0][p0];
        result.y = this->lut[1][p1];
        result.z = this->lut[2][p2];
        result.w = input.w;

        mask.apply_mask(input, result);
        store_pixel_premul(result, image);
        image += 4;
      }
    }
  }

  /* Apply on a float image by doing full math. */
  template<typename MaskSampler>
  void apply(float *image, MaskSampler &mask, int image_x, IndexRange y_range)
  {
    image += y_range.first() * image_x * 4;
    for (int64_t y : y_range) {
      mask.begin_row(y);
      if (this->method == SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN) {
        /* Lift/Gamma/Gain */
        for ([[maybe_unused]] int64_t x : IndexRange(image_x)) {
          float4 input = load_pixel_premul(image);

          float4 result;
          result.x = color_balance_lgg(
              input.x, this->lift.x, this->gain.x, this->gamma.x, this->multiplier);
          result.y = color_balance_lgg(
              input.y, this->lift.y, this->gain.y, this->gamma.y, this->multiplier);
          result.z = color_balance_lgg(
              input.z, this->lift.z, this->gain.z, this->gamma.z, this->multiplier);
          result.w = input.w;

          mask.apply_mask(input, result);
          store_pixel_premul(result, image);
          image += 4;
        }
      }
      else if (this->method == SEQ_COLOR_BALANCE_METHOD_SLOPEOFFSETPOWER) {
        /* Slope/Offset/Power */
        for ([[maybe_unused]] int64_t x : IndexRange(image_x)) {
          float4 input = load_pixel_premul(image);

          float4 result;
          result.x = color_balance_sop(
              input.x, this->slope.x, this->offset.x, this->power.x, this->multiplier);
          result.y = color_balance_sop(
              input.y, this->slope.y, this->offset.y, this->power.y, this->multiplier);
          result.z = color_balance_sop(
              input.z, this->slope.z, this->offset.z, this->power.z, this->multiplier);
          result.w = input.w;

          mask.apply_mask(input, result);
          store_pixel_premul(result, image);
          image += 4;
        }
      }
      else {
        BLI_assert_unreachable();
      }
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

static void colorBalance_apply(ModifierApplyContext &context, StripModifierData *smd, ImBuf *mask)
{
  const ColorBalanceModifierData *cbmd = (const ColorBalanceModifierData *)smd;

  ColorBalanceApplyOp op;
  op.init(*cbmd, context.image->byte_buffer.data != nullptr);
  apply_modifier_op(op, context.image, mask, context.transform);
}

static void colorBalance_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = UI_panel_custom_data_get(panel);

  PointerRNA color_balance = RNA_pointer_get(ptr, "color_balance");
  const int correction_method = RNA_enum_get(&color_balance, "correction_method");

  layout->use_property_split_set(true);

  layout->prop(ptr, "color_multiply", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(&color_balance, "correction_method", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  uiLayout &flow = layout->grid_flow(true, 0, true, false, false);
  flow.use_property_split_set(false);
  if (correction_method == SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN) {
    /* Split into separate scopes to be able to reuse "split" and "col" variable names. */
    {
      uiLayout &split = flow.column(false).split(0.35f, false);
      uiLayout &col = split.column(true);
      col.label(IFACE_("Lift"), ICON_NONE);
      col.separator();
      col.separator();
      col.prop(&color_balance, "lift", UI_ITEM_NONE, "", ICON_NONE);
      col.prop(
          &color_balance, "invert_lift", UI_ITEM_NONE, IFACE_("Invert"), ICON_ARROW_LEFTRIGHT);
      uiTemplateColorPicker(&split, &color_balance, "lift", true, false, false, true);
      col.separator();
    }
    {
      uiLayout &split = flow.column(false).split(0.35f, false);
      uiLayout &col = split.column(true);
      col.label(IFACE_("Gamma"), ICON_NONE);
      col.separator();
      col.separator();
      col.prop(&color_balance, "gamma", UI_ITEM_NONE, "", ICON_NONE);
      col.prop(
          &color_balance, "invert_gamma", UI_ITEM_NONE, IFACE_("Invert"), ICON_ARROW_LEFTRIGHT);
      uiTemplateColorPicker(&split, &color_balance, "gamma", true, false, true, true);
      col.separator();
    }
    {
      uiLayout &split = flow.column(false).split(0.35f, false);
      uiLayout &col = split.column(true);
      col.label(IFACE_("Gain"), ICON_NONE);
      col.separator();
      col.separator();
      col.prop(&color_balance, "gain", UI_ITEM_NONE, "", ICON_NONE);
      col.prop(
          &color_balance, "invert_gain", UI_ITEM_NONE, IFACE_("Invert"), ICON_ARROW_LEFTRIGHT);
      uiTemplateColorPicker(&split, &color_balance, "gain", true, false, true, true);
    }
  }
  else if (correction_method == SEQ_COLOR_BALANCE_METHOD_SLOPEOFFSETPOWER) {
    {
      uiLayout &split = flow.column(false).split(0.35f, false);
      uiLayout &col = split.column(true);
      col.label(IFACE_("Offset"), ICON_NONE);
      col.separator();
      col.separator();
      col.prop(&color_balance, "offset", UI_ITEM_NONE, "", ICON_NONE);
      col.prop(
          &color_balance, "invert_offset", UI_ITEM_NONE, IFACE_("Invert"), ICON_ARROW_LEFTRIGHT);
      uiTemplateColorPicker(&split, &color_balance, "offset", true, false, false, true);
      col.separator();
    }
    {
      uiLayout &split = flow.column(false).split(0.35f, false);
      uiLayout &col = split.column(true);
      col.label(IFACE_("Power"), ICON_NONE);
      col.separator();
      col.separator();
      col.prop(&color_balance, "power", UI_ITEM_NONE, "", ICON_NONE);
      col.prop(
          &color_balance, "invert_power", UI_ITEM_NONE, IFACE_("Invert"), ICON_ARROW_LEFTRIGHT);
      uiTemplateColorPicker(&split, &color_balance, "power", true, false, false, true);
      col.separator();
    }
    {
      uiLayout &split = flow.column(false).split(0.35f, false);
      uiLayout &col = split.column(true);
      col.label(IFACE_("Slope"), ICON_NONE);
      col.separator();
      col.separator();
      col.prop(&color_balance, "slope", UI_ITEM_NONE, "", ICON_NONE);
      col.prop(
          &color_balance, "invert_slope", UI_ITEM_NONE, IFACE_("Invert"), ICON_ARROW_LEFTRIGHT);
      uiTemplateColorPicker(&split, &color_balance, "slope", true, false, false, true);
    }
  }
  else {
    BLI_assert_unreachable();
  }

  if (uiLayout *mask_input_layout = layout->panel_prop(
          C, ptr, "open_mask_input_panel", IFACE_("Mask Input")))
  {
    draw_mask_input_type_settings(C, mask_input_layout, ptr);
  }
}

static void colorBalance_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eSeqModifierType_ColorBalance, colorBalance_panel_draw);
}

StripModifierTypeInfo seqModifierType_ColorBalance = {
    /*idname*/ "ColorBalance",
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Color Balance"),
    /*struct_name*/ "ColorBalanceModifierData",
    /*struct_size*/ sizeof(ColorBalanceModifierData),
    /*init_data*/ colorBalance_init_data,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*apply*/ colorBalance_apply,
    /*panel_register*/ colorBalance_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
};

};  // namespace blender::seq
