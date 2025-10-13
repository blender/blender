/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_math_color.h"

#include "BKE_colortools.hh"

#include "BLT_translation.hh"

#include "DNA_curve_enums.h"
#include "DNA_sequence_types.h"

#include "SEQ_modifier.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "modifier.hh"

namespace blender::seq {

static void hue_correct_init_data(StripModifierData *smd)
{
  HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;
  int c;

  BKE_curvemapping_set_defaults(&hcmd->curve_mapping, 1, 0.0f, 0.0f, 1.0f, 1.0f, HD_AUTO);
  hcmd->curve_mapping.preset = CURVE_PRESET_MID8;

  for (c = 0; c < 3; c++) {
    CurveMap *cuma = &hcmd->curve_mapping.cm[c];
    BKE_curvemap_reset(
        cuma, &hcmd->curve_mapping.clipr, hcmd->curve_mapping.preset, CurveMapSlopeType::Positive);
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

  template<typename ImageT, typename MaskSampler>
  void apply(ImageT *image, MaskSampler &mask, int image_x, IndexRange y_range)
  {
    image += y_range.first() * image_x * 4;
    for (int64_t y : y_range) {
      mask.begin_row(y);
      for ([[maybe_unused]] int64_t x : IndexRange(image_x)) {
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

        mask.apply_mask(input, result);
        store_pixel_raw(result, image);
        image += 4;
      }
    }
  }
};

static void hue_correct_apply(ModifierApplyContext &context, StripModifierData *smd, ImBuf *mask)
{
  HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;

  BKE_curvemapping_init(&hcmd->curve_mapping);

  HueCorrectApplyOp op;
  op.curve_mapping = &hcmd->curve_mapping;
  apply_modifier_op(op, context.image, mask, context.transform);
}

static void hue_correct_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = UI_panel_custom_data_get(panel);

  uiTemplateCurveMapping(layout, ptr, "curve_mapping", 'h', false, false, false, false, false);

  if (uiLayout *mask_input_layout = layout->panel_prop(
          C, ptr, "open_mask_input_panel", IFACE_("Mask Input")))
  {
    draw_mask_input_type_settings(C, mask_input_layout, ptr);
  }
}

static void hue_correct_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eSeqModifierType_HueCorrect, hue_correct_panel_draw);
}

static void hue_correct_write(BlendWriter *writer, const StripModifierData *smd)
{
  const HueCorrectModifierData *hmd = reinterpret_cast<const HueCorrectModifierData *>(smd);
  BKE_curvemapping_blend_write(writer, &hmd->curve_mapping);
}

static void hue_correct_read(BlendDataReader *reader, StripModifierData *smd)
{
  HueCorrectModifierData *hmd = reinterpret_cast<HueCorrectModifierData *>(smd);
  BKE_curvemapping_blend_read(reader, &hmd->curve_mapping);
}

StripModifierTypeInfo seqModifierType_HueCorrect = {
    /*idname*/ "HueCorrect",
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Hue Correct"),
    /*struct_name*/ "HueCorrectModifierData",
    /*struct_size*/ sizeof(HueCorrectModifierData),
    /*init_data*/ hue_correct_init_data,
    /*free_data*/ hue_correct_free_data,
    /*copy_data*/ hue_correct_copy_data,
    /*apply*/ hue_correct_apply,
    /*panel_register*/ hue_correct_register,
    /*blend_write*/ hue_correct_write,
    /*blend_read*/ hue_correct_read,
};

};  // namespace blender::seq
