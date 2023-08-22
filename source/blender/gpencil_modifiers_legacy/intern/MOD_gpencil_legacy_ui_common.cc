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
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_material.h"
#include "BKE_screen.h"

#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "ED_object.hh"

#include "BLT_translation.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "MOD_gpencil_legacy_ui_common.h" /* Self include */

/**
 * Poll function so these modifier panels only show for grease pencil objects.
 */
static bool gpencil_modifier_ui_poll(const bContext *C, PanelType * /*pt*/)
{
  Object *ob = ED_object_active_context(C);

  return (ob != nullptr) && (ob->type == OB_GPENCIL_LEGACY);
}

/* -------------------------------------------------------------------- */
/** \name Panel Drag and Drop, Expansion Saving
 * \{ */

/**
 * Move a modifier to the index it's moved to after a drag and drop.
 */
static void gpencil_modifier_reorder(bContext *C, Panel *panel, int new_index)
{
  PointerRNA *md_ptr = UI_panel_custom_data_get(panel);
  GpencilModifierData *md = (GpencilModifierData *)md_ptr->data;

  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_gpencil_modifier_move_to_index", false);
  WM_operator_properties_create_ptr(&props_ptr, ot);
  RNA_string_set(&props_ptr, "modifier", md->name);
  RNA_int_set(&props_ptr, "index", new_index);
  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &props_ptr, nullptr);
  WM_operator_properties_free(&props_ptr);
}

static short get_gpencil_modifier_expand_flag(const bContext * /*C*/, Panel *panel)
{
  PointerRNA *md_ptr = UI_panel_custom_data_get(panel);
  GpencilModifierData *md = (GpencilModifierData *)md_ptr->data;
  return md->ui_expand_flag;
}

static void set_gpencil_modifier_expand_flag(const bContext * /*C*/,
                                             Panel *panel,
                                             short expand_flag)
{
  PointerRNA *md_ptr = UI_panel_custom_data_get(panel);
  GpencilModifierData *md = (GpencilModifierData *)md_ptr->data;
  md->ui_expand_flag = expand_flag;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modifier Panel Layouts
 * \{ */

void gpencil_modifier_masking_panel_draw(Panel *panel, bool use_material, bool use_vertex)
{
  uiLayout *row, *col, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");
  bool has_layer = RNA_string_length(ptr, "layer") != 0;

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, true);
  row = uiLayoutRow(col, true);
  uiItemPointerR(row, ptr, "layer", &obj_data_ptr, "layers", nullptr, ICON_GREASEPENCIL);
  sub = uiLayoutRow(row, true);
  uiLayoutSetActive(sub, has_layer);
  uiLayoutSetPropDecorate(sub, false);
  uiItemR(sub, ptr, "invert_layers", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);

  row = uiLayoutRow(col, true);
  uiItemR(row, ptr, "layer_pass", UI_ITEM_NONE, nullptr, ICON_NONE);
  sub = uiLayoutRow(row, true);
  uiLayoutSetActive(sub, RNA_int_get(ptr, "layer_pass") != 0);
  uiLayoutSetPropDecorate(sub, false);
  uiItemR(sub, ptr, "invert_layer_pass", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);

  if (use_material) {
    PointerRNA material_ptr = RNA_pointer_get(ptr, "material");
    bool has_material = !RNA_pointer_is_null(&material_ptr);

    /* Because the Gpencil modifier material property used to be a string in an earlier version of
     * Blender, we need to check if the material is valid and display it differently if so. */
    bool valid = false;
    {
      if (!has_material) {
        valid = true;
      }
      else {
        Material *current_material = static_cast<Material *>(material_ptr.data);
        Object *ob = static_cast<Object *>(ob_ptr.data);
        for (int i = 0; i <= ob->totcol; i++) {
          Material *mat = BKE_object_material_get(ob, i);
          if (mat == current_material) {
            valid = true;
            break;
          }
        }
      }
    }

    col = uiLayoutColumn(layout, true);
    row = uiLayoutRow(col, true);
    uiLayoutSetRedAlert(row, !valid);
    uiItemPointerR(row,
                   ptr,
                   "material",
                   &obj_data_ptr,
                   "materials",
                   nullptr,
                   valid ? ICON_SHADING_TEXTURE : ICON_ERROR);
    sub = uiLayoutRow(row, true);
    uiLayoutSetActive(sub, has_material);
    uiLayoutSetPropDecorate(sub, false);
    uiItemR(sub, ptr, "invert_materials", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);

    row = uiLayoutRow(col, true);
    uiItemR(row, ptr, "pass_index", UI_ITEM_NONE, nullptr, ICON_NONE);
    sub = uiLayoutRow(row, true);
    uiLayoutSetActive(sub, RNA_int_get(ptr, "pass_index") != 0);
    uiLayoutSetPropDecorate(sub, false);
    uiItemR(sub, ptr, "invert_material_pass", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);
  }

  if (use_vertex) {
    bool has_vertex_group = RNA_string_length(ptr, "vertex_group") != 0;

    row = uiLayoutRow(layout, true);
    uiItemPointerR(row, ptr, "vertex_group", &ob_ptr, "vertex_groups", nullptr, ICON_NONE);
    sub = uiLayoutRow(row, true);
    uiLayoutSetActive(sub, has_vertex_group);
    uiLayoutSetPropDecorate(sub, false);
    uiItemR(sub, ptr, "invert_vertex", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);
  }
}

void gpencil_modifier_curve_header_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiItemR(layout, ptr, "use_custom_curve", UI_ITEM_NONE, nullptr, ICON_NONE);
}

void gpencil_modifier_curve_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiTemplateCurveMapping(layout, ptr, "curve", 0, false, false, false, false);
}

void gpencil_modifier_panel_end(uiLayout *layout, PointerRNA *ptr)
{
  GpencilModifierData *md = static_cast<GpencilModifierData *>(ptr->data);
  if (md->error) {
    uiLayout *row = uiLayoutRow(layout, false);
    uiItemL(row, IFACE_(md->error), ICON_ERROR);
  }
}

/**
 * Gets RNA pointers for the active object and the panel's modifier data.
 */
#define ERROR_LIBDATA_MESSAGE TIP_("External library data")
PointerRNA *gpencil_modifier_panel_get_property_pointers(Panel *panel, PointerRNA *r_ob_ptr)
{
  PointerRNA *ptr = UI_panel_custom_data_get(panel);
  BLI_assert(RNA_struct_is_a(ptr->type, &RNA_GpencilModifier));

  if (r_ob_ptr != nullptr) {
    RNA_pointer_create(ptr->owner_id, &RNA_Object, ptr->owner_id, r_ob_ptr);
  }

  uiBlock *block = uiLayoutGetBlock(panel->layout);
  UI_block_lock_clear(block);
  UI_block_lock_set(block, ID_IS_LINKED((Object *)ptr->owner_id), ERROR_LIBDATA_MESSAGE);

  UI_panel_context_pointer_set(panel, "modifier", ptr);

  return ptr;
}

static void gpencil_modifier_ops_extra_draw(bContext *C, uiLayout *layout, void *md_v)
{
  PointerRNA op_ptr;
  uiLayout *row;
  GpencilModifierData *md = (GpencilModifierData *)md_v;
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(
      GpencilModifierType(md->type));

  PointerRNA ptr;
  Object *ob = ED_object_active_context(C);
  RNA_pointer_create(&ob->id, &RNA_GpencilModifier, md, &ptr);
  uiLayoutSetContextPointer(layout, "modifier", &ptr);
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  uiLayoutSetUnitsX(layout, 4.0f);

  UI_block_flag_enable(uiLayoutGetBlock(layout), UI_BLOCK_IS_FLIP);

  /* Apply. */
  if (!(mti->flags & eGpencilModifierTypeFlag_NoApply)) {
    uiItemO(layout,
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Apply"),
            ICON_CHECKMARK,
            "OBJECT_OT_gpencil_modifier_apply");
  }

  /* Duplicate. */
  uiItemO(layout,
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Duplicate"),
          ICON_DUPLICATE,
          "OBJECT_OT_gpencil_modifier_copy");

  uiItemO(layout,
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy to Selected"),
          0,
          "OBJECT_OT_gpencil_modifier_copy_to_selected");

  uiItemS(layout);

  /* Move to first. */
  row = uiLayoutColumn(layout, false);
  uiItemFullO(row,
              "OBJECT_OT_gpencil_modifier_move_to_index",
              IFACE_("Move to First"),
              ICON_TRIA_UP,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              UI_ITEM_NONE,
              &op_ptr);
  RNA_int_set(&op_ptr, "index", 0);
  if (!md->prev) {
    uiLayoutSetEnabled(row, false);
  }

  /* Move to last. */
  row = uiLayoutColumn(layout, false);
  uiItemFullO(row,
              "OBJECT_OT_gpencil_modifier_move_to_index",
              IFACE_("Move to Last"),
              ICON_TRIA_DOWN,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              UI_ITEM_NONE,
              &op_ptr);
  RNA_int_set(&op_ptr, "index", BLI_listbase_count(&ob->greasepencil_modifiers) - 1);
  if (!md->next) {
    uiLayoutSetEnabled(row, false);
  }
}

static void gpencil_modifier_panel_header(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = UI_panel_custom_data_get(panel);
  GpencilModifierData *md = (GpencilModifierData *)ptr->data;

  UI_panel_context_pointer_set(panel, "modifier", ptr);

  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(
      GpencilModifierType(md->type));
  bool narrow_panel = (panel->sizex < UI_UNIT_X * 9 && panel->sizex != 0);

  /* Modifier Icon. */
  row = uiLayoutRow(layout, false);
  if (mti->is_disabled && mti->is_disabled(md, false)) {
    uiLayoutSetRedAlert(row, true);
  }
  uiItemL(row, "", RNA_struct_ui_icon(ptr->type));

  /* Modifier name. */
  row = uiLayoutRow(layout, true);
  if (!narrow_panel) {
    uiItemR(row, ptr, "name", UI_ITEM_NONE, "", ICON_NONE);
  }
  else {
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);
  }

  /* Display mode buttons. */
  if (mti->flags & eGpencilModifierTypeFlag_SupportsEditmode) {
    sub = uiLayoutRow(row, true);
    uiItemR(sub, ptr, "show_in_editmode", UI_ITEM_NONE, "", ICON_NONE);
  }
  uiItemR(row, ptr, "show_viewport", UI_ITEM_NONE, "", ICON_NONE);
  uiItemR(row, ptr, "show_render", UI_ITEM_NONE, "", ICON_NONE);

  /* Extra operators. */
  // row = uiLayoutRow(layout, true);
  uiItemMenuF(row, "", ICON_DOWNARROW_HLT, gpencil_modifier_ops_extra_draw, md);

  /* Remove button. */
  sub = uiLayoutRow(row, false);
  uiLayoutSetEmboss(sub, UI_EMBOSS_NONE);
  uiItemO(sub, "", ICON_X, "OBJECT_OT_gpencil_modifier_remove");

  /* Extra padding. */
  uiItemS(layout);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modifier Registration Helpers
 * \{ */

PanelType *gpencil_modifier_panel_register(ARegionType *region_type,
                                           GpencilModifierType type,
                                           PanelDrawFn draw)
{
  PanelType *panel_type = static_cast<PanelType *>(MEM_callocN(sizeof(PanelType), __func__));

  BKE_gpencil_modifierType_panel_id(type, panel_type->idname);
  STRNCPY(panel_type->label, "");
  STRNCPY(panel_type->context, "modifier");
  STRNCPY(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);

  panel_type->draw_header = gpencil_modifier_panel_header;
  panel_type->draw = draw;
  panel_type->poll = gpencil_modifier_ui_poll;

  /* Give the panel the special flag that says it was built here and corresponds to a
   * modifier rather than a #PanelType. */
  panel_type->flag = PANEL_TYPE_HEADER_EXPAND | PANEL_TYPE_INSTANCED;
  panel_type->reorder = gpencil_modifier_reorder;
  panel_type->get_list_data_expand_flag = get_gpencil_modifier_expand_flag;
  panel_type->set_list_data_expand_flag = set_gpencil_modifier_expand_flag;

  BLI_addtail(&region_type->paneltypes, panel_type);

  return panel_type;
}

PanelType *gpencil_modifier_subpanel_register(ARegionType *region_type,
                                              const char *name,
                                              const char *label,
                                              PanelDrawFn draw_header,
                                              PanelDrawFn draw,
                                              PanelType *parent)
{
  PanelType *panel_type = static_cast<PanelType *>(MEM_callocN(sizeof(PanelType), __func__));

  SNPRINTF(panel_type->idname, "%s_%s", parent->idname, name);
  STRNCPY(panel_type->label, label);
  STRNCPY(panel_type->context, "modifier");
  STRNCPY(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);

  panel_type->draw_header = draw_header;
  panel_type->draw = draw;
  panel_type->poll = gpencil_modifier_ui_poll;
  panel_type->flag = PANEL_TYPE_DEFAULT_CLOSED;

  BLI_assert(parent != nullptr);
  STRNCPY(panel_type->parent_id, parent->idname);
  panel_type->parent = parent;
  BLI_addtail(&parent->children, BLI_genericNodeN(panel_type));
  BLI_addtail(&region_type->paneltypes, panel_type);

  return panel_type;
}

/** \} */
