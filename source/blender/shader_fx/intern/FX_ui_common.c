/* This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup modifiers
 */

#include <string.h>

#include "BLI_listbase.h"

#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_screen.h"
#include "BKE_shader_fx.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_space_types.h"

#include "ED_object.h"

#include "BLT_translation.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "FX_ui_common.h" /* Self include */

static Object *get_context_object(const bContext *C)
{
  SpaceProperties *sbuts = CTX_wm_space_properties(C);
  if (sbuts != NULL && (sbuts->pinid != NULL) && GS(sbuts->pinid->name) == ID_OB) {
    return (Object *)sbuts->pinid;
  }
  else {
    return CTX_data_active_object(C);
  }
}

/* -------------------------------------------------------------------- */
/** \name Panel Drag and Drop, Expansion Saving
 * \{ */

/**
 * Move an effect to the index it's moved to after a drag and drop.
 */
static void shaderfx_reorder(bContext *C, Panel *panel, int new_index)
{
  Object *ob = get_context_object(C);

  ShaderFxData *fx = BLI_findlink(&ob->shader_fx, panel->runtime.list_index);
  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_shaderfx_move_to_index", false);
  WM_operator_properties_create_ptr(&props_ptr, ot);
  RNA_string_set(&props_ptr, "shaderfx", fx->name);
  RNA_int_set(&props_ptr, "index", new_index);
  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &props_ptr);
}

/**
 * Get the expand flag from the active effect to use for the panel.
 */
static short get_shaderfx_expand_flag(const bContext *C, Panel *panel)
{
  Object *ob = get_context_object(C);
  ShaderFxData *fx = BLI_findlink(&ob->shader_fx, panel->runtime.list_index);
  return fx->ui_expand_flag;
}

/**
 * Save the expand flag for the panel and subpanels to the effect.
 */
static void set_shaderfx_expand_flag(const bContext *C, Panel *panel, short expand_flag)
{
  Object *ob = get_context_object(C);
  ShaderFxData *fx = BLI_findlink(&ob->shader_fx, panel->runtime.list_index);
  fx->ui_expand_flag = expand_flag;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ShaderFx Panel Layouts
 * \{ */

/**
 * Draw shaderfx error message.
 */
void shaderfx_panel_end(uiLayout *layout, PointerRNA *ptr)
{
  ShaderFxData *fx = ptr->data;
  if (fx->error) {
    uiLayout *row = uiLayoutRow(layout, false);
    uiItemL(row, IFACE_(fx->error), ICON_ERROR);
  }
}

/**
 * Gets RNA pointers for the active object and the panel's shaderfx data.
 */
void shaderfx_panel_get_property_pointers(const bContext *C,
                                          Panel *panel,
                                          PointerRNA *r_ob_ptr,
                                          PointerRNA *r_md_ptr)
{
  Object *ob = get_context_object(C);
  ShaderFxData *md = BLI_findlink(&ob->shader_fx, panel->runtime.list_index);

  RNA_pointer_create(&ob->id, &RNA_ShaderFx, md, r_md_ptr);

  if (r_ob_ptr != NULL) {
    RNA_pointer_create(&ob->id, &RNA_Object, ob, r_ob_ptr);
  }

  uiLayoutSetContextPointer(panel->layout, "shaderfx", r_md_ptr);
}

#define ERROR_LIBDATA_MESSAGE TIP_("External library data")

static void shaderfx_panel_header(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  bool narrow_panel = (panel->sizex < UI_UNIT_X * 7 && panel->sizex != 0);

  PointerRNA ptr;
  shaderfx_panel_get_property_pointers(C, panel, NULL, &ptr);
  Object *ob = get_context_object(C);
  ShaderFxData *fx = (ShaderFxData *)ptr.data;

  const ShaderFxTypeInfo *fxti = BKE_shaderfx_get_info(fx->type);

  UI_block_lock_set(uiLayoutGetBlock(layout), (ob && ID_IS_LINKED(ob)), ERROR_LIBDATA_MESSAGE);

  /* Effect type icon. */
  uiLayout *row = uiLayoutRow(layout, false);
  if (fxti->isDisabled && fxti->isDisabled(fx, 0)) {
    uiLayoutSetRedAlert(row, true);
  }
  uiItemL(row, "", RNA_struct_ui_icon(ptr.type));

  /* Effect name. */
  row = uiLayoutRow(layout, true);
  if (!narrow_panel) {
    uiItemR(row, &ptr, "name", 0, "", ICON_NONE);
  }

  /* Mode enabling buttons. */
  if (fxti->flags & eShaderFxTypeFlag_SupportsEditmode) {
    uiLayout *sub = uiLayoutRow(row, true);
    uiLayoutSetActive(sub, false);
    uiItemR(sub, &ptr, "show_in_editmode", 0, "", ICON_NONE);
  }
  uiItemR(row, &ptr, "show_viewport", 0, "", ICON_NONE);
  uiItemR(row, &ptr, "show_render", 0, "", ICON_NONE);

  row = uiLayoutRow(row, false);
  uiLayoutSetEmboss(row, UI_EMBOSS_NONE);
  uiItemO(row, "", ICON_X, "OBJECT_OT_shaderfx_remove");

  /* Some padding so the X isn't too close to the drag icon. */
  uiItemS(layout);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ShaderFx Registration Helpers
 * \{ */

static bool shaderfx_ui_poll(const bContext *C, PanelType *UNUSED(pt))
{
  Object *ob = get_context_object(C);

  return (ob != NULL) && (ob->type == OB_GPENCIL);
}

/**
 * Create a panel in the context's region
 */
PanelType *shaderfx_panel_register(ARegionType *region_type, ShaderFxType type, PanelDrawFn draw)
{

  /* Get the name for the effect's panel. */
  char panel_idname[BKE_ST_MAXNAME];
  BKE_shaderfxType_panel_id(type, panel_idname);

  PanelType *panel_type = MEM_callocN(sizeof(PanelType), panel_idname);

  strcpy(panel_type->idname, panel_idname);
  strcpy(panel_type->label, "");
  strcpy(panel_type->context, "shaderfx");
  strcpy(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);

  panel_type->draw_header = shaderfx_panel_header;
  panel_type->draw = draw;
  panel_type->poll = shaderfx_ui_poll;

  /* Give the panel the special flag that says it was built here and corresponds to a
   * shader effect rather than a PanelType. */
  panel_type->flag = PNL_LAYOUT_HEADER_EXPAND | PNL_DRAW_BOX | PNL_INSTANCED;
  panel_type->reorder = shaderfx_reorder;
  panel_type->get_list_data_expand_flag = get_shaderfx_expand_flag;
  panel_type->set_list_data_expand_flag = set_shaderfx_expand_flag;

  BLI_addtail(&region_type->paneltypes, panel_type);

  return panel_type;
}

/**
 * Add a child panel to the parent.
 *
 * \note To create the panel type's idname, it appends the \a name argument to the \a parent's
 * idname.
 */
PanelType *shaderfx_subpanel_register(ARegionType *region_type,
                                      const char *name,
                                      const char *label,
                                      PanelDrawFn draw_header,
                                      PanelDrawFn draw,
                                      PanelType *parent)
{
  /* Create the subpanel's ID name. */
  char panel_idname[BKE_ST_MAXNAME];
  strcpy(panel_idname, parent->idname);
  strcat(panel_idname, "_");
  strcat(panel_idname, name);

  PanelType *panel_type = MEM_callocN(sizeof(PanelType), panel_idname);

  strcpy(panel_type->idname, panel_idname);
  strcpy(panel_type->label, label);
  strcpy(panel_type->context, "shaderfx");
  strcpy(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);

  panel_type->draw_header = draw_header;
  panel_type->draw = draw;
  panel_type->poll = shaderfx_ui_poll;
  panel_type->flag = (PNL_DEFAULT_CLOSED | PNL_DRAW_BOX);

  BLI_assert(parent != NULL);
  strcpy(panel_type->parent_id, parent->idname);
  panel_type->parent = parent;
  BLI_addtail(&parent->children, BLI_genericNodeN(panel_type));
  BLI_addtail(&region_type->paneltypes, panel_type);

  return panel_type;
}

/** \} */
