/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Template for building the panel layout for the active object's modifiers.
 */

#include "BKE_context.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "BLI_listbase.h"

#include "ED_object.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"

static void modifier_panel_id(void *md_link, char *r_name)
{
  ModifierData *md = (ModifierData *)md_link;
  BKE_modifier_type_panel_id(ModifierType(md->type), r_name);
}

void uiTemplateModifiers(uiLayout * /*layout*/, bContext *C)
{
  ARegion *region = CTX_wm_region(C);

  Object *ob = blender::ed::object::context_active_object(C);
  ListBase *modifiers = &ob->modifiers;

  const bool panels_match = UI_panel_list_matches_data(region, modifiers, modifier_panel_id);

  if (!panels_match) {
    UI_panels_free_instanced(C, region);
    LISTBASE_FOREACH (ModifierData *, md, modifiers) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
      if (mti->panel_register == nullptr) {
        continue;
      }

      char panel_idname[MAX_NAME];
      modifier_panel_id(md, panel_idname);

      /* Create custom data RNA pointer. */
      PointerRNA *md_ptr = MEM_new<PointerRNA>(__func__);
      *md_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Modifier, md);

      UI_panel_add_instanced(C, region, &region->panels, panel_idname, md_ptr);
    }
  }
  else {
    /* Assuming there's only one group of instanced panels, update the custom data pointers. */
    Panel *panel = static_cast<Panel *>(region->panels.first);
    LISTBASE_FOREACH (ModifierData *, md, modifiers) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
      if (mti->panel_register == nullptr) {
        continue;
      }

      /* Move to the next instanced panel corresponding to the next modifier. */
      while ((panel->type == nullptr) || !(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        panel = panel->next;
        BLI_assert(panel !=
                   nullptr); /* There shouldn't be fewer panels than modifiers with UIs. */
      }

      PointerRNA *md_ptr = MEM_new<PointerRNA>(__func__);
      *md_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Modifier, md);
      UI_panel_custom_data_set(panel, md_ptr);

      panel = panel->next;
    }
  }
}
