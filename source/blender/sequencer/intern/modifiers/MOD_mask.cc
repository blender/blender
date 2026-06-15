/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_math_base_c.hh"
#include "BLI_math_matrix.hh"

#include "BLT_translation.hh"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "PRF_profile.hh"

#include "SEQ_modifier.hh"
#include "SEQ_render.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "modifier.hh"
#include "render.hh"

namespace blender::seq {

struct MaskApplyOp {
  template<typename ImageT, typename MaskSampler>
  void apply(ImageT *image, MaskSampler &mask, int image_x, IndexRange y_range)
  {
    image += y_range.first() * image_x * 4;
    for (int64_t y : y_range) {
      mask.begin_row(y);
      for ([[maybe_unused]] int64_t x : IndexRange(image_x)) {
        float m = mask.load_mask_min();

        if constexpr (std::is_same_v<ImageT, uchar>) {
          /* Byte buffer is straight, so only affect on alpha itself, this is
           * the only way to alpha-over byte strip after applying mask modifier. */
          image[3] = uchar(image[3] * m);
        }
        else if constexpr (std::is_same_v<ImageT, float>) {
          /* Float buffers are pre-multiplied, so need to pre-multiply color as well to make it
           * easy to alpha-over masked strip. */
          float4 pix(image);
          pix *= m;
          *reinterpret_cast<float4 *>(image) = pix;
        }
        image += 4;
      }
    }
  }
};

static void maskmodifier_apply(ModifierApplyContext &context, StripModifierData *smd)
{
  PRF_scope_with_name("SeqModMask", ProfileCategory::Draw);
  ImBuf *mask = modifier_render_mask_input(context, *smd);
  if (mask != nullptr && (mask->byte_data() != nullptr || mask->float_data() != nullptr)) {
    ensure_ibuf_is_sequencer_space(context.render_data.scene, context.result.image, false);

    MaskApplyOp op;
    apply_modifier_op(op, context.result.image, mask, context.transform);

    /* Image has gained transparency. */
    context.result.image->color_mode = ImColorMode::RGBA;
  }

  if (mask != nullptr) {
    IMB_freeImBuf(mask);
  }
}

static void maskmodifier_panel_draw(const bContext *C, Panel *panel)
{
  ui::Layout &layout = *panel->layout;
  PointerRNA *ptr = ui::panel_custom_data_get(panel);

  draw_mask_input_type_settings(C, layout, ptr);
}

static void maskmodifier_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eSeqModifierType_Mask, maskmodifier_panel_draw);
}

StripModifierTypeInfo seqModifierType_Mask = {
    /*idname*/ "Mask",
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Mask"),
    /*struct_name*/ "SequencerMaskModifierData",
    /*struct_size*/ sizeof(SequencerMaskModifierData),
    /*init_data*/ nullptr,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*apply*/ maskmodifier_apply,
    /*panel_register*/ maskmodifier_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
};

};  // namespace blender::seq
