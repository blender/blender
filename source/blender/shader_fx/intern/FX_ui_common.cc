/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_screen.h"
#include "BKE_shader_fx.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_shader_fx_types.h"

#include "ED_object.hh"

#include "BLT_translation.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FX_ui_common.h" /* Self include */

/* -------------------------------------------------------------------- */
/** \name Panel Drag and Drop, Expansion Saving
 * \{ */

/**
 * Move an effect to the index it's moved to after a drag and drop.
 */
static void shaderfx_reorder(bContext *C, Panel *panel, int new_index)
{
  PointerRNA *fx_ptr = UI_panel_custom_data_get(panel);
  ShaderFxData *fx = (ShaderFxData *)fx_ptr->data;

  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_shaderfx_move_to_index", false);
  WM_operator_properties_create_ptr(&props_ptr, ot);
  RNA_string_set(&props_ptr, "shaderfx", fx->name);
  RNA_int_set(&props_ptr, "index", new_index);
  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &props_ptr, nullptr);
  WM_operator_properties_free(&props_ptr);
}

/**
 * Get the expand flag from the active effect to use for the panel.
 */
static short get_shaderfx_expand_flag(const bContext * /*C*/, Panel *panel)
{
  PointerRNA *fx_ptr = UI_panel_custom_data_get(panel);
  ShaderFxData *fx = (ShaderFxData *)fx_ptr->data;
  return fx->ui_expand_flag;
}

/**
 * Save the expand flag for the panel and sub-panels to the effect.
 */
static void set_shaderfx_expand_flag(const bContext * /*C*/, Panel *panel, short expand_flag)
{
  PointerRNA *fx_ptr = UI_panel_custom_data_get(panel);
  ShaderFxData *fx = (ShaderFxData *)fx_ptr->data;
  fx->ui_expand_flag = expand_flag;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ShaderFx Panel Layouts
 * \{ */

void shaderfx_panel_end(uiLayout *layout, PointerRNA *ptr)
{
  ShaderFxData *fx = static_cast<ShaderFxData *>(ptr->data);
  if (fx->error) {
    uiLayout *row = uiLayoutRow(layout, false);
    uiItemL(row, TIP_(fx->error), ICON_ERROR);
  }
}

PointerRNA *shaderfx_panel_get_property_pointers(Panel *panel, PointerRNA *r_ob_ptr)
{
  PointerRNA *ptr = UI_panel_custom_data_get(panel);
  BLI_assert(RNA_struct_is_a(ptr->type, &RNA_ShaderFx));

  if (r_ob_ptr != nullptr) {
    RNA_pointer_create(ptr->owner_id, &RNA_Object, ptr->owner_id, r_ob_ptr);
  }

  UI_panel_context_pointer_set(panel, "shaderfx", ptr);

  return ptr;
}

#define ERROR_LIBDATA_MESSAGE TIP_("External library data")

static void gpencil_shaderfx_ops_extra_draw(bContext *C, uiLayout *layout, void *fx_v)
{
  PointerRNA op_ptr;
  uiLayout *row;
  ShaderFxData *fx = (ShaderFxData *)fx_v;

  PointerRNA ptr;
  Object *ob = ED_object_active_context(C);
  RNA_pointer_create(&ob->id, &RNA_ShaderFx, fx, &ptr);
  uiLayoutSetContextPointer(layout, "shaderfx", &ptr);
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  uiLayoutSetUnitsX(layout, 4.0f);

  /* Duplicate. */
  uiItemO(layout,
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Duplicate"),
          ICON_DUPLICATE,
          "OBJECT_OT_shaderfx_copy");

  uiItemS(layout);

  /* Move to first. */
  row = uiLayoutColumn(layout, false);
  uiItemFullO(row,
              "OBJECT_OT_shaderfx_move_to_index",
              IFACE_("Move to First"),
              ICON_TRIA_UP,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              UI_ITEM_NONE,
              &op_ptr);
  RNA_int_set(&op_ptr, "index", 0);
  if (!fx->prev) {
    uiLayoutSetEnabled(row, false);
  }

  /* Move to last. */
  row = uiLayoutColumn(layout, false);
  uiItemFullO(row,
              "OBJECT_OT_shaderfx_move_to_index",
              IFACE_("Move to Last"),
              ICON_TRIA_DOWN,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              UI_ITEM_NONE,
              &op_ptr);
  RNA_int_set(&op_ptr, "index", BLI_listbase_count(&ob->shader_fx) - 1);
  if (!fx->next) {
    uiLayoutSetEnabled(row, false);
  }
}

static void shaderfx_panel_header(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  bool narrow_panel = (panel->sizex < UI_UNIT_X * 7 && panel->sizex != 0);

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);
  Object *ob = (Object *)ptr->owner_id;
  ShaderFxData *fx = (ShaderFxData *)ptr->data;

  const ShaderFxTypeInfo *fxti = BKE_shaderfx_get_info(ShaderFxType(fx->type));

  UI_block_lock_set(uiLayoutGetBlock(layout), (ob && ID_IS_LINKED(ob)), ERROR_LIBDATA_MESSAGE);

  /* Effect type icon. */
  uiLayout *row = uiLayoutRow(layout, false);
  if (fxti->is_disabled && fxti->is_disabled(fx, false)) {
    uiLayoutSetRedAlert(row, true);
  }
  uiItemL(row, "", RNA_struct_ui_icon(ptr->type));

  /* Effect name. */
  row = uiLayoutRow(layout, true);
  if (!narrow_panel) {
    uiItemR(row, ptr, "name", UI_ITEM_NONE, "", ICON_NONE);
  }

  /* Mode enabling buttons. */
  if (fxti->flags & eShaderFxTypeFlag_SupportsEditmode) {
    uiLayout *sub = uiLayoutRow(row, true);
    uiLayoutSetActive(sub, false);
    uiItemR(sub, ptr, "show_in_editmode", UI_ITEM_NONE, "", ICON_NONE);
  }
  uiItemR(row, ptr, "show_viewport", UI_ITEM_NONE, "", ICON_NONE);
  uiItemR(row, ptr, "show_render", UI_ITEM_NONE, "", ICON_NONE);

  /* Extra operators. */
  uiItemMenuF(row, "", ICON_DOWNARROW_HLT, gpencil_shaderfx_ops_extra_draw, fx);

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

static bool shaderfx_ui_poll(const bContext *C, PanelType * /*pt*/)
{
  Object *ob = ED_object_active_context(C);

  return (ob != nullptr) && (ob->type == OB_GPENCIL_LEGACY);
}

PanelType *shaderfx_panel_register(ARegionType *region_type, ShaderFxType type, PanelDrawFn draw)
{
  PanelType *panel_type = static_cast<PanelType *>(MEM_callocN(sizeof(PanelType), __func__));

  BKE_shaderfxType_panel_id(type, panel_type->idname);
  STRNCPY(panel_type->label, "");
  STRNCPY(panel_type->context, "shaderfx");
  STRNCPY(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);

  panel_type->draw_header = shaderfx_panel_header;
  panel_type->draw = draw;
  panel_type->poll = shaderfx_ui_poll;

  /* Give the panel the special flag that says it was built here and corresponds to a
   * shader effect rather than a PanelType. */
  panel_type->flag = PANEL_TYPE_HEADER_EXPAND | PANEL_TYPE_INSTANCED;
  panel_type->reorder = shaderfx_reorder;
  panel_type->get_list_data_expand_flag = get_shaderfx_expand_flag;
  panel_type->set_list_data_expand_flag = set_shaderfx_expand_flag;

  BLI_addtail(&region_type->paneltypes, panel_type);

  return panel_type;
}

PanelType *shaderfx_subpanel_register(ARegionType *region_type,
                                      const char *name,
                                      const char *label,
                                      PanelDrawFn draw_header,
                                      PanelDrawFn draw,
                                      PanelType *parent)
{
  PanelType *panel_type = static_cast<PanelType *>(MEM_callocN(sizeof(PanelType), __func__));

  SNPRINTF(panel_type->idname, "%s_%s", parent->idname, name);
  STRNCPY(panel_type->label, label);
  STRNCPY(panel_type->context, "shaderfx");
  STRNCPY(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);

  panel_type->draw_header = draw_header;
  panel_type->draw = draw;
  panel_type->poll = shaderfx_ui_poll;
  panel_type->flag = PANEL_TYPE_DEFAULT_CLOSED;

  BLI_assert(parent != nullptr);
  STRNCPY(panel_type->parent_id, parent->idname);
  panel_type->parent = parent;
  BLI_addtail(&parent->children, BLI_genericNodeN(panel_type));
  BLI_addtail(&region_type->paneltypes, panel_type);

  return panel_type;
}

/** \} */
