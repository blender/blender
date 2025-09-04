/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_math_base.h"

#include "BLT_translation.hh"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "SEQ_modifier.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "modifier.hh"

namespace blender::seq {

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

static void maskmodifier_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = UI_panel_custom_data_get(panel);

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
};

};  // namespace blender::seq
