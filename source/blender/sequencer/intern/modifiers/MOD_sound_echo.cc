/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLT_translation.hh"

#include "DNA_sequence_types.h"

#include "SEQ_modifier.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "modifier.hh"

namespace blender::seq {

static void echomodifier_init_data(StripModifierData *smd)
{
  EchoModifierData *emd = (EchoModifierData *)smd;

  emd->delay = 1.0f;
  emd->feedback = 0.5f;
  emd->mix = 0.5f;
}

static void echomodifier_draw(const bContext * /*C*/, Panel *panel)
{
  ui::Layout &layout = *panel->layout;
  PointerRNA *ptr = UI_panel_custom_data_get(panel);

  layout.use_property_split_set(true);

  ui::Layout &col = layout.column(false);

  col.prop(ptr, "delay", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col.prop(ptr, "feedback", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col.prop(ptr, "mix", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void echomodifier_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eSeqModifierType_Echo, echomodifier_draw);
}

StripModifierTypeInfo seqModifierType_Echo = {
    /*idname*/ "Echo",
    /*name*/ CTX_N_(BLT_I18NCONTEXT_ID_SEQUENCE, "Echo"),
    /*struct_name*/ "EchoModifierData",
    /*struct_size*/ sizeof(EchoModifierData),
    /*init_data*/ echomodifier_init_data,
    /*free_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*apply*/ nullptr,
    /*panel_register*/ echomodifier_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
};

};  // namespace blender::seq
