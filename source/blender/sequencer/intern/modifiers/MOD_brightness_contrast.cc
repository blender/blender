/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <cfloat>

#include "BLI_math_base.h"
#include "BLI_math_vector.hh"

#include "BLT_translation.hh"

#include "DNA_sequence_types.h"

#include "SEQ_modifier.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "modifier.hh"

namespace blender::seq {

struct BrightContrastApplyOp {
  float mul;
  float add;

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
        result = input * this->mul + this->add;
        result.w = input.w;

        mask.apply_mask(input, result);
        store_pixel_raw(result, image);
        image += 4;
      }
    }
  }
};

static void brightcontrast_apply(ModifierApplyContext &context,
                                 StripModifierData *smd,
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

  apply_modifier_op(op, context.image, mask, context.transform);
}

static void brightcontrast_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = UI_panel_custom_data_get(panel);

  layout->use_property_split_set(true);

  layout->prop(ptr, "bright", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "contrast", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (uiLayout *mask_input_layout = layout->panel_prop(
          C, ptr, "open_mask_input_panel", IFACE_("Mask Input")))
  {
    draw_mask_input_type_settings(C, mask_input_layout, ptr);
  }
}

static void brightcontrast_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eSeqModifierType_BrightContrast, brightcontrast_panel_draw);
}

StripModifierTypeInfo seqModifierType_BrightContrast = {
    /*idname*/ "BrightContrast",
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Brightness/Contrast"),
    /*struct_name*/ "BrightContrastModifierData",
    /*struct_size*/ sizeof(BrightContrastModifierData),
    /*init_data*/ nullptr,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*apply*/ brightcontrast_apply,
    /*panel_register*/ brightcontrast_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
};

};  // namespace blender::seq
