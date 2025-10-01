/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <fmt/format.h>

#include "BKE_colortools.hh"
#include "BLI_listbase.h"
#include "BLO_read_write.hh"
#include "BLT_translation.hh"

#include "DNA_sequence_types.h"

#include "SEQ_modifier.hh"
#include "SEQ_sound.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "modifier.hh"

namespace blender::seq {

static void sound_equalizermodifier_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA *ptr = UI_panel_custom_data_get(panel);

  layout->use_property_split_set(true);

  uiLayout &flow = layout->grid_flow(true, 0, true, false, false);
  RNA_BEGIN (ptr, sound_eq, "graphics") {
    PointerRNA curve_mapping = RNA_pointer_get(&sound_eq, "curve_mapping");
    const float clip_min_x = RNA_float_get(&curve_mapping, "clip_min_x");
    const float clip_max_x = RNA_float_get(&curve_mapping, "clip_max_x");

    uiLayout &col = flow.column(false);
    uiLayout &split = col.split(0.4f, false);
    split.label(fmt::format("{:.2f}", clip_min_x), ICON_NONE);
    split.label("Hz", ICON_NONE);
    split.alignment_set(ui::LayoutAlign::Right);
    split.label(fmt::format("{:.2f}", clip_max_x), ICON_NONE);
    uiTemplateCurveMapping(&col, &sound_eq, "curve_mapping", 0, false, true, true, false, false);
    uiLayout &row = col.row(false);
    row.alignment_set(ui::LayoutAlign::Center);
    row.label("dB", ICON_NONE);
  }
  RNA_END;
}

static void sound_equalizermodifier_register(ARegionType *region_type)
{
  modifier_panel_register(
      region_type, eSeqModifierType_SoundEqualizer, sound_equalizermodifier_draw);
}

static void sound_equalizermodifier_write(BlendWriter *writer, const StripModifierData *smd)
{
  const SoundEqualizerModifierData *semd = reinterpret_cast<const SoundEqualizerModifierData *>(
      smd);
  LISTBASE_FOREACH (EQCurveMappingData *, eqcmd, &semd->graphics) {
    BLO_write_struct_by_name(writer, "EQCurveMappingData", eqcmd);
    BKE_curvemapping_blend_write(writer, &eqcmd->curve_mapping);
  }
}

static void sound_equalizermodifier_read(BlendDataReader *reader, StripModifierData *smd)
{
  SoundEqualizerModifierData *semd = reinterpret_cast<SoundEqualizerModifierData *>(smd);
  BLO_read_struct_list(reader, EQCurveMappingData, &semd->graphics);
  LISTBASE_FOREACH (EQCurveMappingData *, eqcmd, &semd->graphics) {
    BKE_curvemapping_blend_read(reader, &eqcmd->curve_mapping);
  }
}

StripModifierTypeInfo seqModifierType_SoundEqualizer = {
    /*idname*/ "SoundEqualizer",
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Equalizer"),
    /*struct_name*/ "SoundEqualizerModifierData",
    /*struct_size*/ sizeof(SoundEqualizerModifierData),
    /*init_data*/ sound_equalizermodifier_init_data,
    /*free_data*/ sound_equalizermodifier_free,
    /*copy_data*/ sound_equalizermodifier_copy_data,
    /*apply*/ nullptr,
    /*panel_register*/ sound_equalizermodifier_register,
    /*blend_write*/ sound_equalizermodifier_write,
    /*blend_read*/ sound_equalizermodifier_read,
};

};  // namespace blender::seq
