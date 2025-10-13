/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_math_vector.h"

#include "BLT_translation.hh"

#include "DNA_sequence_types.h"

#include "SEQ_modifier.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "modifier.hh"

namespace blender::seq {

static void whiteBalance_init_data(StripModifierData *smd)
{
  WhiteBalanceModifierData *cbmd = (WhiteBalanceModifierData *)smd;
  copy_v3_fl(cbmd->white_value, 1.0f);
}

struct WhiteBalanceApplyOp {
  float multiplier[3];

  template<typename ImageT, typename MaskSampler>
  void apply(ImageT *image, MaskSampler &mask, int image_x, IndexRange y_range)
  {
    image += y_range.first() * image_x * 4;
    for (int64_t y : y_range) {
      mask.begin_row(y);
      for ([[maybe_unused]] int64_t x : IndexRange(image_x)) {
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

        mask.apply_mask(input, result);
        store_pixel_premul(result, image);
        image += 4;
      }
    }
  }
};

static void whiteBalance_apply(ModifierApplyContext &context, StripModifierData *smd, ImBuf *mask)
{
  const WhiteBalanceModifierData *data = (const WhiteBalanceModifierData *)smd;

  WhiteBalanceApplyOp op;
  op.multiplier[0] = (data->white_value[0] != 0.0f) ? 1.0f / data->white_value[0] : FLT_MAX;
  op.multiplier[1] = (data->white_value[1] != 0.0f) ? 1.0f / data->white_value[1] : FLT_MAX;
  op.multiplier[2] = (data->white_value[2] != 0.0f) ? 1.0f / data->white_value[2] : FLT_MAX;
  apply_modifier_op(op, context.image, mask, context.transform);
}

static void whiteBalance_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = UI_panel_custom_data_get(panel);

  layout->use_property_split_set(true);

  layout->prop(ptr, "white_value", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (uiLayout *mask_input_layout = layout->panel_prop(
          C, ptr, "open_mask_input_panel", IFACE_("Mask Input")))
  {
    draw_mask_input_type_settings(C, mask_input_layout, ptr);
  }
}

static void whiteBalance_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eSeqModifierType_WhiteBalance, whiteBalance_panel_draw);
}

StripModifierTypeInfo seqModifierType_WhiteBalance = {
    /*idname*/ "WhiteBalance",
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "White Balance"),
    /*struct_name*/ "WhiteBalanceModifierData",
    /*struct_size*/ sizeof(WhiteBalanceModifierData),
    /*init_data*/ whiteBalance_init_data,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*apply*/ whiteBalance_apply,
    /*panel_register*/ whiteBalance_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
};

};  // namespace blender::seq
