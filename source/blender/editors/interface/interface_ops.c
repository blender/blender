/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup edinterface
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h" /* for handling geometry nodes properties */
#include "DNA_object_types.h"   /* for OB_DATA_SUPPORT_ID */
#include "DNA_screen_types.h"
#include "DNA_text_types.h"

#include "BLI_blenlib.h"
#include "BLI_math_color.h"

#include "BLF_api.h"
#include "BLT_lang.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_screen.h"
#include "BKE_text.h"

#include "IMB_colormanagement.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_prototypes.h"
#include "RNA_types.h"

#include "UI_interface.h"

#include "interface_intern.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_paint.h"

/* for Copy As Driver */
#include "ED_keyframing.h"

/* only for UI_OT_editsource */
#include "BKE_main.h"
#include "BLI_ghash.h"
#include "ED_screen.h"
#include "ED_text.h"

/* -------------------------------------------------------------------- */
/** \name Immediate redraw helper
 *
 * Generally handlers shouldn't do any redrawing, that includes the layout/button definitions. That
 * violates the Model-View-Controller pattern.
 *
 * But there are some operators which really need to re-run the layout definitions for various
 * reasons. For example, "Edit Source" does it to find out which exact Python code added a button.
 * Other operators may need to access buttons that aren't currently visible. In Blender's UI code
 * design that typically means just not adding the button in the first place, for a particular
 * redraw. So the operator needs to change context and re-create the layout, so the button becomes
 * available to act on.
 *
 * \{ */

static void ui_region_redraw_immediately(bContext *C, ARegion *region)
{
  ED_region_do_layout(C, region);
  WM_draw_region_viewport_bind(region);
  ED_region_do_draw(C, region);
  WM_draw_region_viewport_unbind(region);
  region->do_draw = false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy Data Path Operator
 * \{ */

static bool copy_data_path_button_poll(bContext *C)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  char *path;
  int index;

  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (ptr.owner_id && ptr.data && prop) {
    path = RNA_path_from_ID_to_property(&ptr, prop);

    if (path) {
      MEM_freeN(path);
      return true;
    }
  }

  return false;
}

static int copy_data_path_button_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr;
  PropertyRNA *prop;
  char *path;
  int index;
  ID *id;

  const bool full_path = RNA_boolean_get(op->ptr, "full_path");

  /* try to create driver using property retrieved from UI */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (ptr.owner_id != NULL) {
    if (full_path) {
      if (prop) {
        path = RNA_path_full_property_py_ex(bmain, &ptr, prop, index, true);
      }
      else {
        path = RNA_path_full_struct_py(bmain, &ptr);
      }
    }
    else {
      path = RNA_path_from_real_ID_to_property_index(bmain, &ptr, prop, 0, -1, &id);

      if (!path) {
        path = RNA_path_from_ID_to_property(&ptr, prop);
      }
    }

    if (path) {
      WM_clipboard_text_set(path, false);
      MEM_freeN(path);
      return OPERATOR_FINISHED;
    }
  }

  return OPERATOR_CANCELLED;
}

static void UI_OT_copy_data_path_button(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Copy Data Path";
  ot->idname = "UI_OT_copy_data_path_button";
  ot->description = "Copy the RNA data path for this property to the clipboard";

  /* callbacks */
  ot->exec = copy_data_path_button_exec;
  ot->poll = copy_data_path_button_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;

  /* properties */
  prop = RNA_def_boolean(ot->srna, "full_path", false, "full_path", "Copy full data path");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy As Driver Operator
 * \{ */

static bool copy_as_driver_button_poll(bContext *C)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  char *path;
  int index;

  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (ptr.owner_id && ptr.data && prop &&
      ELEM(RNA_property_type(prop), PROP_BOOLEAN, PROP_INT, PROP_FLOAT, PROP_ENUM) &&
      (index >= 0 || !RNA_property_array_check(prop))) {
    path = RNA_path_from_ID_to_property(&ptr, prop);

    if (path) {
      MEM_freeN(path);
      return true;
    }
  }

  return false;
}

static int copy_as_driver_button_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  /* try to create driver using property retrieved from UI */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (ptr.owner_id && ptr.data && prop) {
    ID *id;
    const int dim = RNA_property_array_dimension(&ptr, prop, NULL);
    char *path = RNA_path_from_real_ID_to_property_index(bmain, &ptr, prop, dim, index, &id);

    if (path) {
      ANIM_copy_as_driver(id, path, RNA_property_identifier(prop));
      MEM_freeN(path);
      return OPERATOR_FINISHED;
    }

    BKE_reportf(op->reports, RPT_ERROR, "Could not compute a valid data path");
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_CANCELLED;
}

static void UI_OT_copy_as_driver_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy as New Driver";
  ot->idname = "UI_OT_copy_as_driver_button";
  ot->description =
      "Create a new driver with this property as input, and copy it to the "
      "clipboard. Use Paste Driver to add it to the target property, or Paste "
      "Driver Variables to extend an existing driver";

  /* callbacks */
  ot->exec = copy_as_driver_button_exec;
  ot->poll = copy_as_driver_button_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy Python Command Operator
 * \{ */

static bool copy_python_command_button_poll(bContext *C)
{
  uiBut *but = UI_context_active_but_get(C);

  if (but && (but->optype != NULL)) {
    return 1;
  }

  return 0;
}

static int copy_python_command_button_exec(bContext *C, wmOperator *UNUSED(op))
{
  uiBut *but = UI_context_active_but_get(C);

  if (but && (but->optype != NULL)) {
    PointerRNA *opptr;
    char *str;
    opptr = UI_but_operator_ptr_get(but); /* allocated when needed, the button owns it */

    str = WM_operator_pystring_ex(C, NULL, false, true, but->optype, opptr);

    WM_clipboard_text_set(str, 0);

    MEM_freeN(str);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static void UI_OT_copy_python_command_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Python Command";
  ot->idname = "UI_OT_copy_python_command_button";
  ot->description = "Copy the Python command matching this button";

  /* callbacks */
  ot->exec = copy_python_command_button_exec;
  ot->poll = copy_python_command_button_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reset to Default Values Button Operator
 * \{ */

static int operator_button_property_finish(bContext *C, PointerRNA *ptr, PropertyRNA *prop)
{
  ID *id = ptr->owner_id;

  /* perform updates required for this property */
  RNA_property_update(C, ptr, prop);

  /* as if we pressed the button */
  UI_context_active_but_prop_handle(C, false);

  /* Since we don't want to undo _all_ edits to settings, eg window
   * edits on the screen or on operator settings.
   * it might be better to move undo's inline - campbell */
  if (id && ID_CHECK_UNDO(id)) {
    /* do nothing, go ahead with undo */
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static int operator_button_property_finish_with_undo(bContext *C,
                                                     PointerRNA *ptr,
                                                     PropertyRNA *prop)
{
  /* Perform updates required for this property. */
  RNA_property_update(C, ptr, prop);

  /* As if we pressed the button. */
  UI_context_active_but_prop_handle(C, true);

  return OPERATOR_FINISHED;
}

static bool reset_default_button_poll(bContext *C)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  return (ptr.data && prop && RNA_property_editable(&ptr, prop));
}

static int reset_default_button_exec(bContext *C, wmOperator *op)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;
  const bool all = RNA_boolean_get(op->ptr, "all");

  /* try to reset the nominated setting to its default value */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  /* if there is a valid property that is editable... */
  if (ptr.data && prop && RNA_property_editable(&ptr, prop)) {
    if (RNA_property_reset(&ptr, prop, (all) ? -1 : index)) {
      return operator_button_property_finish_with_undo(C, &ptr, prop);
    }
  }

  return OPERATOR_CANCELLED;
}

static void UI_OT_reset_default_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset to Default Value";
  ot->idname = "UI_OT_reset_default_button";
  ot->description = "Reset this property's value to its default value";

  /* callbacks */
  ot->poll = reset_default_button_poll;
  ot->exec = reset_default_button_exec;

  /* flags */
  /* Don't set #OPTYPE_UNDO because #operator_button_property_finish_with_undo
   * is responsible for the undo push. */
  ot->flag = 0;

  /* properties */
  RNA_def_boolean(ot->srna, "all", 1, "All", "Reset to default values all elements of the array");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Assign Value as Default Button Operator
 * \{ */

static bool assign_default_button_poll(bContext *C)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (ptr.data && prop && RNA_property_editable(&ptr, prop)) {
    const PropertyType type = RNA_property_type(prop);

    return RNA_property_is_idprop(prop) && !RNA_property_array_check(prop) &&
           ELEM(type, PROP_INT, PROP_FLOAT);
  }

  return false;
}

static int assign_default_button_exec(bContext *C, wmOperator *UNUSED(op))
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  /* try to reset the nominated setting to its default value */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  /* if there is a valid property that is editable... */
  if (ptr.data && prop && RNA_property_editable(&ptr, prop)) {
    if (RNA_property_assign_default(&ptr, prop)) {
      return operator_button_property_finish(C, &ptr, prop);
    }
  }

  return OPERATOR_CANCELLED;
}

static void UI_OT_assign_default_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Assign Value as Default";
  ot->idname = "UI_OT_assign_default_button";
  ot->description = "Set this property's current value as the new default";

  /* callbacks */
  ot->poll = assign_default_button_poll;
  ot->exec = assign_default_button_exec;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unset Property Button Operator
 * \{ */

static int unset_property_button_exec(bContext *C, wmOperator *UNUSED(op))
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  /* try to unset the nominated property */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  /* if there is a valid property that is editable... */
  if (ptr.data && prop && RNA_property_editable(&ptr, prop) &&
      /* RNA_property_is_idprop(prop) && */
      RNA_property_is_set(&ptr, prop)) {
    RNA_property_unset(&ptr, prop);
    return operator_button_property_finish(C, &ptr, prop);
  }

  return OPERATOR_CANCELLED;
}

static void UI_OT_unset_property_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unset Property";
  ot->idname = "UI_OT_unset_property_button";
  ot->description = "Clear the property and use default or generated value in operators";

  /* callbacks */
  ot->poll = ED_operator_regionactive;
  ot->exec = unset_property_button_exec;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Define Override Type Operator
 * \{ */

/* Note that we use different values for UI/UX than 'real' override operations, user does not care
 * whether it's added or removed for the differential operation e.g. */
enum {
  UIOverride_Type_NOOP = 0,
  UIOverride_Type_Replace = 1,
  UIOverride_Type_Difference = 2, /* Add/subtract */
  UIOverride_Type_Factor = 3,     /* Multiply */
  /* TODO: should/can we expose insert/remove ones for collections? Doubt it... */
};

static EnumPropertyItem override_type_items[] = {
    {UIOverride_Type_NOOP,
     "NOOP",
     0,
     "NoOp",
     "'No-Operation', place holder preventing automatic override to ever affect the property"},
    {UIOverride_Type_Replace,
     "REPLACE",
     0,
     "Replace",
     "Completely replace value from linked data by local one"},
    {UIOverride_Type_Difference,
     "DIFFERENCE",
     0,
     "Difference",
     "Store difference to linked data value"},
    {UIOverride_Type_Factor,
     "FACTOR",
     0,
     "Factor",
     "Store factor to linked data value (useful e.g. for scale)"},
    {0, NULL, 0, NULL, NULL},
};

static bool override_type_set_button_poll(bContext *C)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  const uint override_status = RNA_property_override_library_status(
      CTX_data_main(C), &ptr, prop, index);

  return (ptr.data && prop && (override_status & RNA_OVERRIDE_STATUS_OVERRIDABLE));
}

static int override_type_set_button_exec(bContext *C, wmOperator *op)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;
  bool created;
  const bool all = RNA_boolean_get(op->ptr, "all");
  const int op_type = RNA_enum_get(op->ptr, "type");

  short operation;

  switch (op_type) {
    case UIOverride_Type_NOOP:
      operation = IDOVERRIDE_LIBRARY_OP_NOOP;
      break;
    case UIOverride_Type_Replace:
      operation = IDOVERRIDE_LIBRARY_OP_REPLACE;
      break;
    case UIOverride_Type_Difference:
      /* override code will automatically switch to subtract if needed. */
      operation = IDOVERRIDE_LIBRARY_OP_ADD;
      break;
    case UIOverride_Type_Factor:
      operation = IDOVERRIDE_LIBRARY_OP_MULTIPLY;
      break;
    default:
      operation = IDOVERRIDE_LIBRARY_OP_REPLACE;
      BLI_assert(0);
      break;
  }

  /* try to reset the nominated setting to its default value */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  BLI_assert(ptr.owner_id != NULL);

  if (all) {
    index = -1;
  }

  IDOverrideLibraryPropertyOperation *opop = RNA_property_override_property_operation_get(
      CTX_data_main(C), &ptr, prop, operation, index, true, NULL, &created);

  if (opop == NULL) {
    /* Sometimes e.g. RNA cannot generate a path to the given property. */
    BKE_reportf(op->reports, RPT_WARNING, "Failed to create the override operation");
    return OPERATOR_CANCELLED;
  }

  if (!created) {
    opop->operation = operation;
  }

  /* Outliner e.g. has to be aware of this change. */
  WM_main_add_notifier(NC_WM | ND_LIB_OVERRIDE_CHANGED, NULL);

  return operator_button_property_finish(C, &ptr, prop);
}

static int override_type_set_button_invoke(bContext *C,
                                           wmOperator *op,
                                           const wmEvent *UNUSED(event))
{
#if 0 /* Disabled for now */
  return WM_menu_invoke_ex(C, op, WM_OP_INVOKE_DEFAULT);
#else
  RNA_enum_set(op->ptr, "type", IDOVERRIDE_LIBRARY_OP_REPLACE);
  return override_type_set_button_exec(C, op);
#endif
}

static void UI_OT_override_type_set_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Define Override Type";
  ot->idname = "UI_OT_override_type_set_button";
  ot->description = "Create an override operation, or set the type of an existing one";

  /* callbacks */
  ot->poll = override_type_set_button_poll;
  ot->exec = override_type_set_button_exec;
  ot->invoke = override_type_set_button_invoke;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "all", 1, "All", "Reset to default values all elements of the array");
  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          override_type_items,
                          UIOverride_Type_Replace,
                          "Type",
                          "Type of override operation");
  /* TODO: add itemf callback, not all options are available for all data types... */
}

static bool override_remove_button_poll(bContext *C)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  const uint override_status = RNA_property_override_library_status(
      CTX_data_main(C), &ptr, prop, index);

  return (ptr.data && ptr.owner_id && prop && (override_status & RNA_OVERRIDE_STATUS_OVERRIDDEN));
}

static int override_remove_button_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr, id_refptr, src;
  PropertyRNA *prop;
  int index;
  const bool all = RNA_boolean_get(op->ptr, "all");

  /* try to reset the nominated setting to its default value */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  ID *id = ptr.owner_id;
  IDOverrideLibraryProperty *oprop = RNA_property_override_property_find(bmain, &ptr, prop, &id);
  BLI_assert(oprop != NULL);
  BLI_assert(id != NULL && id->override_library != NULL);

  const bool is_template = ID_IS_OVERRIDE_LIBRARY_TEMPLATE(id);

  /* We need source (i.e. linked data) to restore values of deleted overrides...
   * If this is an override template, we obviously do not need to restore anything. */
  if (!is_template) {
    PropertyRNA *src_prop;
    RNA_id_pointer_create(id->override_library->reference, &id_refptr);
    if (!RNA_path_resolve_property(&id_refptr, oprop->rna_path, &src, &src_prop)) {
      BLI_assert_msg(0, "Failed to create matching source (linked data) RNA pointer");
    }
  }

  if (!all && index != -1) {
    bool is_strict_find;
    /* Remove override operation for given item,
     * add singular operations for the other items as needed. */
    IDOverrideLibraryPropertyOperation *opop = BKE_lib_override_library_property_operation_find(
        oprop, NULL, NULL, index, index, false, &is_strict_find);
    BLI_assert(opop != NULL);
    if (!is_strict_find) {
      /* No specific override operation, we have to get generic one,
       * and create item-specific override operations for all but given index,
       * before removing generic one. */
      for (int idx = RNA_property_array_length(&ptr, prop); idx--;) {
        if (idx != index) {
          BKE_lib_override_library_property_operation_get(
              oprop, opop->operation, NULL, NULL, idx, idx, true, NULL, NULL);
        }
      }
    }
    BKE_lib_override_library_property_operation_delete(oprop, opop);
    if (!is_template) {
      RNA_property_copy(bmain, &ptr, &src, prop, index);
    }
    if (BLI_listbase_is_empty(&oprop->operations)) {
      BKE_lib_override_library_property_delete(id->override_library, oprop);
    }
  }
  else {
    /* Just remove whole generic override operation of this property. */
    BKE_lib_override_library_property_delete(id->override_library, oprop);
    if (!is_template) {
      RNA_property_copy(bmain, &ptr, &src, prop, -1);
    }
  }

  /* Outliner e.g. has to be aware of this change. */
  WM_main_add_notifier(NC_WM | ND_LIB_OVERRIDE_CHANGED, NULL);

  return operator_button_property_finish(C, &ptr, prop);
}

static void UI_OT_override_remove_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Override";
  ot->idname = "UI_OT_override_remove_button";
  ot->description = "Remove an override operation";

  /* callbacks */
  ot->poll = override_remove_button_poll;
  ot->exec = override_remove_button_exec;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "all", 1, "All", "Reset to default values all elements of the array");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy To Selected Operator
 * \{ */

#define NOT_NULL(assignment) ((assignment) != NULL)
#define NOT_RNA_NULL(assignment) ((assignment).data != NULL)

static void ui_context_selected_bones_via_pose(bContext *C, ListBase *r_lb)
{
  ListBase lb;
  lb = CTX_data_collection_get(C, "selected_pose_bones");

  if (!BLI_listbase_is_empty(&lb)) {
    LISTBASE_FOREACH (CollectionPointerLink *, link, &lb) {
      bPoseChannel *pchan = link->ptr.data;
      RNA_pointer_create(link->ptr.owner_id, &RNA_Bone, pchan->bone, &link->ptr);
    }
  }

  *r_lb = lb;
}

bool UI_context_copy_to_selected_list(bContext *C,
                                      PointerRNA *ptr,
                                      PropertyRNA *prop,
                                      ListBase *r_lb,
                                      bool *r_use_path_from_id,
                                      char **r_path)
{
  *r_use_path_from_id = false;
  *r_path = NULL;
  /* special case for bone constraints */
  char *path_from_bone = NULL;
  /* Remove links from the collection list which don't contain 'prop'. */
  bool ensure_list_items_contain_prop = false;

  /* PropertyGroup objects don't have a reference to the struct that actually owns
   * them, so it is normally necessary to do a brute force search to find it. This
   * handles the search for non-ID owners by using the 'active' reference as a hint
   * to preserve efficiency. Only properties defined through RNA are handled, as
   * custom properties cannot be assumed to be valid for all instances.
   *
   * Properties owned by the ID are handled by the 'if (ptr->owner_id)' case below.
   */
  if (!RNA_property_is_idprop(prop) && RNA_struct_is_a(ptr->type, &RNA_PropertyGroup)) {
    PointerRNA owner_ptr;
    char *idpath = NULL;

    /* First, check the active PoseBone and PoseBone->Bone. */
    if (NOT_RNA_NULL(
            owner_ptr = CTX_data_pointer_get_type(C, "active_pose_bone", &RNA_PoseBone))) {
      if (NOT_NULL(idpath = RNA_path_from_struct_to_idproperty(&owner_ptr, ptr->data))) {
        *r_lb = CTX_data_collection_get(C, "selected_pose_bones");
      }
      else {
        bPoseChannel *pchan = owner_ptr.data;
        RNA_pointer_create(owner_ptr.owner_id, &RNA_Bone, pchan->bone, &owner_ptr);

        if (NOT_NULL(idpath = RNA_path_from_struct_to_idproperty(&owner_ptr, ptr->data))) {
          ui_context_selected_bones_via_pose(C, r_lb);
        }
      }
    }

    if (idpath == NULL) {
      /* Check the active EditBone if in edit mode. */
      if (NOT_RNA_NULL(
              owner_ptr = CTX_data_pointer_get_type_silent(C, "active_bone", &RNA_EditBone)) &&
          NOT_NULL(idpath = RNA_path_from_struct_to_idproperty(&owner_ptr, ptr->data))) {
        *r_lb = CTX_data_collection_get(C, "selected_editable_bones");
      }

      /* Add other simple cases here (Node, NodeSocket, Sequence, ViewLayer etc). */
    }

    if (idpath) {
      *r_path = BLI_sprintfN("%s.%s", idpath, RNA_property_identifier(prop));
      MEM_freeN(idpath);
      return true;
    }
  }

  if (RNA_struct_is_a(ptr->type, &RNA_EditBone)) {
    *r_lb = CTX_data_collection_get(C, "selected_editable_bones");
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_PoseBone)) {
    *r_lb = CTX_data_collection_get(C, "selected_pose_bones");
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_Bone)) {
    ui_context_selected_bones_via_pose(C, r_lb);
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_Sequence)) {
    /* Special case when we do this for 'Sequence.lock'.
     * (if the sequence is locked, it won't be in "selected_editable_sequences"). */
    const char *prop_id = RNA_property_identifier(prop);
    if (STREQ(prop_id, "lock")) {
      *r_lb = CTX_data_collection_get(C, "selected_sequences");
    }
    else {
      *r_lb = CTX_data_collection_get(C, "selected_editable_sequences");
    }
    /* Account for properties only being available for some sequence types. */
    ensure_list_items_contain_prop = true;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_FCurve)) {
    *r_lb = CTX_data_collection_get(C, "selected_editable_fcurves");
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_Keyframe)) {
    *r_lb = CTX_data_collection_get(C, "selected_editable_keyframes");
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_Action)) {
    *r_lb = CTX_data_collection_get(C, "selected_editable_actions");
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_NlaStrip)) {
    *r_lb = CTX_data_collection_get(C, "selected_nla_strips");
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_MovieTrackingTrack)) {
    *r_lb = CTX_data_collection_get(C, "selected_movieclip_tracks");
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_Constraint) &&
           (path_from_bone = RNA_path_resolve_from_type_to_property(ptr, prop, &RNA_PoseBone)) !=
               NULL) {
    *r_lb = CTX_data_collection_get(C, "selected_pose_bones");
    *r_path = path_from_bone;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_Node) || RNA_struct_is_a(ptr->type, &RNA_NodeSocket)) {
    ListBase lb = {NULL, NULL};
    char *path = NULL;
    bNode *node = NULL;

    /* Get the node we're editing */
    if (RNA_struct_is_a(ptr->type, &RNA_NodeSocket)) {
      bNodeTree *ntree = (bNodeTree *)ptr->owner_id;
      bNodeSocket *sock = ptr->data;
      if (nodeFindNode(ntree, sock, &node, NULL)) {
        if ((path = RNA_path_resolve_from_type_to_property(ptr, prop, &RNA_Node)) != NULL) {
          /* we're good! */
        }
        else {
          node = NULL;
        }
      }
    }
    else {
      node = ptr->data;
    }

    /* Now filter by type */
    if (node) {
      lb = CTX_data_collection_get(C, "selected_nodes");

      LISTBASE_FOREACH_MUTABLE (CollectionPointerLink *, link, &lb) {
        bNode *node_data = link->ptr.data;

        if (node_data->type != node->type) {
          BLI_remlink(&lb, link);
          MEM_freeN(link);
        }
      }
    }

    *r_lb = lb;
    *r_path = path;
  }
  else if (ptr->owner_id) {
    ID *id = ptr->owner_id;

    if (GS(id->name) == ID_OB) {
      *r_lb = CTX_data_collection_get(C, "selected_editable_objects");
      *r_use_path_from_id = true;
      *r_path = RNA_path_from_ID_to_property(ptr, prop);
    }
    else if (OB_DATA_SUPPORT_ID(GS(id->name))) {
      /* check we're using the active object */
      const short id_code = GS(id->name);
      ListBase lb = CTX_data_collection_get(C, "selected_editable_objects");
      char *path = RNA_path_from_ID_to_property(ptr, prop);

      /* de-duplicate obdata */
      if (!BLI_listbase_is_empty(&lb)) {
        LISTBASE_FOREACH (CollectionPointerLink *, link, &lb) {
          Object *ob = (Object *)link->ptr.owner_id;
          if (ob->data) {
            ID *id_data = ob->data;
            id_data->tag |= LIB_TAG_DOIT;
          }
        }

        LISTBASE_FOREACH_MUTABLE (CollectionPointerLink *, link, &lb) {
          Object *ob = (Object *)link->ptr.owner_id;
          ID *id_data = ob->data;

          if ((id_data == NULL) || (id_data->tag & LIB_TAG_DOIT) == 0 || ID_IS_LINKED(id_data) ||
              (GS(id_data->name) != id_code)) {
            BLI_remlink(&lb, link);
            MEM_freeN(link);
          }
          else {
            /* Avoid prepending 'data' to the path. */
            RNA_id_pointer_create(id_data, &link->ptr);
          }

          if (id_data) {
            id_data->tag &= ~LIB_TAG_DOIT;
          }
        }
      }

      *r_lb = lb;
      *r_path = path;
    }
    else if (GS(id->name) == ID_SCE) {
      /* Sequencer's ID is scene :/ */
      /* Try to recursively find an RNA_Sequence ancestor,
       * to handle situations like T41062... */
      if ((*r_path = RNA_path_resolve_from_type_to_property(ptr, prop, &RNA_Sequence)) != NULL) {
        /* Special case when we do this for 'Sequence.lock'.
         * (if the sequence is locked, it won't be in "selected_editable_sequences"). */
        const char *prop_id = RNA_property_identifier(prop);
        if (STREQ(prop_id, "lock")) {
          *r_lb = CTX_data_collection_get(C, "selected_sequences");
        }
        else {
          *r_lb = CTX_data_collection_get(C, "selected_editable_sequences");
        }
        /* Account for properties only being available for some sequence types. */
        ensure_list_items_contain_prop = true;
      }
    }
    return (*r_path != NULL);
  }
  else {
    return false;
  }

  if (ensure_list_items_contain_prop) {
    const char *prop_id = RNA_property_identifier(prop);
    LISTBASE_FOREACH_MUTABLE (CollectionPointerLink *, link, r_lb) {
      if ((ptr->type != link->ptr.type) &&
          (RNA_struct_type_find_property(link->ptr.type, prop_id) != prop)) {
        BLI_remlink(r_lb, link);
        MEM_freeN(link);
      }
    }
  }

  return true;
}

bool UI_context_copy_to_selected_check(PointerRNA *ptr,
                                       PointerRNA *ptr_link,
                                       PropertyRNA *prop,
                                       const char *path,
                                       bool use_path_from_id,
                                       PointerRNA *r_ptr,
                                       PropertyRNA **r_prop)
{
  PointerRNA idptr;
  PropertyRNA *lprop;
  PointerRNA lptr;

  if (ptr_link->data == ptr->data) {
    return false;
  }

  if (use_path_from_id) {
    /* Path relative to ID. */
    lprop = NULL;
    RNA_id_pointer_create(ptr_link->owner_id, &idptr);
    RNA_path_resolve_property(&idptr, path, &lptr, &lprop);
  }
  else if (path) {
    /* Path relative to elements from list. */
    lprop = NULL;
    RNA_path_resolve_property(ptr_link, path, &lptr, &lprop);
  }
  else {
    lptr = *ptr_link;
    lprop = prop;
  }

  if (lptr.data == ptr->data) {
    /* temp_ptr might not be the same as ptr_link! */
    return false;
  }

  /* Skip non-existing properties on link. This was previously covered with the `lprop != prop`
   * check but we are now more permissive when it comes to ID properties, see below. */
  if (lprop == NULL) {
    return false;
  }

  if (RNA_property_type(lprop) != RNA_property_type(prop)) {
    return false;
  }

  /* Check property pointers matching.
   * For ID properties, these pointers match:
   * - If the property is API defined on an existing class (and they are equally named).
   * - Never for ID properties on specific ID (even if they are equally named).
   * - Never for NodesModifierSettings properties (even if they are equally named).
   *
   * Be permissive on ID properties in the following cases:
   * - #NodesModifierSettings properties
   *   - (special check: only if the node-group matches, since the 'Input_n' properties are name
   *      based and similar on potentially very different node-groups).
   * - ID properties on specific ID
   *   - (no special check, copying seems OK [even if type does not match -- does not do anything
   *      then])
   */
  bool ignore_prop_eq = RNA_property_is_idprop(lprop) && RNA_property_is_idprop(prop);
  if (RNA_struct_is_a(lptr.type, &RNA_NodesModifier) &&
      RNA_struct_is_a(ptr->type, &RNA_NodesModifier)) {
    ignore_prop_eq = false;

    NodesModifierData *nmd_link = (NodesModifierData *)lptr.data;
    NodesModifierData *nmd_src = (NodesModifierData *)ptr->data;
    if (nmd_link->node_group == nmd_src->node_group) {
      ignore_prop_eq = true;
    }
  }

  if ((lprop != prop) && !ignore_prop_eq) {
    return false;
  }

  if (!RNA_property_editable(&lptr, lprop)) {
    return false;
  }

  if (r_ptr) {
    *r_ptr = lptr;
  }
  if (r_prop) {
    *r_prop = lprop;
  }

  return true;
}

/**
 * Called from both exec & poll.
 *
 * \note Normally we wouldn't call a loop from within a poll function,
 * however this is a special case, and for regular poll calls, getting
 * the context from the button will fail early.
 */
static bool copy_to_selected_button(bContext *C, bool all, bool poll)
{
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr, lptr;
  PropertyRNA *prop, *lprop;
  bool success = false;
  int index;

  /* try to reset the nominated setting to its default value */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  /* if there is a valid property that is editable... */
  if (ptr.data == NULL || prop == NULL) {
    return false;
  }

  char *path = NULL;
  bool use_path_from_id;
  ListBase lb = {NULL};

  if (!UI_context_copy_to_selected_list(C, &ptr, prop, &lb, &use_path_from_id, &path)) {
    return false;
  }
  if (BLI_listbase_is_empty(&lb)) {
    MEM_SAFE_FREE(path);
    return false;
  }

  LISTBASE_FOREACH (CollectionPointerLink *, link, &lb) {
    if (link->ptr.data == ptr.data) {
      continue;
    }

    if (!UI_context_copy_to_selected_check(
            &ptr, &link->ptr, prop, path, use_path_from_id, &lptr, &lprop)) {
      continue;
    }

    if (poll) {
      success = true;
      break;
    }
    if (RNA_property_copy(bmain, &lptr, &ptr, prop, (all) ? -1 : index)) {
      RNA_property_update(C, &lptr, prop);
      success = true;
    }
  }

  MEM_SAFE_FREE(path);
  BLI_freelistN(&lb);

  return success;
}

static bool copy_to_selected_button_poll(bContext *C)
{
  return copy_to_selected_button(C, false, true);
}

static int copy_to_selected_button_exec(bContext *C, wmOperator *op)
{
  bool success;

  const bool all = RNA_boolean_get(op->ptr, "all");

  success = copy_to_selected_button(C, all, false);

  return (success) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void UI_OT_copy_to_selected_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy to Selected";
  ot->idname = "UI_OT_copy_to_selected_button";
  ot->description = "Copy property from this object to selected objects or bones";

  /* callbacks */
  ot->poll = copy_to_selected_button_poll;
  ot->exec = copy_to_selected_button_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "all", true, "All", "Copy to selected all elements of the array");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Jump to Target Operator
 * \{ */

/** Jump to the object or bone referenced by the pointer, or check if it is possible. */
static bool jump_to_target_ptr(bContext *C, PointerRNA ptr, const bool poll)
{
  if (RNA_pointer_is_null(&ptr)) {
    return false;
  }

  /* Verify pointer type. */
  char bone_name[MAXBONENAME];
  const StructRNA *target_type = NULL;

  if (ELEM(ptr.type, &RNA_EditBone, &RNA_PoseBone, &RNA_Bone)) {
    RNA_string_get(&ptr, "name", bone_name);
    if (bone_name[0] != '\0') {
      target_type = &RNA_Bone;
    }
  }
  else if (RNA_struct_is_a(ptr.type, &RNA_Object)) {
    target_type = &RNA_Object;
  }

  if (target_type == NULL) {
    return false;
  }

  /* Find the containing Object. */
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Base *base = NULL;
  const short id_type = GS(ptr.owner_id->name);
  if (id_type == ID_OB) {
    base = BKE_view_layer_base_find(view_layer, (Object *)ptr.owner_id);
  }
  else if (OB_DATA_SUPPORT_ID(id_type)) {
    base = ED_object_find_first_by_data_id(view_layer, ptr.owner_id);
  }

  bool ok = false;
  if ((base == NULL) || ((target_type == &RNA_Bone) && (base->object->type != OB_ARMATURE))) {
    /* pass */
  }
  else if (poll) {
    ok = true;
  }
  else {
    /* Make optional. */
    const bool reveal_hidden = true;
    /* Select and activate the target. */
    if (target_type == &RNA_Bone) {
      ok = ED_object_jump_to_bone(C, base->object, bone_name, reveal_hidden);
    }
    else if (target_type == &RNA_Object) {
      ok = ED_object_jump_to_object(C, base->object, reveal_hidden);
    }
    else {
      BLI_assert(0);
    }
  }
  return ok;
}

/**
 * Jump to the object or bone referred to by the current UI field value.
 *
 * \note quite heavy for a poll callback, but the operator is only
 * used as a right click menu item for certain UI field types, and
 * this will fail quickly if the context is completely unsuitable.
 */
static bool jump_to_target_button(bContext *C, bool poll)
{
  PointerRNA ptr, target_ptr;
  PropertyRNA *prop;
  int index;

  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  /* If there is a valid property... */
  if (ptr.data && prop) {
    const PropertyType type = RNA_property_type(prop);

    /* For pointer properties, use their value directly. */
    if (type == PROP_POINTER) {
      target_ptr = RNA_property_pointer_get(&ptr, prop);

      return jump_to_target_ptr(C, target_ptr, poll);
    }
    /* For string properties with prop_search, look up the search collection item. */
    if (type == PROP_STRING) {
      const uiBut *but = UI_context_active_but_get(C);
      const uiButSearch *search_but = (but->type == UI_BTYPE_SEARCH_MENU) ? (uiButSearch *)but :
                                                                            NULL;

      if (search_but && search_but->items_update_fn == ui_rna_collection_search_update_fn) {
        uiRNACollectionSearch *coll_search = search_but->arg;

        char str_buf[MAXBONENAME];
        char *str_ptr = RNA_property_string_get_alloc(&ptr, prop, str_buf, sizeof(str_buf), NULL);

        int found = RNA_property_collection_lookup_string(
            &coll_search->search_ptr, coll_search->search_prop, str_ptr, &target_ptr);

        if (str_ptr != str_buf) {
          MEM_freeN(str_ptr);
        }

        if (found) {
          return jump_to_target_ptr(C, target_ptr, poll);
        }
      }
    }
  }

  return false;
}

bool ui_jump_to_target_button_poll(bContext *C)
{
  return jump_to_target_button(C, true);
}

static int jump_to_target_button_exec(bContext *C, wmOperator *UNUSED(op))
{
  const bool success = jump_to_target_button(C, false);

  return (success) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void UI_OT_jump_to_target_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Jump to Target";
  ot->idname = "UI_OT_jump_to_target_button";
  ot->description = "Switch to the target object or bone";

  /* callbacks */
  ot->poll = ui_jump_to_target_button_poll;
  ot->exec = jump_to_target_button_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Python Source Operator
 * \{ */

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* EditSource Utility funcs and operator,
 * NOTE: this includes utility functions and button matching checks. */

typedef struct uiEditSourceStore {
  uiBut but_orig;
  GHash *hash;
} uiEditSourceStore;

typedef struct uiEditSourceButStore {
  char py_dbg_fn[FILE_MAX];
  int py_dbg_line_number;
} uiEditSourceButStore;

/* should only ever be set while the edit source operator is running */
static struct uiEditSourceStore *ui_editsource_info = NULL;

bool UI_editsource_enable_check(void)
{
  return (ui_editsource_info != NULL);
}

static void ui_editsource_active_but_set(uiBut *but)
{
  BLI_assert(ui_editsource_info == NULL);

  ui_editsource_info = MEM_callocN(sizeof(uiEditSourceStore), __func__);
  memcpy(&ui_editsource_info->but_orig, but, sizeof(uiBut));

  ui_editsource_info->hash = BLI_ghash_ptr_new(__func__);
}

static void ui_editsource_active_but_clear(void)
{
  BLI_ghash_free(ui_editsource_info->hash, NULL, MEM_freeN);
  MEM_freeN(ui_editsource_info);
  ui_editsource_info = NULL;
}

static bool ui_editsource_uibut_match(uiBut *but_a, uiBut *but_b)
{
#  if 0
  printf("matching buttons: '%s' == '%s'\n", but_a->drawstr, but_b->drawstr);
#  endif

  /* this just needs to be a 'good-enough' comparison so we can know beyond
   * reasonable doubt that these buttons are the same between redraws.
   * if this fails it only means edit-source fails - campbell */
  if (BLI_rctf_compare(&but_a->rect, &but_b->rect, FLT_EPSILON) && (but_a->type == but_b->type) &&
      (but_a->rnaprop == but_b->rnaprop) && (but_a->optype == but_b->optype) &&
      (but_a->unit_type == but_b->unit_type) &&
      STREQLEN(but_a->drawstr, but_b->drawstr, UI_MAX_DRAW_STR)) {
    return true;
  }
  return false;
}

void UI_editsource_active_but_test(uiBut *but)
{
  extern void PyC_FileAndNum_Safe(const char **r_filename, int *r_lineno);

  struct uiEditSourceButStore *but_store = MEM_callocN(sizeof(uiEditSourceButStore), __func__);

  const char *fn;
  int line_number = -1;

#  if 0
  printf("comparing buttons: '%s' == '%s'\n", but->drawstr, ui_editsource_info->but_orig.drawstr);
#  endif

  PyC_FileAndNum_Safe(&fn, &line_number);

  if (line_number != -1) {
    BLI_strncpy(but_store->py_dbg_fn, fn, sizeof(but_store->py_dbg_fn));
    but_store->py_dbg_line_number = line_number;
  }
  else {
    but_store->py_dbg_fn[0] = '\0';
    but_store->py_dbg_line_number = -1;
  }

  BLI_ghash_insert(ui_editsource_info->hash, but, but_store);
}

void UI_editsource_but_replace(const uiBut *old_but, uiBut *new_but)
{
  uiEditSourceButStore *but_store = BLI_ghash_lookup(ui_editsource_info->hash, old_but);
  if (but_store) {
    BLI_ghash_remove(ui_editsource_info->hash, old_but, NULL, NULL);
    BLI_ghash_insert(ui_editsource_info->hash, new_but, but_store);
  }
}

static int editsource_text_edit(bContext *C,
                                wmOperator *op,
                                const char filepath[FILE_MAX],
                                const int line)
{
  struct Main *bmain = CTX_data_main(C);
  Text *text = NULL;

  /* Developers may wish to copy-paste to an external editor. */
  printf("%s:%d\n", filepath, line);

  LISTBASE_FOREACH (Text *, text_iter, &bmain->texts) {
    if (text_iter->filepath && BLI_path_cmp(text_iter->filepath, filepath) == 0) {
      text = text_iter;
      break;
    }
  }

  if (text == NULL) {
    text = BKE_text_load(bmain, filepath, BKE_main_blendfile_path(bmain));
  }

  if (text == NULL) {
    BKE_reportf(op->reports, RPT_WARNING, "File '%s' cannot be opened", filepath);
    return OPERATOR_CANCELLED;
  }

  txt_move_toline(text, line - 1, false);

  /* naughty!, find text area to set, not good behavior
   * but since this is a developer tool lets allow it - campbell */
  if (!ED_text_activate_in_screen(C, text)) {
    BKE_reportf(op->reports, RPT_INFO, "See '%s' in the text editor", text->id.name + 2);
  }

  WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, text);

  return OPERATOR_FINISHED;
}

static int editsource_exec(bContext *C, wmOperator *op)
{
  uiBut *but = UI_context_active_but_get(C);

  if (but) {
    GHashIterator ghi;
    struct uiEditSourceButStore *but_store = NULL;

    ARegion *region = CTX_wm_region(C);
    int ret;

    /* needed else the active button does not get tested */
    UI_screen_free_active_but_highlight(C, CTX_wm_screen(C));

    // printf("%s: begin\n", __func__);

    /* take care not to return before calling ui_editsource_active_but_clear */
    ui_editsource_active_but_set(but);

    /* redraw and get active button python info */
    ui_region_redraw_immediately(C, region);

    for (BLI_ghashIterator_init(&ghi, ui_editsource_info->hash);
         BLI_ghashIterator_done(&ghi) == false;
         BLI_ghashIterator_step(&ghi)) {
      uiBut *but_key = BLI_ghashIterator_getKey(&ghi);
      if (but_key && ui_editsource_uibut_match(&ui_editsource_info->but_orig, but_key)) {
        but_store = BLI_ghashIterator_getValue(&ghi);
        break;
      }
    }

    if (but_store) {
      if (but_store->py_dbg_line_number != -1) {
        ret = editsource_text_edit(C, op, but_store->py_dbg_fn, but_store->py_dbg_line_number);
      }
      else {
        BKE_report(
            op->reports, RPT_ERROR, "Active button is not from a script, cannot edit source");
        ret = OPERATOR_CANCELLED;
      }
    }
    else {
      BKE_report(op->reports, RPT_ERROR, "Active button match cannot be found");
      ret = OPERATOR_CANCELLED;
    }

    ui_editsource_active_but_clear();

    // printf("%s: end\n", __func__);

    return ret;
  }

  BKE_report(op->reports, RPT_ERROR, "Active button not found");
  return OPERATOR_CANCELLED;
}

static void UI_OT_editsource(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edit Source";
  ot->idname = "UI_OT_editsource";
  ot->description = "Edit UI source code of the active button";

  /* callbacks */
  ot->exec = editsource_exec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Translation Operator
 * \{ */

/**
 * EditTranslation utility funcs and operator,
 *
 * \note this includes utility functions and button matching checks.
 * this only works in conjunction with a Python operator!
 */
static void edittranslation_find_po_file(const char *root,
                                         const char *uilng,
                                         char *path,
                                         const size_t maxlen)
{
  char tstr[32]; /* Should be more than enough! */

  /* First, full lang code. */
  BLI_snprintf(tstr, sizeof(tstr), "%s.po", uilng);
  BLI_join_dirfile(path, maxlen, root, uilng);
  BLI_path_append(path, maxlen, tstr);
  if (BLI_is_file(path)) {
    return;
  }

  /* Now try without the second iso code part (_ES in es_ES). */
  {
    const char *tc = NULL;
    size_t szt = 0;
    tstr[0] = '\0';

    tc = strchr(uilng, '_');
    if (tc) {
      szt = tc - uilng;
      if (szt < sizeof(tstr)) {            /* Paranoid, should always be true! */
        BLI_strncpy(tstr, uilng, szt + 1); /* +1 for '\0' char! */
      }
    }
    if (tstr[0]) {
      /* Because of some codes like sr_SR@latin... */
      tc = strchr(uilng, '@');
      if (tc) {
        BLI_strncpy(tstr + szt, tc, sizeof(tstr) - szt);
      }

      BLI_join_dirfile(path, maxlen, root, tstr);
      strcat(tstr, ".po");
      BLI_path_append(path, maxlen, tstr);
      if (BLI_is_file(path)) {
        return;
      }
    }
  }

  /* Else no po file! */
  path[0] = '\0';
}

static int edittranslation_exec(bContext *C, wmOperator *op)
{
  uiBut *but = UI_context_active_but_get(C);
  if (but == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Active button not found");
    return OPERATOR_CANCELLED;
  }

  wmOperatorType *ot;
  PointerRNA ptr;
  char popath[FILE_MAX];
  const char *root = U.i18ndir;
  const char *uilng = BLT_lang_get();

  uiStringInfo but_label = {BUT_GET_LABEL, NULL};
  uiStringInfo rna_label = {BUT_GET_RNA_LABEL, NULL};
  uiStringInfo enum_label = {BUT_GET_RNAENUM_LABEL, NULL};
  uiStringInfo but_tip = {BUT_GET_TIP, NULL};
  uiStringInfo rna_tip = {BUT_GET_RNA_TIP, NULL};
  uiStringInfo enum_tip = {BUT_GET_RNAENUM_TIP, NULL};
  uiStringInfo rna_struct = {BUT_GET_RNASTRUCT_IDENTIFIER, NULL};
  uiStringInfo rna_prop = {BUT_GET_RNAPROP_IDENTIFIER, NULL};
  uiStringInfo rna_enum = {BUT_GET_RNAENUM_IDENTIFIER, NULL};
  uiStringInfo rna_ctxt = {BUT_GET_RNA_LABEL_CONTEXT, NULL};

  if (!BLI_is_dir(root)) {
    BKE_report(op->reports,
               RPT_ERROR,
               "Please set your Preferences' 'Translation Branches "
               "Directory' path to a valid directory");
    return OPERATOR_CANCELLED;
  }
  ot = WM_operatortype_find(EDTSRC_I18N_OP_NAME, 0);
  if (ot == NULL) {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Could not find operator '%s'! Please enable ui_translate add-on "
                "in the User Preferences",
                EDTSRC_I18N_OP_NAME);
    return OPERATOR_CANCELLED;
  }
  /* Try to find a valid po file for current language... */
  edittranslation_find_po_file(root, uilng, popath, FILE_MAX);
  // printf("po path: %s\n", popath);
  if (popath[0] == '\0') {
    BKE_reportf(
        op->reports, RPT_ERROR, "No valid po found for language '%s' under %s", uilng, root);
    return OPERATOR_CANCELLED;
  }

  UI_but_string_info_get(C,
                         but,
                         &but_label,
                         &rna_label,
                         &enum_label,
                         &but_tip,
                         &rna_tip,
                         &enum_tip,
                         &rna_struct,
                         &rna_prop,
                         &rna_enum,
                         &rna_ctxt,
                         NULL);

  WM_operator_properties_create_ptr(&ptr, ot);
  RNA_string_set(&ptr, "lang", uilng);
  RNA_string_set(&ptr, "po_file", popath);
  RNA_string_set(&ptr, "but_label", but_label.strinfo);
  RNA_string_set(&ptr, "rna_label", rna_label.strinfo);
  RNA_string_set(&ptr, "enum_label", enum_label.strinfo);
  RNA_string_set(&ptr, "but_tip", but_tip.strinfo);
  RNA_string_set(&ptr, "rna_tip", rna_tip.strinfo);
  RNA_string_set(&ptr, "enum_tip", enum_tip.strinfo);
  RNA_string_set(&ptr, "rna_struct", rna_struct.strinfo);
  RNA_string_set(&ptr, "rna_prop", rna_prop.strinfo);
  RNA_string_set(&ptr, "rna_enum", rna_enum.strinfo);
  RNA_string_set(&ptr, "rna_ctxt", rna_ctxt.strinfo);
  const int ret = WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &ptr, NULL);

  /* Clean up */
  if (but_label.strinfo) {
    MEM_freeN(but_label.strinfo);
  }
  if (rna_label.strinfo) {
    MEM_freeN(rna_label.strinfo);
  }
  if (enum_label.strinfo) {
    MEM_freeN(enum_label.strinfo);
  }
  if (but_tip.strinfo) {
    MEM_freeN(but_tip.strinfo);
  }
  if (rna_tip.strinfo) {
    MEM_freeN(rna_tip.strinfo);
  }
  if (enum_tip.strinfo) {
    MEM_freeN(enum_tip.strinfo);
  }
  if (rna_struct.strinfo) {
    MEM_freeN(rna_struct.strinfo);
  }
  if (rna_prop.strinfo) {
    MEM_freeN(rna_prop.strinfo);
  }
  if (rna_enum.strinfo) {
    MEM_freeN(rna_enum.strinfo);
  }
  if (rna_ctxt.strinfo) {
    MEM_freeN(rna_ctxt.strinfo);
  }

  return ret;
}

static void UI_OT_edittranslation_init(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edit Translation";
  ot->idname = "UI_OT_edittranslation_init";
  ot->description = "Edit i18n in current language for the active button";

  /* callbacks */
  ot->exec = edittranslation_exec;
}

#endif /* WITH_PYTHON */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reload Translation Operator
 * \{ */

static int reloadtranslation_exec(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
  BLT_lang_init();
  BLF_cache_clear();
  BLT_lang_set(NULL);
  UI_reinit_font();
  return OPERATOR_FINISHED;
}

static void UI_OT_reloadtranslation(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reload Translation";
  ot->idname = "UI_OT_reloadtranslation";
  ot->description = "Force a full reload of UI translation";

  /* callbacks */
  ot->exec = reloadtranslation_exec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Press Button Operator
 * \{ */

static int ui_button_press_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bScreen *screen = CTX_wm_screen(C);
  const bool skip_depressed = RNA_boolean_get(op->ptr, "skip_depressed");
  ARegion *region_prev = CTX_wm_region(C);
  ARegion *region = screen ? BKE_screen_find_region_xy(screen, RGN_TYPE_ANY, event->xy) : NULL;

  if (region == NULL) {
    region = region_prev;
  }

  if (region == NULL) {
    return OPERATOR_PASS_THROUGH;
  }

  CTX_wm_region_set(C, region);
  uiBut *but = UI_context_active_but_get(C);
  CTX_wm_region_set(C, region_prev);

  if (but == NULL) {
    return OPERATOR_PASS_THROUGH;
  }
  if (skip_depressed && (but->flag & (UI_SELECT | UI_SELECT_DRAW))) {
    return OPERATOR_PASS_THROUGH;
  }

  /* Weak, this is a workaround for 'UI_but_is_tool', which checks the operator type,
   * having this avoids a minor drawing glitch. */
  void *but_optype = but->optype;

  UI_but_execute(C, region, but);

  but->optype = but_optype;

  WM_event_add_mousemove(CTX_wm_window(C));

  return OPERATOR_FINISHED;
}

static void UI_OT_button_execute(wmOperatorType *ot)
{
  ot->name = "Press Button";
  ot->idname = "UI_OT_button_execute";
  ot->description = "Presses active button";

  ot->invoke = ui_button_press_invoke;
  ot->flag = OPTYPE_INTERNAL;

  RNA_def_boolean(ot->srna, "skip_depressed", 0, "Skip Depressed", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Button Clear Operator
 * \{ */

static int button_string_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
  uiBut *but = UI_context_active_but_get_respect_menu(C);

  if (but) {
    ui_but_active_string_clear_and_exit(C, but);
  }

  return OPERATOR_FINISHED;
}

static void UI_OT_button_string_clear(wmOperatorType *ot)
{
  ot->name = "Clear Button String";
  ot->idname = "UI_OT_button_string_clear";
  ot->description = "Unsets the text of the active button";

  ot->poll = ED_operator_regionactive;
  ot->exec = button_string_clear_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drop Color Operator
 * \{ */

bool UI_drop_color_poll(struct bContext *C, wmDrag *drag, const wmEvent *UNUSED(event))
{
  /* should only return true for regions that include buttons, for now
   * return true always */
  if (drag->type == WM_DRAG_COLOR) {
    SpaceImage *sima = CTX_wm_space_image(C);
    ARegion *region = CTX_wm_region(C);

    if (UI_but_active_drop_color(C)) {
      return 1;
    }

    if (sima && (sima->mode == SI_MODE_PAINT) && sima->image &&
        (region && region->regiontype == RGN_TYPE_WINDOW)) {
      return 1;
    }
  }

  return 0;
}

void UI_drop_color_copy(wmDrag *drag, wmDropBox *drop)
{
  uiDragColorHandle *drag_info = drag->poin;

  RNA_float_set_array(drop->ptr, "color", drag_info->color);
  RNA_boolean_set(drop->ptr, "gamma", drag_info->gamma_corrected);
}

static int drop_color_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  uiBut *but = NULL;
  float color[4];
  bool gamma;

  RNA_float_get_array(op->ptr, "color", color);
  gamma = RNA_boolean_get(op->ptr, "gamma");

  /* find button under mouse, check if it has RNA color property and
   * if it does copy the data */
  but = ui_region_find_active_but(region);

  if (but && but->type == UI_BTYPE_COLOR && but->rnaprop) {
    const int color_len = RNA_property_array_length(&but->rnapoin, but->rnaprop);
    BLI_assert(color_len <= 4);

    /* keep alpha channel as-is */
    if (color_len == 4) {
      color[3] = RNA_property_float_get_index(&but->rnapoin, but->rnaprop, 3);
    }

    if (RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
      if (!gamma) {
        IMB_colormanagement_scene_linear_to_srgb_v3(color);
      }
      RNA_property_float_set_array(&but->rnapoin, but->rnaprop, color);
      RNA_property_update(C, &but->rnapoin, but->rnaprop);
    }
    else if (RNA_property_subtype(but->rnaprop) == PROP_COLOR) {
      if (gamma) {
        IMB_colormanagement_srgb_to_scene_linear_v3(color);
      }
      RNA_property_float_set_array(&but->rnapoin, but->rnaprop, color);
      RNA_property_update(C, &but->rnapoin, but->rnaprop);
    }
  }
  else {
    if (gamma) {
      srgb_to_linearrgb_v3_v3(color, color);
    }

    ED_imapaint_bucket_fill(C, color, op, event->mval);
  }

  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

static void UI_OT_drop_color(wmOperatorType *ot)
{
  ot->name = "Drop Color";
  ot->idname = "UI_OT_drop_color";
  ot->description = "Drop colors to buttons";

  ot->invoke = drop_color_invoke;
  ot->flag = OPTYPE_INTERNAL;

  RNA_def_float_color(ot->srna, "color", 3, NULL, 0.0, FLT_MAX, "Color", "Source color", 0.0, 1.0);
  RNA_def_boolean(ot->srna, "gamma", 0, "Gamma Corrected", "The source color is gamma corrected");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drop Name Operator
 * \{ */

static int drop_name_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  uiBut *but = UI_but_active_drop_name_button(C);
  char *str = RNA_string_get_alloc(op->ptr, "string", NULL, 0, NULL);

  if (str) {
    ui_but_set_string_interactive(C, but, str);
    MEM_freeN(str);
  }

  return OPERATOR_FINISHED;
}

static void UI_OT_drop_name(wmOperatorType *ot)
{
  ot->name = "Drop Name";
  ot->idname = "UI_OT_drop_name";
  ot->description = "Drop name to button";

  ot->poll = ED_operator_regionactive;
  ot->invoke = drop_name_invoke;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  RNA_def_string(
      ot->srna, "string", NULL, 0, "String", "The string value to drop into the button");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI List Search Operator
 * \{ */

static bool ui_list_focused_poll(bContext *C)
{
  const ARegion *region = CTX_wm_region(C);
  const wmWindow *win = CTX_wm_window(C);
  const uiList *list = UI_list_find_mouse_over(region, win->eventstate);

  return list != NULL;
}

/**
 * Ensure the filter options are set to be visible in the UI list.
 * \return if the visibility changed, requiring a redraw.
 */
static bool ui_list_unhide_filter_options(uiList *list)
{
  if (list->filter_flag & UILST_FLT_SHOW) {
    /* Nothing to be done. */
    return false;
  }

  list->filter_flag |= UILST_FLT_SHOW;
  return true;
}

static int ui_list_start_filter_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  uiList *list = UI_list_find_mouse_over(region, event);
  /* Poll should check. */
  BLI_assert(list != NULL);

  if (ui_list_unhide_filter_options(list)) {
    ui_region_redraw_immediately(C, region);
  }

  if (!UI_textbutton_activate_rna(C, region, list, "filter_name")) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static void UI_OT_list_start_filter(wmOperatorType *ot)
{
  ot->name = "List Filter";
  ot->idname = "UI_OT_list_start_filter";
  ot->description = "Start entering filter text for the list in focus";

  ot->invoke = ui_list_start_filter_invoke;
  ot->poll = ui_list_focused_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Tree-View Drop Operator
 * \{ */

static bool ui_tree_view_drop_poll(bContext *C)
{
  const wmWindow *win = CTX_wm_window(C);
  const ARegion *region = CTX_wm_region(C);
  const uiTreeViewItemHandle *hovered_tree_item = UI_block_tree_view_find_item_at(
      region, win->eventstate->xy);

  return hovered_tree_item != NULL;
}

static int ui_tree_view_drop_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  if (event->custom != EVT_DATA_DRAGDROP) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  const ARegion *region = CTX_wm_region(C);
  uiTreeViewItemHandle *hovered_tree_item = UI_block_tree_view_find_item_at(region, event->xy);

  if (!UI_tree_view_item_drop_handle(C, hovered_tree_item, event->customdata)) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  return OPERATOR_FINISHED;
}

static void UI_OT_tree_view_drop(wmOperatorType *ot)
{
  ot->name = "Tree View drop";
  ot->idname = "UI_OT_tree_view_drop";
  ot->description = "Drag and drop items onto a tree item";

  ot->invoke = ui_tree_view_drop_invoke;
  ot->poll = ui_tree_view_drop_poll;

  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Tree-View Item Rename Operator
 *
 * General purpose renaming operator for tree-views. Thanks to this, to add a rename button to
 * context menus for example, tree-view API users don't have to implement their own renaming
 * operators with the same logic as they already have for their #ui::AbstractTreeViewItem::rename()
 * override.
 *
 * \{ */

static bool ui_tree_view_item_rename_poll(bContext *C)
{
  const ARegion *region = CTX_wm_region(C);
  const uiTreeViewItemHandle *active_item = UI_block_tree_view_find_active_item(region);
  return active_item != NULL && UI_tree_view_item_can_rename(active_item);
}

static int ui_tree_view_item_rename_exec(bContext *C, wmOperator *UNUSED(op))
{
  ARegion *region = CTX_wm_region(C);
  uiTreeViewItemHandle *active_item = UI_block_tree_view_find_active_item(region);

  UI_tree_view_item_begin_rename(active_item);
  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

static void UI_OT_tree_view_item_rename(wmOperatorType *ot)
{
  ot->name = "Rename Tree-View Item";
  ot->idname = "UI_OT_tree_view_item_rename";
  ot->description = "Rename the active item in the tree";

  ot->exec = ui_tree_view_item_rename_exec;
  ot->poll = ui_tree_view_item_rename_poll;
  /* Could get a custom tooltip via the `get_description()` callback and another overridable
   * function of the tree-view. */

  ot->flag = OPTYPE_INTERNAL;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Drag/Drop Operator
 *
 * \{ */

static bool ui_drop_material_poll(bContext *C)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "object", &RNA_Object);
  Object *ob = ptr.data;
  if (ob == NULL) {
    return false;
  }

  PointerRNA mat_slot = CTX_data_pointer_get_type(C, "material_slot", &RNA_MaterialSlot);
  if (RNA_pointer_is_null(&mat_slot)) {
    return false;
  }

  return true;
}

static int ui_drop_material_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  if (!RNA_struct_property_is_set(op->ptr, "session_uuid")) {
    return OPERATOR_CANCELLED;
  }
  const uint32_t session_uuid = (uint32_t)RNA_int_get(op->ptr, "session_uuid");
  Material *ma = (Material *)BKE_libblock_find_session_uuid(bmain, ID_MA, session_uuid);
  if (ma == NULL) {
    return OPERATOR_CANCELLED;
  }

  PointerRNA ptr = CTX_data_pointer_get_type(C, "object", &RNA_Object);
  Object *ob = ptr.data;
  BLI_assert(ob);

  PointerRNA mat_slot = CTX_data_pointer_get_type(C, "material_slot", &RNA_MaterialSlot);
  BLI_assert(mat_slot.data);
  const int target_slot = RNA_int_get(&mat_slot, "slot_index") + 1;

  /* only drop grease pencil material on grease pencil objects */
  if ((ma->gp_style != NULL) && (ob->type != OB_GPENCIL)) {
    return OPERATOR_CANCELLED;
  }

  BKE_object_material_assign(bmain, ob, ma, target_slot, BKE_MAT_ASSIGN_USERPREF);

  WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, ob);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
  WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, ma);
  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);

  return OPERATOR_FINISHED;
}

static void UI_OT_drop_material(wmOperatorType *ot)
{
  ot->name = "Drop Material in Material slots";
  ot->description = "Drag material to Material slots in Properties";
  ot->idname = "UI_OT_drop_material";

  ot->poll = ui_drop_material_poll;
  ot->exec = ui_drop_material_exec;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  PropertyRNA *prop = RNA_def_int(ot->srna,
                                  "session_uuid",
                                  0,
                                  INT32_MIN,
                                  INT32_MAX,
                                  "Session UUID",
                                  "Session UUID of the data-block to assign",
                                  INT32_MIN,
                                  INT32_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator & Keymap Registration
 * \{ */

void ED_operatortypes_ui(void)
{
  WM_operatortype_append(UI_OT_copy_data_path_button);
  WM_operatortype_append(UI_OT_copy_as_driver_button);
  WM_operatortype_append(UI_OT_copy_python_command_button);
  WM_operatortype_append(UI_OT_reset_default_button);
  WM_operatortype_append(UI_OT_assign_default_button);
  WM_operatortype_append(UI_OT_unset_property_button);
  WM_operatortype_append(UI_OT_override_type_set_button);
  WM_operatortype_append(UI_OT_override_remove_button);
  WM_operatortype_append(UI_OT_copy_to_selected_button);
  WM_operatortype_append(UI_OT_jump_to_target_button);
  WM_operatortype_append(UI_OT_drop_color);
  WM_operatortype_append(UI_OT_drop_name);
  WM_operatortype_append(UI_OT_drop_material);
#ifdef WITH_PYTHON
  WM_operatortype_append(UI_OT_editsource);
  WM_operatortype_append(UI_OT_edittranslation_init);
#endif
  WM_operatortype_append(UI_OT_reloadtranslation);
  WM_operatortype_append(UI_OT_button_execute);
  WM_operatortype_append(UI_OT_button_string_clear);

  WM_operatortype_append(UI_OT_list_start_filter);

  WM_operatortype_append(UI_OT_tree_view_drop);
  WM_operatortype_append(UI_OT_tree_view_item_rename);

  /* external */
  WM_operatortype_append(UI_OT_eyedropper_color);
  WM_operatortype_append(UI_OT_eyedropper_colorramp);
  WM_operatortype_append(UI_OT_eyedropper_colorramp_point);
  WM_operatortype_append(UI_OT_eyedropper_id);
  WM_operatortype_append(UI_OT_eyedropper_depth);
  WM_operatortype_append(UI_OT_eyedropper_driver);
  WM_operatortype_append(UI_OT_eyedropper_gpencil_color);
}

void ED_keymap_ui(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "User Interface", 0, 0);

  eyedropper_modal_keymap(keyconf);
  eyedropper_colorband_modal_keymap(keyconf);
}

/** \} */
