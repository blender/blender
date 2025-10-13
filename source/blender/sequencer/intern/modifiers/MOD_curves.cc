/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BKE_colortools.hh"

#include "BLT_translation.hh"

#include "DNA_curve_enums.h"
#include "DNA_sequence_types.h"

#include "SEQ_modifier.hh"

#include "UI_interface.hh"
#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"

#include "modifier.hh"

namespace blender::seq {

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

  template<typename ImageT, typename MaskSampler>
  void apply(ImageT *image, MaskSampler &mask, int image_x, IndexRange y_range)
  {
    image += y_range.first() * image_x * 4;
    for (int64_t y : y_range) {
      mask.begin_row(y);
      for ([[maybe_unused]] int64_t x : IndexRange(image_x)) {
        float4 input = load_pixel_premul(image);

        float4 result;
        BKE_curvemapping_evaluate_premulRGBF(this->curve_mapping, result, input);
        result.w = input.w;

        mask.apply_mask(input, result);
        store_pixel_premul(result, image);
        image += 4;
      }
    }
  }
};

static void curves_apply(ModifierApplyContext &context, StripModifierData *smd, ImBuf *mask)
{
  CurvesModifierData *cmd = (CurvesModifierData *)smd;

  const float black[3] = {0.0f, 0.0f, 0.0f};
  const float white[3] = {1.0f, 1.0f, 1.0f};

  BKE_curvemapping_init(&cmd->curve_mapping);

  BKE_curvemapping_premultiply(&cmd->curve_mapping, false);
  BKE_curvemapping_set_black_white(&cmd->curve_mapping, black, white);

  CurvesApplyOp op;
  op.curve_mapping = &cmd->curve_mapping;
  apply_modifier_op(op, context.image, mask, context.transform);

  BKE_curvemapping_premultiply(&cmd->curve_mapping, true);
}

static void curves_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = UI_panel_custom_data_get(panel);

  uiTemplateCurveMapping(layout, ptr, "curve_mapping", 'c', false, false, false, true, false);

  if (uiLayout *mask_input_layout = layout->panel_prop(
          C, ptr, "open_mask_input_panel", IFACE_("Mask Input")))
  {
    draw_mask_input_type_settings(C, mask_input_layout, ptr);
  }
}

static void curves_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eSeqModifierType_Curves, curves_panel_draw);
}

static void curves_write(BlendWriter *writer, const StripModifierData *smd)
{
  const CurvesModifierData *cmd = reinterpret_cast<const CurvesModifierData *>(smd);
  BKE_curvemapping_blend_write(writer, &cmd->curve_mapping);
}

static void curves_read(BlendDataReader *reader, StripModifierData *smd)
{
  CurvesModifierData *cmd = reinterpret_cast<CurvesModifierData *>(smd);
  BKE_curvemapping_blend_read(reader, &cmd->curve_mapping);
}

StripModifierTypeInfo seqModifierType_Curves = {
    /*idname*/ "Curves",
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Curves"),
    /*struct_name*/ "CurvesModifierData",
    /*struct_size*/ sizeof(CurvesModifierData),
    /*init_data*/ curves_init_data,
    /*free_data*/ curves_free_data,
    /*copy_data*/ curves_copy_data,
    /*apply*/ curves_apply,
    /*panel_register*/ curves_register,
    /*blend_write*/ curves_write,
    /*blend_read*/ curves_read,
};

};  // namespace blender::seq
