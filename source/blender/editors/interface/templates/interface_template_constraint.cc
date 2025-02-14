/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Template for building the panel layout for the active object or bone's constraints.
 */

#include "BKE_constraint.h"
#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLI_string_utils.hh"

#include "BLT_translation.hh"

#include "DNA_constraint_types.h"

#include "ED_object.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"

#include "UI_interface.hh"
#include "interface_intern.hh"
#include "interface_templates_intern.hh"

static void constraint_active_func(bContext * /*C*/, void *ob_v, void *con_v)
{
  blender::ed::object::constraint_active_set(static_cast<Object *>(ob_v),
                                             static_cast<bConstraint *>(con_v));
}

static void constraint_ops_extra_draw(bContext *C, uiLayout *layout, void *con_v)
{
  PointerRNA op_ptr;
  uiLayout *row;
  bConstraint *con = (bConstraint *)con_v;

  Object *ob = blender::ed::object::context_active_object(C);

  PointerRNA ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Constraint, con);
  uiLayoutSetContextPointer(layout, "constraint", &ptr);
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  uiLayoutSetUnitsX(layout, 4.0f);

  /* Apply. */
  uiItemO(layout,
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Apply"),
          ICON_CHECKMARK,
          "CONSTRAINT_OT_apply");

  /* Duplicate. */
  uiItemO(layout,
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Duplicate"),
          ICON_DUPLICATE,
          "CONSTRAINT_OT_copy");

  uiItemO(layout,
          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Copy to Selected"),
          0,
          "CONSTRAINT_OT_copy_to_selected");

  uiItemS(layout);

  /* Move to first. */
  row = uiLayoutColumn(layout, false);
  uiItemFullO(row,
              "CONSTRAINT_OT_move_to_index",
              IFACE_("Move to First"),
              ICON_TRIA_UP,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              UI_ITEM_NONE,
              &op_ptr);
  RNA_int_set(&op_ptr, "index", 0);
  if (!con->prev) {
    uiLayoutSetEnabled(row, false);
  }

  /* Move to last. */
  row = uiLayoutColumn(layout, false);
  uiItemFullO(row,
              "CONSTRAINT_OT_move_to_index",
              IFACE_("Move to Last"),
              ICON_TRIA_DOWN,
              nullptr,
              WM_OP_INVOKE_DEFAULT,
              UI_ITEM_NONE,
              &op_ptr);
  ListBase *constraint_list = blender::ed::object::constraint_list_from_constraint(
      ob, con, nullptr);
  RNA_int_set(&op_ptr, "index", BLI_listbase_count(constraint_list) - 1);
  if (!con->next) {
    uiLayoutSetEnabled(row, false);
  }
}

/* -------------------------------------------------------------------- */
/** \name Constraint Header Template
 * \{ */

static void draw_constraint_header(uiLayout *layout, Object *ob, bConstraint *con)
{
  /* unless button has its own callback, it adds this callback to button */
  uiBlock *block = uiLayoutGetBlock(layout);
  UI_block_func_set(block, constraint_active_func, ob, con);

  PointerRNA ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Constraint, con);

  if (block->panel) {
    UI_panel_context_pointer_set(block->panel, "constraint", &ptr);
  }
  else {
    uiLayoutSetContextPointer(layout, "constraint", &ptr);
  }

  /* Constraint type icon. */
  uiLayout *sub = uiLayoutRow(layout, false);
  uiLayoutSetEmboss(sub, UI_EMBOSS);
  uiLayoutSetRedAlert(sub, (con->flag & CONSTRAINT_DISABLE));
  uiItemL(sub, "", RNA_struct_ui_icon(ptr.type));

  UI_block_emboss_set(block, UI_EMBOSS);

  uiLayout *row = uiLayoutRow(layout, true);

  uiItemR(row, &ptr, "name", UI_ITEM_NONE, "", ICON_NONE);

  /* Enabled eye icon. */
  uiItemR(row, &ptr, "enabled", UI_ITEM_NONE, "", ICON_NONE);

  /* Extra operators menu. */
  uiItemMenuF(row, "", ICON_DOWNARROW_HLT, constraint_ops_extra_draw, con);

  /* Close 'button' - emboss calls here disable drawing of 'button' behind X */
  sub = uiLayoutRow(row, false);
  uiLayoutSetEmboss(sub, UI_EMBOSS_NONE);
  uiLayoutSetOperatorContext(sub, WM_OP_INVOKE_DEFAULT);
  uiItemO(sub, "", ICON_X, "CONSTRAINT_OT_delete");

  /* Some extra padding at the end, so the 'x' icon isn't too close to drag button. */
  uiItemS(layout);

  /* clear any locks set up for proxies/lib-linking */
  UI_block_lock_clear(block);
}

void uiTemplateConstraintHeader(uiLayout *layout, PointerRNA *ptr)
{
  /* verify we have valid data */
  if (!RNA_struct_is_a(ptr->type, &RNA_Constraint)) {
    RNA_warning("Expected constraint on object");
    return;
  }

  Object *ob = (Object *)ptr->owner_id;
  bConstraint *con = static_cast<bConstraint *>(ptr->data);

  if (!ob || !(GS(ob->id.name) == ID_OB)) {
    RNA_warning("Expected constraint on object");
    return;
  }

  UI_block_lock_set(uiLayoutGetBlock(layout), (ob && !ID_IS_EDITABLE(ob)), ERROR_LIBDATA_MESSAGE);

  draw_constraint_header(layout, ob, con);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Constraints Template
 *
 * Template for building the panel layout for the active object or bone's constraints.
 * \{ */

/** For building the panel UI for constraints. */
#define CONSTRAINT_TYPE_PANEL_PREFIX "OBJECT_PT_"
#define CONSTRAINT_BONE_TYPE_PANEL_PREFIX "BONE_PT_"

/**
 * Check if the panel's ID starts with 'BONE', meaning it is a bone constraint.
 */
static bool constraint_panel_is_bone(Panel *panel)
{
  return (panel->panelname[0] == 'B') && (panel->panelname[1] == 'O') &&
         (panel->panelname[2] == 'N') && (panel->panelname[3] == 'E');
}

/**
 * Move a constraint to the index it's moved to after a drag and drop.
 */
static void constraint_reorder(bContext *C, Panel *panel, int new_index)
{
  const bool constraint_from_bone = constraint_panel_is_bone(panel);

  PointerRNA *con_ptr = UI_panel_custom_data_get(panel);
  bConstraint *con = (bConstraint *)con_ptr->data;

  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find("CONSTRAINT_OT_move_to_index", false);
  WM_operator_properties_create_ptr(&props_ptr, ot);
  RNA_string_set(&props_ptr, "constraint", con->name);
  RNA_int_set(&props_ptr, "index", new_index);
  /* Set owner to #EDIT_CONSTRAINT_OWNER_OBJECT or #EDIT_CONSTRAINT_OWNER_BONE. */
  RNA_enum_set(&props_ptr, "owner", constraint_from_bone ? 1 : 0);
  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &props_ptr, nullptr);
  WM_operator_properties_free(&props_ptr);
}

/**
 * Get the expand flag from the active constraint to use for the panel.
 */
static short get_constraint_expand_flag(const bContext * /*C*/, Panel *panel)
{
  PointerRNA *con_ptr = UI_panel_custom_data_get(panel);
  bConstraint *con = (bConstraint *)con_ptr->data;

  return con->ui_expand_flag;
}

/**
 * Save the expand flag for the panel and sub-panels to the constraint.
 */
static void set_constraint_expand_flag(const bContext * /*C*/, Panel *panel, short expand_flag)
{
  PointerRNA *con_ptr = UI_panel_custom_data_get(panel);
  bConstraint *con = (bConstraint *)con_ptr->data;
  con->ui_expand_flag = expand_flag;
}

/**
 * Function with void * argument for #uiListPanelIDFromDataFunc.
 *
 * \note Constraint panel types are assumed to be named with the struct name field
 * concatenated to the defined prefix.
 */
static void object_constraint_panel_id(void *md_link, char *r_idname)
{
  bConstraint *con = (bConstraint *)md_link;
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(con->type);

  /* Cannot get TypeInfo for invalid/legacy constraints. */
  if (cti == nullptr) {
    return;
  }
  BLI_string_join(r_idname, BKE_ST_MAXNAME, CONSTRAINT_TYPE_PANEL_PREFIX, cti->struct_name);
}

static void bone_constraint_panel_id(void *md_link, char *r_idname)
{
  bConstraint *con = (bConstraint *)md_link;
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(con->type);

  /* Cannot get TypeInfo for invalid/legacy constraints. */
  if (cti == nullptr) {
    return;
  }
  BLI_string_join(r_idname, BKE_ST_MAXNAME, CONSTRAINT_BONE_TYPE_PANEL_PREFIX, cti->struct_name);
}

void uiTemplateConstraints(uiLayout * /*layout*/, bContext *C, bool use_bone_constraints)
{
  ARegion *region = CTX_wm_region(C);

  Object *ob = blender::ed::object::context_active_object(C);
  ListBase *constraints = {nullptr};
  if (use_bone_constraints) {
    constraints = blender::ed::object::pose_constraint_list(C);
  }
  else if (ob != nullptr) {
    constraints = &ob->constraints;
  }

  /* Switch between the bone panel ID function and the object panel ID function. */
  uiListPanelIDFromDataFunc panel_id_func = use_bone_constraints ? bone_constraint_panel_id :
                                                                   object_constraint_panel_id;

  const bool panels_match = UI_panel_list_matches_data(region, constraints, panel_id_func);

  if (!panels_match) {
    UI_panels_free_instanced(C, region);
    for (bConstraint *con =
             (constraints == nullptr) ? nullptr : static_cast<bConstraint *>(constraints->first);
         con;
         con = con->next)
    {
      /* Don't show invalid/legacy constraints. */
      if (con->type == CONSTRAINT_TYPE_NULL) {
        continue;
      }
      /* Don't show temporary constraints (AutoIK and target-less IK constraints). */
      if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
        bKinematicConstraint *data = static_cast<bKinematicConstraint *>(con->data);
        if (data->flag & CONSTRAINT_IK_TEMP) {
          continue;
        }
      }

      char panel_idname[MAX_NAME];
      panel_id_func(con, panel_idname);

      /* Create custom data RNA pointer. */
      PointerRNA *con_ptr = MEM_new<PointerRNA>(__func__);
      *con_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Constraint, con);

      Panel *new_panel = UI_panel_add_instanced(C, region, &region->panels, panel_idname, con_ptr);

      if (new_panel) {
        /* Set the list panel functionality function pointers since we don't do it with python. */
        new_panel->type->set_list_data_expand_flag = set_constraint_expand_flag;
        new_panel->type->get_list_data_expand_flag = get_constraint_expand_flag;
        new_panel->type->reorder = constraint_reorder;
      }
    }
  }
  else {
    /* Assuming there's only one group of instanced panels, update the custom data pointers. */
    Panel *panel = static_cast<Panel *>(region->panels.first);
    LISTBASE_FOREACH (bConstraint *, con, constraints) {
      /* Don't show invalid/legacy constraints. */
      if (con->type == CONSTRAINT_TYPE_NULL) {
        continue;
      }
      /* Don't show temporary constraints (AutoIK and target-less IK constraints). */
      if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
        bKinematicConstraint *data = static_cast<bKinematicConstraint *>(con->data);
        if (data->flag & CONSTRAINT_IK_TEMP) {
          continue;
        }
      }

      /* Move to the next instanced panel corresponding to the next constraint. */
      while ((panel->type == nullptr) || !(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        panel = panel->next;
        BLI_assert(panel != nullptr); /* There shouldn't be fewer panels than constraint panels. */
      }

      PointerRNA *con_ptr = MEM_new<PointerRNA>(__func__);
      *con_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Constraint, con);
      UI_panel_custom_data_set(panel, con_ptr);

      panel = panel->next;
    }
  }
}

#undef CONSTRAINT_TYPE_PANEL_PREFIX
#undef CONSTRAINT_BONE_TYPE_PANEL_PREFIX

/** \} */
