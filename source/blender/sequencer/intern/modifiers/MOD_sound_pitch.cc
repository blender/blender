/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLT_translation.hh"
#include <fmt/format.h>

#include "DNA_sequence_types.h"

#include "SEQ_modifier.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "modifier.hh"

namespace blender::seq {

static void pitchmodifier_init_data(StripModifierData *smd)
{
  PitchModifierData *pmd = (PitchModifierData *)smd;
  pmd->mode = ePitchMode::PITCH_MODE_SEMITONES;
  pmd->semitones = 0;
  pmd->cents = 0;
  pmd->ratio = 1;
  pmd->preserve_formant = false;
  pmd->quality = ePitchQuality::PITCH_QUALITY_HIGH;
}

static void pitchmodifier_draw(const bContext * /*C*/, Panel *panel)
{
  ui::Layout &layout = *panel->layout;
  PointerRNA *ptr = blender::ui::panel_custom_data_get(panel);

  layout.use_property_split_set(true);

  ui::Layout &col = layout.column(false);

  col.prop(ptr, "mode", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  int mode = RNA_enum_get(ptr, "mode");
  if (mode == ePitchMode::PITCH_MODE_SEMITONES) {
    col.prop(ptr, "semitones", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col.prop(ptr, "cents", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else if (mode == ePitchMode::PITCH_MODE_RATIO) {
    col.prop(ptr, "ratio", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  col.prop(ptr, "preserve_formant", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col.prop(ptr, "quality", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void pitchmodifier_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eSeqModifierType_Pitch, pitchmodifier_draw);
}

StripModifierTypeInfo seqModifierType_Pitch = {
    /*idname*/ "Pitch",
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Pitch"),
    /*struct_name*/ "PitchModifierData",
    /*struct_size*/ sizeof(PitchModifierData),
    /*init_data*/ pitchmodifier_init_data,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*apply*/ nullptr,
    /*panel_register*/ pitchmodifier_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
};

};  // namespace blender::seq
