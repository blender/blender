/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "MEM_guardedalloc.h"

#include "BKE_context.hh"
#include "BKE_library.hh"
#include "BKE_screen.hh"
#include "BKE_shader_fx.h"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_shader_fx_types.h"

#include "ED_object.hh"

#include "BLT_translation.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

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
  WM_operator_name_call_ptr(C, ot, blender::wm::OpCallContext::InvokeDefault, &props_ptr, nullptr);
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
    uiLayout *row = &layout->row(false);
    row->label(RPT_(fx->error), ICON_ERROR);
  }
}

PointerRNA *shaderfx_panel_get_property_pointers(Panel *panel, PointerRNA *r_ob_ptr)
{
  PointerRNA *ptr = UI_panel_custom_data_get(panel);
  BLI_assert(RNA_struct_is_a(ptr->type, &RNA_ShaderFx));

  if (r_ob_ptr != nullptr) {
    *r_ob_ptr = RNA_pointer_create_discrete(ptr->owner_id, &RNA_Object, ptr->owner_id);
  }

  UI_panel_context_pointer_set(panel, "shaderfx", ptr);

  return ptr;
}

#define ERROR_LIBDATA_MESSAGE N_("External library data")

static void gpencil_shaderfx_ops_extra_draw(bContext *C, uiLayout *layout, void *fx_v)
{
  PointerRNA op_ptr;
  uiLayout *row;
  ShaderFxData *fx = (ShaderFxData *)fx_v;

  Object *ob = blender::ed::object::context_active_object(C);
  PointerRNA ptr = RNA_pointer_create_discrete(&ob->id, &RNA_ShaderFx, fx);
  layout->context_ptr_set("shaderfx", &ptr);
  layout->operator_context_set(blender::wm::OpCallContext::InvokeDefault);

  layout->ui_units_x_set(4.0f);

  /* Duplicate. */
  layout->op("OBJECT_OT_shaderfx_copy",
             CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Duplicate"),
             ICON_DUPLICATE);

  layout->separator();

  /* Move to first. */
  row = &layout->column(false);
  op_ptr = row->op("OBJECT_OT_shaderfx_move_to_index",
                   IFACE_("Move to First"),
                   ICON_TRIA_UP,
                   blender::wm::OpCallContext::InvokeDefault,
                   UI_ITEM_NONE);
  RNA_int_set(&op_ptr, "index", 0);
  if (!fx->prev) {
    row->enabled_set(false);
  }

  /* Move to last. */
  row = &layout->column(false);
  op_ptr = row->op("OBJECT_OT_shaderfx_move_to_index",
                   IFACE_("Move to Last"),
                   ICON_TRIA_DOWN,
                   blender::wm::OpCallContext::InvokeDefault,
                   UI_ITEM_NONE);
  RNA_int_set(&op_ptr, "index", BLI_listbase_count(&ob->shader_fx) - 1);
  if (!fx->next) {
    row->enabled_set(false);
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

  UI_block_lock_set(layout->block(), (ob && !ID_IS_EDITABLE(ob)), ERROR_LIBDATA_MESSAGE);

  /* Effect type icon. */
  uiLayout *row = &layout->row(false);
  if (fxti->is_disabled && fxti->is_disabled(fx, false)) {
    row->red_alert_set(true);
  }
  row->label("", RNA_struct_ui_icon(ptr->type));

  /* Effect name. */
  row = &layout->row(true);
  if (!narrow_panel) {
    row->prop(ptr, "name", UI_ITEM_NONE, "", ICON_NONE);
  }

  /* Mode enabling buttons. */
  if (fxti->flags & eShaderFxTypeFlag_SupportsEditmode) {
    uiLayout *sub = &row->row(true);
    sub->active_set(false);
    sub->prop(ptr, "show_in_editmode", UI_ITEM_NONE, "", ICON_NONE);
  }
  row->prop(ptr, "show_viewport", UI_ITEM_NONE, "", ICON_NONE);
  row->prop(ptr, "show_render", UI_ITEM_NONE, "", ICON_NONE);

  /* Extra operators. */
  row->menu_fn("", ICON_DOWNARROW_HLT, gpencil_shaderfx_ops_extra_draw, fx);

  row = &row->row(false);
  row->emboss_set(blender::ui::EmbossType::None);
  row->op("OBJECT_OT_shaderfx_remove", "", ICON_X);

  /* Some padding so the X isn't too close to the drag icon. */
  layout->separator();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ShaderFx Registration Helpers
 * \{ */

static bool shaderfx_ui_poll(const bContext *C, PanelType * /*pt*/)
{
  Object *ob = blender::ed::object::context_active_object(C);

  return (ob != nullptr) && ob->type == OB_GREASE_PENCIL;
}

PanelType *shaderfx_panel_register(ARegionType *region_type, ShaderFxType type, PanelDrawFn draw)
{
  PanelType *panel_type = MEM_callocN<PanelType>(__func__);

  BKE_shaderfxType_panel_id(type, panel_type->idname);
  STRNCPY_UTF8(panel_type->label, "");
  STRNCPY_UTF8(panel_type->context, "shaderfx");
  STRNCPY_UTF8(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);

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
  PanelType *panel_type = MEM_callocN<PanelType>(__func__);

  BLI_assert(parent != nullptr);
  SNPRINTF_UTF8(panel_type->idname, "%s_%s", parent->idname, name);
  STRNCPY_UTF8(panel_type->label, label);
  STRNCPY_UTF8(panel_type->context, "shaderfx");
  STRNCPY_UTF8(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);

  panel_type->draw_header = draw_header;
  panel_type->draw = draw;
  panel_type->poll = shaderfx_ui_poll;
  panel_type->flag = PANEL_TYPE_DEFAULT_CLOSED;

  STRNCPY_UTF8(panel_type->parent_id, parent->idname);
  panel_type->parent = parent;
  BLI_addtail(&parent->children, BLI_genericNodeN(panel_type));
  BLI_addtail(&region_type->paneltypes, panel_type);

  return panel_type;
}

/** \} */
