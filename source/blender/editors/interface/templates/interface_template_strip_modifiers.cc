/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Template for building the panel layout for the active strip's modifiers.
 */

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLI_listbase.h"

#include "SEQ_modifier.hh"
#include "SEQ_select.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

namespace blender::ui {

static void strip_modifier_panel_id(void *smd_link, char *r_name)
{
  StripModifierData *smd = reinterpret_cast<StripModifierData *>(smd_link);
  seq::modifier_type_panel_id(eStripModifierType(smd->type), r_name);
}

void template_strip_modifiers(Layout * /*layout*/, bContext *C)
{
  ARegion *region = CTX_wm_region(C);

  Scene *sequencer_scene = CTX_data_sequencer_scene(C);
  if (!sequencer_scene) {
    return;
  }
  Strip *active_strip = seq::select_active_get(sequencer_scene);
  BLI_assert(active_strip != nullptr);
  ListBaseT<StripModifierData> *modifiers = &active_strip->modifiers;

  const bool panels_match = panel_list_matches_data(region, modifiers, strip_modifier_panel_id);

  if (!panels_match) {
    panels_free_instanced(C, region);
    for (StripModifierData &smd : *modifiers) {
      const seq::StripModifierTypeInfo *mti = seq::modifier_type_info_get(smd.type);
      if (mti->panel_register == nullptr) {
        continue;
      }

      char panel_idname[MAX_NAME];
      strip_modifier_panel_id(&smd, panel_idname);

      /* Create custom data RNA pointer. */
      PointerRNA *md_ptr = MEM_new<PointerRNA>(__func__);
      *md_ptr = RNA_pointer_create_discrete(&sequencer_scene->id, RNA_StripModifier, &smd);

      panel_add_instanced(C, region, &region->panels, panel_idname, md_ptr);
    }
  }
  else {
    /* Assuming there's only one group of instanced panels, update the custom data pointers. */
    Panel *panel = static_cast<Panel *>(region->panels.first);
    for (StripModifierData &smd : *modifiers) {
      const seq::StripModifierTypeInfo *mti = seq::modifier_type_info_get(smd.type);
      if (mti->panel_register == nullptr) {
        continue;
      }

      /* Move to the next instanced panel corresponding to the next modifier. */
      while ((panel->type == nullptr) || !(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        panel = panel->next;
        /* There shouldn't be fewer panels than modifiers with UIs. */
        BLI_assert(panel != nullptr);
      }

      PointerRNA *md_ptr = MEM_new<PointerRNA>(__func__);
      *md_ptr = RNA_pointer_create_discrete(&sequencer_scene->id, RNA_StripModifier, &smd);
      panel_custom_data_set(panel, md_ptr);

      panel = panel->next;
    }
  }
}

}  // namespace blender::ui
