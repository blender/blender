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
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"

#include "ED_object.hh"

#include "BLT_translation.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "MOD_ui_common.hh" /* Self include */

using blender::StringRefNull;

/**
 * Poll function so these modifier panels don't show for other object types with modifiers (only
 * grease pencil currently).
 */
static bool modifier_ui_poll(const bContext *C, PanelType * /*pt*/)
{
  Object *ob = blender::ed::object::context_active_object(C);
  return ob != nullptr;
}

/* -------------------------------------------------------------------- */
/** \name Panel Drag and Drop, Expansion Saving
 * \{ */

/**
 * Move a modifier to the index it's moved to after a drag and drop.
 */
static void modifier_reorder(bContext *C, Panel *panel, int new_index)
{
  PointerRNA *md_ptr = UI_panel_custom_data_get(panel);
  ModifierData *md = (ModifierData *)md_ptr->data;

  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_modifier_move_to_index", false);
  WM_operator_properties_create_ptr(&props_ptr, ot);
  RNA_string_set(&props_ptr, "modifier", md->name);
  RNA_int_set(&props_ptr, "index", new_index);
  WM_operator_name_call_ptr(C, ot, blender::wm::OpCallContext::InvokeDefault, &props_ptr, nullptr);
  WM_operator_properties_free(&props_ptr);
}

static short get_modifier_expand_flag(const bContext * /*C*/, Panel *panel)
{
  PointerRNA *md_ptr = UI_panel_custom_data_get(panel);
  ModifierData *md = (ModifierData *)md_ptr->data;
  return md->ui_expand_flag;
}

static void set_modifier_expand_flag(const bContext * /*C*/, Panel *panel, short expand_flag)
{
  PointerRNA *md_ptr = UI_panel_custom_data_get(panel);
  ModifierData *md = (ModifierData *)md_ptr->data;
  md->ui_expand_flag = expand_flag;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modifier Panel Layouts
 * \{ */

void modifier_error_message_draw(uiLayout *layout, PointerRNA *ptr)
{
  ModifierData *md = static_cast<ModifierData *>(ptr->data);
  if (md->error) {
    uiLayout *row = &layout->row(false);
    row->label(RPT_(md->error), ICON_ERROR);
  }
}

/**
 * Gets RNA pointers for the active object and the panel's modifier data. Also locks
 * the layout if the modifier is from a linked object, and sets the context pointer.
 *
 * \note The modifier #PointerRNA is owned by the panel so we only need a pointer to it.
 */
#define ERROR_LIBDATA_MESSAGE N_("External library data")
PointerRNA *modifier_panel_get_property_pointers(Panel *panel, PointerRNA *r_ob_ptr)
{
  PointerRNA *ptr = UI_panel_custom_data_get(panel);
  BLI_assert(!RNA_pointer_is_null(ptr));
  BLI_assert(RNA_struct_is_a(ptr->type, &RNA_Modifier));

  if (r_ob_ptr != nullptr) {
    *r_ob_ptr = RNA_pointer_create_discrete(ptr->owner_id, &RNA_Object, ptr->owner_id);
  }

  uiBlock *block = panel->layout->block();
  UI_block_lock_set(block, !ID_IS_EDITABLE((Object *)ptr->owner_id), ERROR_LIBDATA_MESSAGE);

  UI_panel_context_pointer_set(panel, "modifier", ptr);

  return ptr;
}

void modifier_vgroup_ui(uiLayout *layout,
                        PointerRNA *ptr,
                        PointerRNA *ob_ptr,
                        const StringRefNull vgroup_prop,
                        const std::optional<StringRefNull> invert_vgroup_prop,
                        const std::optional<StringRefNull> text)
{
  bool has_vertex_group = RNA_string_length(ptr, vgroup_prop.c_str()) != 0;

  uiLayout *row = &layout->row(true);
  row->prop_search(ptr, vgroup_prop, ob_ptr, "vertex_groups", text, ICON_GROUP_VERTEX);
  if (invert_vgroup_prop) {
    uiLayout *sub = &row->row(true);
    sub->active_set(has_vertex_group);
    sub->use_property_decorate_set(false);
    sub->prop(ptr, *invert_vgroup_prop, UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);
  }
}

void modifier_grease_pencil_curve_header_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->prop(ptr, "use_custom_curve", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

void modifier_grease_pencil_curve_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiTemplateCurveMapping(layout, ptr, "curve", 0, false, false, false, false, false);
}

/**
 * Check whether Modifier is a simulation or not. Used for switching to the
 * physics/particles context tab.
 */
static int modifier_is_simulation(const ModifierData *md)
{
  /* Physic Tab */
  if (ELEM(md->type,
           eModifierType_Cloth,
           eModifierType_Collision,
           eModifierType_Fluidsim,
           eModifierType_Fluid,
           eModifierType_Softbody,
           eModifierType_Surface,
           eModifierType_DynamicPaint))
  {
    return 1;
  }
  /* Particle Tab */
  if (md->type == eModifierType_ParticleSystem) {
    return 2;
  }

  return 0;
}

static bool modifier_can_delete(ModifierData *md)
{
  /* fluid particle modifier can't be deleted here */
  if (md->type == eModifierType_ParticleSystem) {
    short particle_type = ((ParticleSystemModifierData *)md)->psys->part->type;
    if (ELEM(particle_type,
             PART_FLUID,
             PART_FLUID_FLIP,
             PART_FLUID_FOAM,
             PART_FLUID_SPRAY,
             PART_FLUID_BUBBLE,
             PART_FLUID_TRACER,
             PART_FLUID_SPRAYFOAM,
             PART_FLUID_SPRAYBUBBLE,
             PART_FLUID_FOAMBUBBLE,
             PART_FLUID_SPRAYFOAMBUBBLE))
    {
      return false;
    }
  }
  return true;
}

static void modifier_ops_extra_draw(bContext *C, uiLayout *layout, void *md_v)
{
  PointerRNA op_ptr;
  ModifierData *md = (ModifierData *)md_v;

  Object *ob = blender::ed::object::context_active_object(C);
  PointerRNA ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Modifier, md);
  layout->context_ptr_set("modifier", &ptr);
  layout->operator_context_set(blender::wm::OpCallContext::InvokeDefault);

  layout->ui_units_x_set(4.0f);

  /* Apply. */
  if (ob->type == OB_GREASE_PENCIL) {
    layout->op("OBJECT_OT_modifier_apply",
               CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Apply (Active Keyframe)"),
               ICON_CHECKMARK);

    op_ptr = layout->op("OBJECT_OT_modifier_apply",
                        IFACE_("Apply (All Keyframes)"),
                        ICON_KEYFRAME,
                        blender::wm::OpCallContext::InvokeDefault,
                        UI_ITEM_NONE);
    RNA_boolean_set(&op_ptr, "all_keyframes", true);
  }
  else {
    layout->op("OBJECT_OT_modifier_apply",
               CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Apply"),
               ICON_CHECKMARK);
  }

  /* Apply as shapekey. */
  if (BKE_modifier_is_same_topology(md) && !BKE_modifier_is_non_geometrical(md)) {
    PointerRNA op_ptr = layout->op(
        "OBJECT_OT_modifier_apply_as_shapekey",
        CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Apply as Shape Key"),
        ICON_SHAPEKEY_DATA);
    RNA_boolean_set(&op_ptr, "keep_modifier", false);

    op_ptr = layout->op("OBJECT_OT_modifier_apply_as_shapekey",
                        CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Save as Shape Key"),
                        ICON_NONE);
    RNA_boolean_set(&op_ptr, "keep_modifier", true);
    layout->separator();
  }

  /* Duplicate. */
  if (!ELEM(md->type,
            eModifierType_Fluidsim,
            eModifierType_Softbody,
            eModifierType_ParticleSystem,
            eModifierType_Cloth,
            eModifierType_Fluid))
  {
    layout->op("OBJECT_OT_modifier_copy",
               CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Duplicate"),
               ICON_DUPLICATE);
  }

  layout->op("OBJECT_OT_modifier_copy_to_selected",
             CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy to Selected"),
             0);

  layout->separator();

  /* Move to first. */
  op_ptr = layout->op("OBJECT_OT_modifier_move_to_index",
                      IFACE_("Move to First"),
                      ICON_TRIA_UP,
                      blender::wm::OpCallContext::InvokeDefault,
                      UI_ITEM_NONE);
  RNA_int_set(&op_ptr, "index", 0);

  /* Move to last. */
  op_ptr = layout->op("OBJECT_OT_modifier_move_to_index",
                      IFACE_("Move to Last"),
                      ICON_TRIA_DOWN,
                      blender::wm::OpCallContext::InvokeDefault,
                      UI_ITEM_NONE);
  RNA_int_set(&op_ptr, "index", BLI_listbase_count(&ob->modifiers) - 1);

  layout->separator();

  layout->prop(&ptr, "use_pin_to_last", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (md->type == eModifierType_Nodes) {
    layout->separator();
    op_ptr = layout->op("OBJECT_OT_geometry_nodes_move_to_nodes",
                        std::nullopt,
                        ICON_NONE,
                        blender::wm::OpCallContext::InvokeDefault,
                        UI_ITEM_NONE);
    layout->prop(&ptr, "show_group_selector", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(&ptr, "show_manage_panel", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

static void modifier_panel_header(const bContext *C, Panel *panel)
{
  uiLayout *row, *sub, *name_row;
  uiLayout *layout = panel->layout;

  /* Don't use #modifier_panel_get_property_pointers, we don't want to lock the header. */
  PointerRNA *ptr = UI_panel_custom_data_get(panel);
  ModifierData *md = (ModifierData *)ptr->data;
  Object *ob = (Object *)ptr->owner_id;

  UI_panel_context_pointer_set(panel, "modifier", ptr);

  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
  Scene *scene = CTX_data_scene(C);
  int index = BLI_findindex(&ob->modifiers, md);

  /* Modifier Icon. */
  sub = &layout->row(true);
  sub->emboss_set(blender::ui::EmbossType::None);
  if (mti->is_disabled && mti->is_disabled(scene, md, false)) {
    sub->red_alert_set(true);
  }
  PointerRNA op_ptr = sub->op("OBJECT_OT_modifier_set_active", "", RNA_struct_ui_icon(ptr->type));
  RNA_string_set(&op_ptr, "modifier", md->name);

  row = &layout->row(true);

  /* Modifier Name.
   * Count how many buttons are added to the header to check if there is enough space. */
  int buttons_number = 0;
  name_row = &row->row(true);

  /* Display mode switching buttons. */
  if (ob->type == OB_MESH) {
    int last_cage_index;
    int cage_index = BKE_modifiers_get_cage_index(scene, ob, &last_cage_index, false);
    if (BKE_modifier_supports_cage(scene, md) && (index <= last_cage_index)) {
      sub = &row->row(true);
      if (index < cage_index || !BKE_modifier_couldbe_cage(scene, md)) {
        sub->active_set(false);
      }
      sub->prop(ptr, "show_on_cage", UI_ITEM_NONE, "", ICON_NONE);
      buttons_number++;
    }
  } /* Tessellation point for curve-typed objects. */
  else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF, OB_FONT)) {
    /* Smooth modifier can work with tessellated curves only (works on mesh edges explicitly). */
    if (md->type == eModifierType_Smooth) {
      /* Add button (appearing to be OFF) and add tip why this can't be changed. */
      sub = &row->row(true);
      uiBlock *block = sub->block();
      static int apply_on_spline_always_off_hack = 0;
      uiBut *but = uiDefIconButBitI(block,
                                    ButType::Toggle,
                                    eModifierMode_ApplyOnSpline,
                                    0,
                                    ICON_SURFACE_DATA,
                                    0,
                                    0,
                                    UI_UNIT_X - 2,
                                    UI_UNIT_Y,
                                    &apply_on_spline_always_off_hack,
                                    0.0,
                                    0.0,
                                    RPT_("Apply on Spline"));
      UI_but_disable(but,
                     "This modifier can only deform filled curve/surface, not the control points");
      buttons_number++;
    }
    /* Some modifiers can work with pre-tessellated curves only. */
    else if (ELEM(md->type, eModifierType_Hook, eModifierType_Softbody, eModifierType_MeshDeform))
    {
      /* Add button (appearing to be ON) and add tip why this can't be changed. */
      sub = &row->row(true);
      uiBlock *block = sub->block();
      static int apply_on_spline_always_on_hack = eModifierMode_ApplyOnSpline;
      uiBut *but = uiDefIconButBitI(block,
                                    ButType::Toggle,
                                    eModifierMode_ApplyOnSpline,
                                    0,
                                    ICON_SURFACE_DATA,
                                    0,
                                    0,
                                    UI_UNIT_X - 2,
                                    UI_UNIT_Y,
                                    &apply_on_spline_always_on_hack,
                                    0.0,
                                    0.0,
                                    RPT_("Apply on Spline"));
      UI_but_disable(but,
                     "This modifier can only deform control points, not the filled curve/surface");
      buttons_number++;
    }
    else if (mti->type != ModifierTypeType::Constructive) {
      /* Constructive modifiers tessellates curve before applying. */
      row->prop(ptr, "use_apply_on_spline", UI_ITEM_NONE, "", ICON_NONE);
      buttons_number++;
    }
  }
  /* Collision and Surface are always enabled, hide buttons. */
  if (!ELEM(md->type, eModifierType_Collision, eModifierType_Surface)) {
    if (mti->flags & eModifierTypeFlag_SupportsEditmode) {
      sub = &row->row(true);
      sub->active_set(md->mode & eModifierMode_Realtime);
      sub->prop(ptr, "show_in_editmode", UI_ITEM_NONE, "", ICON_NONE);
      buttons_number++;
    }
    row->prop(ptr, "show_viewport", UI_ITEM_NONE, "", ICON_NONE);
    row->prop(ptr, "show_render", UI_ITEM_NONE, "", ICON_NONE);
    buttons_number += 2;
  }

  /* Extra operators menu. */
  row->menu_fn("", ICON_DOWNARROW_HLT, modifier_ops_extra_draw, md);

  /* Delete button. */
  if (modifier_can_delete(md) && !modifier_is_simulation(md)) {
    sub = &row->row(false);
    sub->emboss_set(blender::ui::EmbossType::None);
    sub->op("OBJECT_OT_modifier_remove", "", ICON_X);
    buttons_number++;
  }

  /* Switch context buttons. */
  if (modifier_is_simulation(md) == 1) {
    PointerRNA op_ptr = row->op("WM_OT_properties_context_change", "", ICON_PROPERTIES);
    if (!RNA_pointer_is_null(&op_ptr)) {
      RNA_string_set(&op_ptr, "context", "PHYSICS");
    }
    buttons_number++;
  }
  else if (modifier_is_simulation(md) == 2) {
    PointerRNA op_ptr = row->op("WM_OT_properties_context_change", "", ICON_PROPERTIES);
    if (!RNA_pointer_is_null(&op_ptr)) {
      RNA_string_set(&op_ptr, "context", "PARTICLES");
    }
    buttons_number++;
  }

  bool display_name = (panel->sizex / UI_UNIT_X - buttons_number > 5) || (panel->sizex == 0);
  if (display_name) {
    name_row->prop(ptr, "name", UI_ITEM_NONE, "", ICON_NONE);
  }
  else {
    row->alignment_set(blender::ui::LayoutAlign::Right);
  }

  /* Extra padding for delete button. */
  layout->separator();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modifier Registration Helpers
 * \{ */

PanelType *modifier_panel_register(ARegionType *region_type, ModifierType type, PanelDrawFn draw)
{
  PanelType *panel_type = MEM_callocN<PanelType>(__func__);

  BKE_modifier_type_panel_id(type, panel_type->idname);
  STRNCPY_UTF8(panel_type->label, "");
  STRNCPY_UTF8(panel_type->context, "modifier");
  STRNCPY_UTF8(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  STRNCPY_UTF8(panel_type->active_property, "is_active");
  STRNCPY_UTF8(panel_type->pin_to_last_property, "use_pin_to_last");

  panel_type->draw_header = modifier_panel_header;
  panel_type->draw = draw;
  panel_type->poll = modifier_ui_poll;

  /* Give the panel the special flag that says it was built here and corresponds to a
   * modifier rather than a #PanelType. */
  panel_type->flag = PANEL_TYPE_HEADER_EXPAND | PANEL_TYPE_INSTANCED;
  panel_type->reorder = modifier_reorder;
  panel_type->get_list_data_expand_flag = get_modifier_expand_flag;
  panel_type->set_list_data_expand_flag = set_modifier_expand_flag;

  BLI_addtail(&region_type->paneltypes, panel_type);

  return panel_type;
}

PanelType *modifier_subpanel_register(ARegionType *region_type,
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
  STRNCPY_UTF8(panel_type->context, "modifier");
  STRNCPY_UTF8(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  STRNCPY_UTF8(panel_type->active_property, "is_active");

  panel_type->draw_header = draw_header;
  panel_type->draw = draw;
  panel_type->poll = modifier_ui_poll;
  panel_type->flag = PANEL_TYPE_DEFAULT_CLOSED;

  STRNCPY_UTF8(panel_type->parent_id, parent->idname);
  panel_type->parent = parent;
  BLI_addtail(&parent->children, BLI_genericNodeN(panel_type));
  BLI_addtail(&region_type->paneltypes, panel_type);

  return panel_type;
}

/** \} */
