/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Template for building the panel layout for the active object's grease pencil shader effects.
 */

#include "BLI_listbase.h"

#include "BKE_screen.hh"
#include "BKE_shader_fx.h"

#include "ED_object.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

/**
 * Function with void * argument for #uiListPanelIDFromDataFunc.
 */
static void shaderfx_panel_id(void *fx_v, char *r_idname)
{
  ShaderFxData *fx = (ShaderFxData *)fx_v;
  BKE_shaderfxType_panel_id(ShaderFxType(fx->type), r_idname);
}

void uiTemplateShaderFx(uiLayout * /*layout*/, bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  Object *ob = blender::ed::object::context_active_object(C);
  ListBase *shaderfx = &ob->shader_fx;

  const bool panels_match = UI_panel_list_matches_data(region, shaderfx, shaderfx_panel_id);

  if (!panels_match) {
    UI_panels_free_instanced(C, region);
    LISTBASE_FOREACH (ShaderFxData *, fx, shaderfx) {
      char panel_idname[MAX_NAME];
      shaderfx_panel_id(fx, panel_idname);

      /* Create custom data RNA pointer. */
      PointerRNA *fx_ptr = MEM_new<PointerRNA>(__func__);
      *fx_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_ShaderFx, fx);

      UI_panel_add_instanced(C, region, &region->panels, panel_idname, fx_ptr);
    }
  }
  else {
    /* Assuming there's only one group of instanced panels, update the custom data pointers. */
    Panel *panel = static_cast<Panel *>(region->panels.first);
    LISTBASE_FOREACH (ShaderFxData *, fx, shaderfx) {
      const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info(ShaderFxType(fx->type));
      if (fxi->panel_register == nullptr) {
        continue;
      }

      /* Move to the next instanced panel corresponding to the next modifier. */
      while ((panel->type == nullptr) || !(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        panel = panel->next;
        BLI_assert(panel !=
                   nullptr); /* There shouldn't be fewer panels than modifiers with UIs. */
      }

      PointerRNA *fx_ptr = MEM_new<PointerRNA>(__func__);
      *fx_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_ShaderFx, fx);
      UI_panel_custom_data_set(panel, fx_ptr);

      panel = panel->next;
    }
  }
}
