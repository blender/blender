/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <cstring>

#include <fmt/format.h>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h" /* for handling geometry nodes properties */
#include "DNA_object_types.h"   /* for OB_DATA_SUPPORT_ID */
#include "DNA_screen_types.h"

#include "ANIM_keyframing.hh"

#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BLF_api.hh"
#include "BLT_lang.hh"
#include "BLT_translation.hh"

#include "BKE_anim_data.hh"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_idtype.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_material.hh"
#include "BKE_node.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "IMB_colormanagement.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "UI_abstract_view.hh"
#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "interface_intern.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_paint.hh"
#include "ED_undo.hh"

/* for Copy As Driver */
#include "ED_keyframing.hh"

/* Only for #UI_OT_editsource. */
#include "BLI_ghash.h"
#include "ED_screen.hh"

using namespace blender::ui;

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
  region->runtime->do_draw = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy Data Path Operator
 * \{ */

static bool copy_data_path_button_poll(bContext *C)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (ptr.owner_id && ptr.data && prop) {
    if (const std::optional<std::string> path = RNA_path_from_ID_to_property(&ptr, prop)) {
      UNUSED_VARS(path);
      return true;
    }
  }

  return false;
}

static wmOperatorStatus copy_data_path_button_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;
  ID *id;

  const bool full_path = RNA_boolean_get(op->ptr, "full_path");

  /* try to create driver using property retrieved from UI */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  std::optional<std::string> path;
  if (ptr.owner_id != nullptr) {
    if (full_path) {
      if (prop) {
        path = RNA_path_full_property_py_ex(&ptr, prop, index, true);
      }
      else {
        path = RNA_path_full_struct_py(&ptr);
      }
    }
    else {
      const int index_dim = (index != -1 && RNA_property_array_check(prop)) ? 1 : 0;
      path = RNA_path_from_real_ID_to_property_index(bmain, &ptr, prop, index_dim, index, &id);

      if (!path) {
        path = RNA_path_from_ID_to_property_index(&ptr, prop, index_dim, index);
      }
    }

    if (path) {
      WM_clipboard_text_set(path->c_str(), false);
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
  int index;

  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (ptr.owner_id && ptr.data && prop &&
      ELEM(RNA_property_type(prop), PROP_BOOLEAN, PROP_INT, PROP_FLOAT, PROP_ENUM) &&
      (index >= 0 || !RNA_property_array_check(prop)))
  {
    if (const std::optional<std::string> path = RNA_path_from_ID_to_property(&ptr, prop)) {
      UNUSED_VARS(path);
      return true;
    }
  }

  return false;
}

static wmOperatorStatus copy_as_driver_button_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  /* try to create driver using property retrieved from UI */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (ptr.owner_id && ptr.data && prop) {
    ID *id;
    const int dim = RNA_property_array_dimension(&ptr, prop, nullptr);
    if (const std::optional<std::string> path = RNA_path_from_real_ID_to_property_index(
            bmain, &ptr, prop, dim, index, &id))
    {
      ANIM_copy_as_driver(id, path->c_str(), RNA_property_identifier(prop));
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
      "internal clipboard. Use Paste Driver to add it to the target property, "
      "or Paste Driver Variables to extend an existing driver";

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

  if (but && (but->optype != nullptr)) {
    return true;
  }

  return false;
}

static wmOperatorStatus copy_python_command_button_exec(bContext *C, wmOperator * /*op*/)
{
  uiBut *but = UI_context_active_but_get(C);

  if (but && (but->optype != nullptr)) {
    /* allocated when needed, the button owns it */
    PointerRNA *opptr = UI_but_operator_ptr_ensure(but);

    std::string str = WM_operator_pystring_ex(C, nullptr, false, true, but->optype, opptr);

    WM_clipboard_text_set(str.c_str(), false);

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

static wmOperatorStatus operator_button_property_finish(bContext *C,
                                                        PointerRNA *ptr,
                                                        PropertyRNA *prop)
{
  /* Assign before executing logic in the unlikely event the ID is freed. */
  const bool is_undo = ptr->owner_id && ID_CHECK_UNDO(ptr->owner_id);

  /* perform updates required for this property */
  RNA_property_update(C, ptr, prop);

  /* as if we pressed the button */
  UI_context_active_but_prop_handle(C, false);

  /* Since we don't want to undo _all_ edits to settings, eg window
   * edits on the screen or on operator settings.
   * it might be better to move undo's inline - campbell */
  if (is_undo) {
    /* do nothing, go ahead with undo */
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static wmOperatorStatus operator_button_property_finish_with_undo(bContext *C,
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

static wmOperatorStatus reset_default_button_exec(bContext *C, wmOperator *op)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;
  const bool all = RNA_boolean_get(op->ptr, "all");

  /* try to reset the nominated setting to its default value */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  /* if there is a valid property that is editable... */
  if (ptr.data && prop && RNA_property_editable(&ptr, prop)) {
    const int array_index = (all) ? -1 : index;
    if (RNA_property_reset(&ptr, prop, array_index)) {

      /* Apply auto keyframe when property is successfully reset. */
      Scene *scene = CTX_data_scene(C);
      blender::animrig::autokeyframe_property(
          C, scene, &ptr, prop, array_index, scene->r.cfra, true);

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
  ot->flag = OPTYPE_REGISTER;

  /* properties */
  RNA_def_boolean(
      ot->srna, "all", true, "All", "Reset to default values all elements of the array");
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

static wmOperatorStatus assign_default_button_exec(bContext *C, wmOperator * /*op*/)
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

static wmOperatorStatus unset_property_button_exec(bContext *C, wmOperator * /*op*/)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  /* try to unset the nominated property */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  /* if there is a valid property that is editable... */
  if (ptr.data && prop && RNA_property_editable(&ptr, prop) &&
      /* RNA_property_is_idprop(prop) && */
      RNA_property_is_set(&ptr, prop))
  {
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
/** \name Add Override Operator
 * \{ */

static bool override_add_button_poll(bContext *C)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;

  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  const uint override_status = RNA_property_override_library_status(
      CTX_data_main(C), &ptr, prop, index);

  return (ptr.data && prop && (override_status & RNA_OVERRIDE_STATUS_OVERRIDABLE));
}

static wmOperatorStatus override_add_button_exec(bContext *C, wmOperator *op)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;
  bool created;
  const bool all = RNA_boolean_get(op->ptr, "all");

  const short operation = LIBOVERRIDE_OP_REPLACE;

  /* try to reset the nominated setting to its default value */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  BLI_assert(ptr.owner_id != nullptr);

  if (all) {
    index = -1;
  }

  IDOverrideLibraryPropertyOperation *opop = RNA_property_override_property_operation_get(
      CTX_data_main(C), &ptr, prop, operation, index, true, nullptr, &created);

  if (opop == nullptr) {
    /* Sometimes e.g. RNA cannot generate a path to the given property. */
    BKE_reportf(op->reports, RPT_WARNING, "Failed to create the override operation");
    return OPERATOR_CANCELLED;
  }

  if (!created) {
    opop->operation = operation;
  }

  /* Outliner e.g. has to be aware of this change. */
  WM_main_add_notifier(NC_WM | ND_LIB_OVERRIDE_CHANGED, nullptr);

  return operator_button_property_finish(C, &ptr, prop);
}

static void UI_OT_override_add_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Override";
  ot->idname = "UI_OT_override_add_button";
  ot->description = "Create an override operation";

  /* callbacks */
  ot->poll = override_add_button_poll;
  ot->exec = override_add_button_exec;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "all", true, "All", "Add overrides for all elements of the array");
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

static wmOperatorStatus override_remove_button_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  PointerRNA ptr, src;
  PropertyRNA *prop;
  int index;
  const bool all = RNA_boolean_get(op->ptr, "all");

  /* try to reset the nominated setting to its default value */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  ID *id = ptr.owner_id;
  IDOverrideLibraryProperty *oprop = RNA_property_override_property_find(bmain, &ptr, prop, &id);
  BLI_assert(oprop != nullptr);
  BLI_assert(id != nullptr && id->override_library != nullptr);

  /* The source (i.e. linked data) is required to restore values of deleted overrides. */
  PropertyRNA *src_prop;
  PointerRNA id_refptr = RNA_id_pointer_create(id->override_library->reference);
  if (!RNA_path_resolve_property(&id_refptr, oprop->rna_path, &src, &src_prop)) {
    BLI_assert_msg(0, "Failed to create matching source (linked data) RNA pointer");
  }

  if (!all && index != -1) {
    bool is_strict_find;
    /* Remove override operation for given item,
     * add singular operations for the other items as needed. */
    IDOverrideLibraryPropertyOperation *opop = BKE_lib_override_library_property_operation_find(
        oprop, nullptr, nullptr, {}, {}, index, index, false, &is_strict_find);
    BLI_assert(opop != nullptr);
    if (!is_strict_find) {
      /* No specific override operation, we have to get generic one,
       * and create item-specific override operations for all but given index,
       * before removing generic one. */
      for (int idx = RNA_property_array_length(&ptr, prop); idx--;) {
        if (idx != index) {
          BKE_lib_override_library_property_operation_get(
              oprop, opop->operation, nullptr, nullptr, {}, {}, idx, idx, true, nullptr, nullptr);
        }
      }
    }
    BKE_lib_override_library_property_operation_delete(oprop, opop);
    RNA_property_copy(bmain, &ptr, &src, prop, index);
    if (BLI_listbase_is_empty(&oprop->operations)) {
      BKE_lib_override_library_property_delete(id->override_library, oprop);
    }
  }
  else {
    /* Just remove whole generic override operation of this property. */
    BKE_lib_override_library_property_delete(id->override_library, oprop);
    RNA_property_copy(bmain, &ptr, &src, prop, -1);
  }

  /* Outliner e.g. has to be aware of this change. */
  WM_main_add_notifier(NC_WM | ND_LIB_OVERRIDE_CHANGED, nullptr);

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
  RNA_def_boolean(
      ot->srna, "all", true, "All", "Reset to default values all elements of the array");
}

static void override_idtemplate_ids_get(
    bContext *C, ID **r_owner_id, ID **r_id, PointerRNA *r_owner_ptr, PropertyRNA **r_prop)
{
  PointerRNA owner_ptr;
  PropertyRNA *prop;
  UI_context_active_but_prop_get_templateID(C, &owner_ptr, &prop);

  if (owner_ptr.data == nullptr || prop == nullptr) {
    *r_owner_id = *r_id = nullptr;
    if (r_owner_ptr != nullptr) {
      *r_owner_ptr = PointerRNA_NULL;
    }
    if (r_prop != nullptr) {
      *r_prop = nullptr;
    }
    return;
  }

  *r_owner_id = owner_ptr.owner_id;
  PointerRNA idptr = RNA_property_pointer_get(&owner_ptr, prop);
  *r_id = static_cast<ID *>(idptr.data);
  if (r_owner_ptr != nullptr) {
    *r_owner_ptr = owner_ptr;
  }
  if (r_prop != nullptr) {
    *r_prop = prop;
  }
}

static bool override_idtemplate_poll(bContext *C, const bool is_create_op)
{
  ID *owner_id, *id;
  override_idtemplate_ids_get(C, &owner_id, &id, nullptr, nullptr);

  if (owner_id == nullptr || id == nullptr) {
    return false;
  }

  if (ID_IS_PACKED(id)) {
    return false;
  }

  if (is_create_op) {
    if (!ID_IS_LINKED(id) && !ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
      return false;
    }
    return true;
  }

  /* Reset/Clear operations. */
  if (ID_IS_LINKED(id) || !ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
    return false;
  }
  return true;
}

static bool override_idtemplate_make_poll(bContext *C)
{
  return override_idtemplate_poll(C, true);
}

static wmOperatorStatus override_idtemplate_make_exec(bContext *C, wmOperator * /*op*/)
{
  ID *owner_id, *id;
  PointerRNA owner_ptr;
  PropertyRNA *prop;
  override_idtemplate_ids_get(C, &owner_id, &id, &owner_ptr, &prop);
  if (ELEM(nullptr, owner_id, id)) {
    return OPERATOR_CANCELLED;
  }

  ID *id_override = ui_template_id_liboverride_hierarchy_make(
      C, CTX_data_main(C), owner_id, id, nullptr);

  if (id_override == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* `idptr` is re-assigned to owner property to ensure proper updates etc. Here we also use it
   * to ensure remapping of the owner property from the linked data to the newly created
   * liboverride (note that in theory this remapping has already been done by code above), but
   * only in case owner ID was already local ID (override or pure local data).
   *
   * Otherwise, owner ID will also have been overridden, and remapped already to use it's
   * override of the data too. */
  if (!ID_IS_LINKED(owner_id)) {
    PointerRNA idptr = RNA_id_pointer_create(id_override);
    RNA_property_pointer_set(&owner_ptr, prop, idptr, nullptr);
  }
  RNA_property_update(C, &owner_ptr, prop);

  /* 'Security' extra tagging, since this process may also affect the owner ID and not only the
   * used ID, relying on the property update code only is not always enough. */
  DEG_id_tag_update(&CTX_data_scene(C)->id, ID_RECALC_BASE_FLAGS | ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  WM_event_add_notifier(C, NC_WM | ND_LIB_OVERRIDE_CHANGED, nullptr);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  return OPERATOR_FINISHED;
}

static void UI_OT_override_idtemplate_make(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Library Override";
  ot->idname = "UI_OT_override_idtemplate_make";
  ot->description =
      "Create a local override of the selected linked data-block, and its hierarchy of "
      "dependencies";

  /* callbacks */
  ot->poll = override_idtemplate_make_poll;
  ot->exec = override_idtemplate_make_exec;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

static bool override_idtemplate_reset_poll(bContext *C)
{
  return override_idtemplate_poll(C, false);
}

static wmOperatorStatus override_idtemplate_reset_exec(bContext *C, wmOperator * /*op*/)
{
  ID *owner_id, *id;
  PointerRNA owner_ptr;
  PropertyRNA *prop;
  override_idtemplate_ids_get(C, &owner_id, &id, &owner_ptr, &prop);
  if (ELEM(nullptr, owner_id, id)) {
    return OPERATOR_CANCELLED;
  }

  if (ID_IS_LINKED(id) || !ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
    return OPERATOR_CANCELLED;
  }

  BKE_lib_override_library_id_reset(CTX_data_main(C), id, false);

  /* `idptr` is re-assigned to owner property to ensure proper updates etc. */
  PointerRNA idptr = RNA_id_pointer_create(id);
  RNA_property_pointer_set(&owner_ptr, prop, idptr, nullptr);
  RNA_property_update(C, &owner_ptr, prop);

  /* No need for 'security' extra tagging here, since this process will never affect the owner ID.
   */

  return OPERATOR_FINISHED;
}

static void UI_OT_override_idtemplate_reset(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset Library Override";
  ot->idname = "UI_OT_override_idtemplate_reset";
  ot->description = "Reset the selected local override to its linked reference values";

  /* callbacks */
  ot->poll = override_idtemplate_reset_poll;
  ot->exec = override_idtemplate_reset_exec;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

static bool override_idtemplate_clear_poll(bContext *C)
{
  return override_idtemplate_poll(C, false);
}

static wmOperatorStatus override_idtemplate_clear_exec(bContext *C, wmOperator * /*op*/)
{
  ID *owner_id, *id;
  PointerRNA owner_ptr;
  PropertyRNA *prop;
  override_idtemplate_ids_get(C, &owner_id, &id, &owner_ptr, &prop);
  if (ELEM(nullptr, owner_id, id)) {
    return OPERATOR_CANCELLED;
  }

  if (ID_IS_LINKED(id)) {
    return OPERATOR_CANCELLED;
  }

  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Scene *scene = CTX_data_scene(C);
  ID *id_new = id;

  if (BKE_lib_override_library_is_hierarchy_leaf(bmain, id)) {
    id_new = id->override_library->reference;
    bool do_remap_active = false;
    BKE_view_layer_synced_ensure(scene, view_layer);
    if (BKE_view_layer_active_object_get(view_layer) == (Object *)id) {
      BLI_assert(GS(id->name) == ID_OB);
      BLI_assert(GS(id_new->name) == ID_OB);
      do_remap_active = true;
    }
    BKE_libblock_remap(bmain, id, id_new, ID_REMAP_SKIP_INDIRECT_USAGE);
    if (do_remap_active) {
      Object *ref_object = (Object *)id_new;
      Base *basact = BKE_view_layer_base_find(view_layer, ref_object);
      if (basact != nullptr) {
        view_layer->basact = basact;
      }
      DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    }
    BKE_id_delete(bmain, id);
  }
  else {
    BKE_lib_override_library_id_reset(bmain, id, true);
  }

  /* Here the affected ID may remain the same, or be replaced by its linked reference. In either
   * case, the owner ID remains unchanged, and remapping is already handled by internal code, so
   * calling `RNA_property_update` on it is enough to ensure proper notifiers are sent. */
  RNA_property_update(C, &owner_ptr, prop);

  /* 'Security' extra tagging, since this process may also affect the owner ID and not only the
   * used ID, relying on the property update code only is not always enough. */
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS | ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  WM_event_add_notifier(C, NC_WM | ND_LIB_OVERRIDE_CHANGED, nullptr);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  return OPERATOR_FINISHED;
}

static void UI_OT_override_idtemplate_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Library Override";
  ot->idname = "UI_OT_override_idtemplate_clear";
  ot->description =
      "Delete the selected local override and relink its usages to the linked data-block if "
      "possible, else reset it and mark it as non editable";

  /* callbacks */
  ot->poll = override_idtemplate_clear_poll;
  ot->exec = override_idtemplate_clear_exec;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

static bool override_idtemplate_menu_poll(const bContext *C_const, MenuType * /*mt*/)
{
  bContext *C = (bContext *)C_const;
  ID *owner_id, *id;
  override_idtemplate_ids_get(C, &owner_id, &id, nullptr, nullptr);

  if (owner_id == nullptr || id == nullptr) {
    return false;
  }

  if (!(ID_IS_LINKED(id) || ID_IS_OVERRIDE_LIBRARY_REAL(id))) {
    return false;
  }
  return true;
}

static void override_idtemplate_menu_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  layout->op("UI_OT_override_idtemplate_make", IFACE_("Make"), ICON_NONE);
  layout->op("UI_OT_override_idtemplate_reset", IFACE_("Reset"), ICON_NONE);
  layout->op("UI_OT_override_idtemplate_clear", IFACE_("Clear"), ICON_NONE);
}

static void override_idtemplate_menu()
{
  MenuType *mt;

  mt = MEM_callocN<MenuType>(__func__);
  STRNCPY_UTF8(mt->idname, "UI_MT_idtemplate_liboverride");
  STRNCPY_UTF8(mt->label, N_("Library Override"));
  mt->poll = override_idtemplate_menu_poll;
  mt->draw = override_idtemplate_menu_draw;
  WM_menutype_add(mt);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy To Selected Operator
 * \{ */

#define NOT_RNA_NULL(assignment) ((assignment).data != nullptr)

/**
 * Construct a PointerRNA that points to pchan->bone.
 *
 * Pose bones are owned by an Object, whereas `pchan->bone` is owned by the Armature, so this
 * doesn't just remap the pointer's `data` field, but also its `owner_id`.
 */
static PointerRNA rnapointer_pchan_to_bone(const PointerRNA &pchan_ptr)
{
  bPoseChannel *pchan = static_cast<bPoseChannel *>(pchan_ptr.data);

  BLI_assert(GS(pchan_ptr.owner_id->name) == ID_OB);
  Object *object = reinterpret_cast<Object *>(pchan_ptr.owner_id);

  BLI_assert(GS(static_cast<ID *>(object->data)->name) == ID_AR);
  bArmature *armature = static_cast<bArmature *>(object->data);

  return RNA_pointer_create_discrete(&armature->id, &RNA_Bone, pchan->bone);
}

static void ui_context_selected_bones_via_pose(bContext *C, blender::Vector<PointerRNA> *r_lb)
{
  blender::Vector<PointerRNA> lb = CTX_data_collection_get(C, "selected_pose_bones");

  for (PointerRNA &ptr : lb) {
    ptr = rnapointer_pchan_to_bone(ptr);
  }

  *r_lb = std::move(lb);
}

static void ui_context_fcurve_modifiers_via_fcurve(bContext *C,
                                                   blender::Vector<PointerRNA> *r_lb,
                                                   FModifier *source)
{
  blender::Vector<PointerRNA> fcurve_links;
  fcurve_links = CTX_data_collection_get(C, "selected_editable_fcurves");
  if (fcurve_links.is_empty()) {
    return;
  }
  r_lb->clear();
  for (const PointerRNA &ptr : fcurve_links) {
    const FCurve *fcu = static_cast<const FCurve *>(ptr.data);
    LISTBASE_FOREACH (FModifier *, mod, &fcu->modifiers) {
      if (STREQ(mod->name, source->name) && mod->type == source->type) {
        r_lb->append(RNA_pointer_create_discrete(ptr.owner_id, &RNA_FModifier, mod));
        /* Since names are unique it is safe to break here. */
        break;
      }
    }
  }
}

static void ui_context_selected_key_blocks(ID *owner_id_key, blender::Vector<PointerRNA> *r_lb)
{
  /* This function chooses to return the selected keyblocks of the owning Key ID.
   * The other option would be to return identically named keyblocks from selected objects. I
   * (christoph) think that the first case is more useful which is why the function works as it
   * does. */
  Key *containing_key = reinterpret_cast<Key *>(owner_id_key);
  LISTBASE_FOREACH (KeyBlock *, key_block, &containing_key->block) {
    /* This does not use the function `shape_key_is_selected` since that would include the active
     * shapekey which is not required for this function to work. */
    if (key_block->flag & KEYBLOCK_SEL) {
      r_lb->append(RNA_pointer_create_discrete(owner_id_key, &RNA_ShapeKey, key_block));
    }
  }
}

bool UI_context_copy_to_selected_list(bContext *C,
                                      PointerRNA *ptr,
                                      PropertyRNA *prop,
                                      blender::Vector<PointerRNA> *r_lb,
                                      bool *r_use_path_from_id,
                                      std::optional<std::string> *r_path)
{
  *r_use_path_from_id = false;
  *r_path = std::nullopt;
  /* special case for bone constraints */
  const bool is_rna = !RNA_property_is_idprop(prop);
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
  if (is_rna && RNA_struct_is_a(ptr->type, &RNA_PropertyGroup)) {
    PointerRNA owner_ptr;
    std::optional<std::string> idpath;

    /* First, check the active PoseBone and PoseBone->Bone. */
    if (NOT_RNA_NULL(owner_ptr = CTX_data_pointer_get_type(C, "active_pose_bone", &RNA_PoseBone)))
    {
      idpath = RNA_path_from_struct_to_idproperty(&owner_ptr,
                                                  static_cast<const IDProperty *>(ptr->data));
      if (idpath) {
        *r_lb = CTX_data_collection_get(C, "selected_pose_bones");
      }
      else {
        PointerRNA bone_ptr = rnapointer_pchan_to_bone(owner_ptr);
        idpath = RNA_path_from_struct_to_idproperty(&bone_ptr,
                                                    static_cast<const IDProperty *>(ptr->data));
        if (idpath) {
          ui_context_selected_bones_via_pose(C, r_lb);
        }
      }
    }

    if (!idpath) {
      /* Check the active EditBone if in edit mode. */
      if (NOT_RNA_NULL(
              owner_ptr = CTX_data_pointer_get_type_silent(C, "active_bone", &RNA_EditBone)))
      {
        idpath = RNA_path_from_struct_to_idproperty(&owner_ptr,
                                                    static_cast<const IDProperty *>(ptr->data));
        if (idpath) {
          *r_lb = CTX_data_collection_get(C, "selected_editable_bones");
        }
      }

      /* Add other simple cases here (Node, NodeSocket, Sequence, ViewLayer etc). */
    }

    if (idpath) {
      *r_path = fmt::format("{}.{}", *idpath, RNA_property_identifier(prop));
      return true;
    }
  }

  if (RNA_struct_is_a(ptr->type, &RNA_EditBone)) {
    /* Special case when we do this for #edit_bone.lock.
     * (if the edit_bone is locked, it is not included in "selected_editable_bones"). */
    const char *prop_id = RNA_property_identifier(prop);
    if (STREQ(prop_id, "lock")) {
      *r_lb = CTX_data_collection_get(C, "selected_bones");
    }
    else {
      *r_lb = CTX_data_collection_get(C, "selected_editable_bones");
    }
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_PoseBone)) {
    *r_lb = CTX_data_collection_get(C, "selected_pose_bones");
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_Bone)) {
    /* "selected_bones" or "selected_editable_bones" will only yield anything in Armature Edit
     * mode. In other modes, it'll be empty, and the only way to get the selected bones is via
     * "selected_pose_bones". */
    ui_context_selected_bones_via_pose(C, r_lb);
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_BoneColor)) {
    /* Get the things that own the bone color (bones, pose bones, or edit bones). */
    /* First this will be bones, then gets remapped to colors. */
    blender::Vector<PointerRNA> list_of_things = {};
    switch (GS(ptr->owner_id->name)) {
      case ID_OB:
        list_of_things = CTX_data_collection_get(C, "selected_pose_bones");
        break;
      case ID_AR: {
        /* Armature-owned bones can be accessed from both edit mode and pose mode.
         * - Edit mode: visit selected edit bones.
         * - Pose mode: visit the armature bones of selected pose bones.
         */
        const bArmature *arm = reinterpret_cast<bArmature *>(ptr->owner_id);
        if (arm->edbo) {
          list_of_things = CTX_data_collection_get(C, "selected_editable_bones");
        }
        else {
          list_of_things = CTX_data_collection_get(C, "selected_pose_bones");
          CTX_data_collection_remap_property(list_of_things, "bone");
        }
        break;
      }
      default:
        printf("BoneColor is unexpectedly owned by %s '%s'\n",
               BKE_idtype_idcode_to_name(GS(ptr->owner_id->name)),
               ptr->owner_id->name + 2);
        BLI_assert_msg(false,
                       "expected BoneColor to be owned by the Armature "
                       "(bone & edit bone) or the Object (pose bone)");
        return false;
    }

    /* Remap from some bone to its color, to ensure the items of r_lb are of
     * type ptr->type. Since all three structs `bPoseChan`, `Bone`, and
     * `EditBone` have the same name for their embedded `BoneColor` struct, this
     * code is suitable for all of them. */
    CTX_data_collection_remap_property(list_of_things, "color");

    *r_lb = list_of_things;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_Strip)) {
    /* Special case when we do this for 'Strip.lock'.
     * (if the strip is locked, it won't be in "selected_editable_strips"). */
    const char *prop_id = RNA_property_identifier(prop);
    if (STREQ(prop_id, "lock")) {
      *r_lb = CTX_data_collection_get(C, "selected_strips");
    }
    else {
      *r_lb = CTX_data_collection_get(C, "selected_editable_strips");
    }

    if (is_rna) {
      /* Account for properties only being available for some sequence types. */
      ensure_list_items_contain_prop = true;
    }
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_FCurve)) {
    *r_lb = CTX_data_collection_get(C, "selected_editable_fcurves");
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_FModifier)) {
    FModifier *mod = static_cast<FModifier *>(ptr->data);
    ui_context_fcurve_modifiers_via_fcurve(C, r_lb, mod);
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
  else if (RNA_struct_is_a(ptr->type, &RNA_ShapeKey)) {
    ui_context_selected_key_blocks(ptr->owner_id, r_lb);
  }
  else if (const std::optional<std::string> path_from_bone =
               RNA_path_resolve_from_type_to_property(ptr, prop, &RNA_PoseBone);
           RNA_struct_is_a(ptr->type, &RNA_Constraint) && path_from_bone)
  {
    *r_lb = CTX_data_collection_get(C, "selected_pose_bones");
    *r_path = path_from_bone;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_Node) || RNA_struct_is_a(ptr->type, &RNA_NodeSocket)) {
    blender::Vector<PointerRNA> lb;
    std::optional<std::string> path;
    bNode *node = nullptr;

    /* Get the node we're editing */
    if (RNA_struct_is_a(ptr->type, &RNA_NodeSocket)) {
      bNodeTree *ntree = (bNodeTree *)ptr->owner_id;
      bNodeSocket *sock = static_cast<bNodeSocket *>(ptr->data);
      node = &blender::bke::node_find_node(*ntree, *sock);
      path = RNA_path_resolve_from_type_to_property(ptr, prop, &RNA_Node);
      if (path) {
        /* we're good! */
      }
      else {
        node = nullptr;
      }
    }
    else {
      node = static_cast<bNode *>(ptr->data);
    }

    /* Now filter out non-matching nodes (by idname). */
    if (node) {
      const blender::StringRef node_idname = node->idname;
      lb = CTX_data_collection_get(C, "selected_nodes");
      lb.remove_if([&](const PointerRNA &link) {
        bNode *node_data = static_cast<bNode *>(link.data);
        if (node_data->idname != node_idname) {
          return true;
        }
        return false;
      });
    }

    *r_lb = lb;
    *r_path = path;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_AssetMetaData)) {
    /* Remap from #AssetRepresentation to #AssetMetaData. */
    blender::Vector<PointerRNA> list_of_things = CTX_data_collection_get(C, "selected_assets");
    CTX_data_collection_remap_property(list_of_things, "metadata");
    *r_lb = list_of_things;
  }
  else if (CTX_wm_space_outliner(C)) {
    const ID *id = ptr->owner_id;
    if (!(id && (GS(id->name) == ID_OB))) {
      return false;
    }

    ListBase selected_objects = {nullptr};
    ED_outliner_selected_objects_get(C, &selected_objects);
    LISTBASE_FOREACH (LinkData *, link, &selected_objects) {
      Object *ob = static_cast<Object *>(link->data);
      r_lb->append(RNA_id_pointer_create(&ob->id));
    }
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
      blender::Vector<PointerRNA> lb = CTX_data_collection_get(C, "selected_editable_objects");
      const std::optional<std::string> path = RNA_path_from_ID_to_property(ptr, prop);

      /* de-duplicate obdata */
      if (!lb.is_empty()) {
        for (const PointerRNA &ob_ptr : lb) {
          Object *ob = (Object *)ob_ptr.owner_id;
          if (ID *id_data = static_cast<ID *>(ob->data)) {
            id_data->tag |= ID_TAG_DOIT;
          }
        }

        blender::Vector<PointerRNA> new_lb;
        for (const PointerRNA &link : lb) {
          Object *ob = (Object *)link.owner_id;
          ID *id_data = static_cast<ID *>(ob->data);
          if ((id_data == nullptr) || (id_data->tag & ID_TAG_DOIT) == 0 ||
              !ID_IS_EDITABLE(id_data) || (GS(id_data->name) != id_code))
          {
            continue;
          }
          /* Avoid prepending 'data' to the path. */
          new_lb.append(RNA_id_pointer_create(id_data));

          if (id_data) {
            id_data->tag &= ~ID_TAG_DOIT;
          }
        }

        lb = std::move(new_lb);
      }

      *r_lb = lb;
      *r_path = path;
    }
    else if (GS(id->name) == ID_SCE) {
      /* Sequencer's ID is scene :/ */
      /* Try to recursively find an RNA_Strip ancestor,
       * to handle situations like #41062... */
      *r_path = RNA_path_resolve_from_type_to_property(ptr, prop, &RNA_Strip);
      if (r_path->has_value()) {
        /* Special case when we do this for 'Strip.lock'.
         * (if the strip is locked, it won't be in "selected_editable_strips"). */
        const char *prop_id = RNA_property_identifier(prop);
        if (is_rna && STREQ(prop_id, "lock")) {
          *r_lb = CTX_data_collection_get(C, "selected_strips");
        }
        else {
          *r_lb = CTX_data_collection_get(C, "selected_editable_strips");
        }

        if (is_rna) {
          /* Account for properties only being available for some sequence types. */
          ensure_list_items_contain_prop = true;
        }
      }
    }
    return r_path->has_value();
  }
  else {
    return false;
  }

  if (RNA_property_is_idprop(prop)) {
    if (!r_path->has_value()) {
      *r_path = RNA_path_from_ptr_to_property_index(ptr, prop, 0, -1);
      BLI_assert(*r_path);
    }
    /* Always resolve custom-properties because they can always exist per-item. */
    ensure_list_items_contain_prop = true;
  }

  if (ensure_list_items_contain_prop) {
    if (is_rna) {
      const char *prop_id = RNA_property_identifier(prop);
      r_lb->remove_if([&](const PointerRNA &link) {
        if ((ptr->type != link.type) &&
            (RNA_struct_type_find_property(link.type, prop_id) != prop))
        {
          return true;
        }
        return false;
      });
    }
    else {
      const bool prop_is_array = RNA_property_array_check(prop);
      const int prop_array_len = prop_is_array ? RNA_property_array_length(ptr, prop) : -1;
      const PropertyType prop_type = RNA_property_type(prop);
      r_lb->remove_if([&](PointerRNA &link) {
        PointerRNA lptr;
        PropertyRNA *lprop = nullptr;
        RNA_path_resolve_property(
            &link, r_path->has_value() ? r_path->value().c_str() : nullptr, &lptr, &lprop);

        if (lprop == nullptr) {
          return true;
        }
        if (!RNA_property_is_idprop(lprop)) {
          return true;
        }
        if (prop_type != RNA_property_type(lprop)) {
          return true;
        }
        if (prop_is_array != RNA_property_array_check(lprop)) {
          return true;
        }
        if (prop_is_array && (prop_array_len != RNA_property_array_length(&link, lprop))) {
          return true;
        }
        return false;
      });
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
  PropertyRNA *lprop;
  PointerRNA lptr;

  if (ptr_link->data == ptr->data) {
    return false;
  }

  if (use_path_from_id) {
    /* Path relative to ID. */
    lprop = nullptr;
    PointerRNA idptr = RNA_id_pointer_create(ptr_link->owner_id);
    RNA_path_resolve_property(&idptr, path, &lptr, &lprop);
  }
  else if (path) {
    /* Path relative to elements from list. */
    lprop = nullptr;
    RNA_path_resolve_property(ptr_link, path, &lptr, &lprop);
  }
  else {
    BLI_assert(!RNA_property_is_idprop(prop));
    lptr = *ptr_link;
    lprop = prop;
  }

  if (lptr.data == ptr->data) {
    /* The source & destination are the same, so there is nothing to copy. */
    return false;
  }

  /* Skip non-existing properties on link. This was previously covered with the `lprop != prop`
   * check but we are now more permissive when it comes to ID properties, see below. */
  if (lprop == nullptr) {
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
      RNA_struct_is_a(ptr->type, &RNA_NodesModifier))
  {
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
  int index;

  /* try to reset the nominated setting to its default value */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  /* if there is a valid property that is editable... */
  if (ptr.data == nullptr || prop == nullptr) {
    return false;
  }

  bool success = false;
  std::optional<std::string> path;
  bool use_path_from_id;
  blender::Vector<PointerRNA> lb;

  if (UI_context_copy_to_selected_list(C, &ptr, prop, &lb, &use_path_from_id, &path)) {
    for (PointerRNA &link : lb) {
      if (link.data == ptr.data) {
        continue;
      }

      if (!UI_context_copy_to_selected_check(&ptr,
                                             &link,
                                             prop,
                                             path.has_value() ? path->c_str() : nullptr,
                                             use_path_from_id,
                                             &lptr,
                                             &lprop))
      {
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
  }

  return success;
}

static bool copy_to_selected_button_poll(bContext *C)
{
  return copy_to_selected_button(C, false, true);
}

static wmOperatorStatus copy_to_selected_button_exec(bContext *C, wmOperator *op)
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
  ot->description =
      "Copy the property's value from the active item to the same property of all selected items "
      "if the same property exists";

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
/** \name Copy Driver To Selected Operator
 * \{ */

/* Name-spaced for unit testing. Conceptually these functions should be static
 * and not be used outside this source file.  But they need to be externally
 * accessible to add unit tests for them. */
namespace blender::interface::internal {

blender::Vector<FCurve *> get_property_drivers(
    PointerRNA *ptr, PropertyRNA *prop, const bool get_all, const int index, bool *r_is_array_prop)
{
  BLI_assert(ptr && prop);

  const std::optional<std::string> path = RNA_path_from_ID_to_property(ptr, prop);
  if (!path.has_value()) {
    return {};
  }

  AnimData *adt = BKE_animdata_from_id(ptr->owner_id);
  if (!adt) {
    return {};
  }

  blender::Vector<FCurve *> drivers = {};
  const bool is_array_prop = RNA_property_array_check(prop);
  if (!is_array_prop) {
    /* NOTE: by convention Blender assigns 0 as the index for drivers of
     * non-array properties, which is why we search for it here.  Values > 0 are
     * not recognized by Blender's driver system in that case.  Values < 0 are
     * recognized for driver evaluation, but `BKE_fcurve_find()` unconditionally
     * returns nullptr in that case so it wouldn't matter here anyway. */
    drivers.append(BKE_fcurve_find(&adt->drivers, path->c_str(), 0));
  }
  else {
    /* For array properties, we always allocate space for all elements of an
     * array property, and the unused ones just remain null. */
    drivers.resize(RNA_property_array_length(ptr, prop), nullptr);
    for (int i = 0; i < drivers.size(); i++) {
      if (get_all || i == index) {
        drivers[i] = BKE_fcurve_find(&adt->drivers, path->c_str(), i);
      }
    }
  }

  /* If we didn't get any drivers to copy, instead of returning a vector of all
   * nullptr, return an empty vector for clarity. That way the caller gets
   * either a useful result or an empty one. */
  bool fetched_at_least_one = false;
  for (const FCurve *driver : drivers) {
    fetched_at_least_one |= driver != nullptr;
  }
  if (!fetched_at_least_one) {
    return {};
  }

  if (r_is_array_prop) {
    *r_is_array_prop = is_array_prop;
  }

  return drivers;
}

int paste_property_drivers(blender::Span<FCurve *> src_drivers,
                           const bool is_array_prop,
                           PointerRNA *dst_ptr,
                           PropertyRNA *dst_prop)
{
  BLI_assert(src_drivers.size() > 0);
  BLI_assert(is_array_prop || src_drivers.size() == 1);

  /* Get the RNA path and relevant animdata for the property we're copying to. */
  const std::optional<std::string> dst_path = RNA_path_from_ID_to_property(dst_ptr, dst_prop);
  if (!dst_path.has_value()) {
    return 0;
  }
  AnimData *dst_adt = BKE_animdata_ensure_id(dst_ptr->owner_id);
  if (!dst_adt) {
    return 0;
  }

  /* Do the copying. */
  int paste_count = 0;
  for (int i = 0; i < src_drivers.size(); i++) {
    if (!src_drivers[i]) {
      continue;
    }
    const int dst_index = is_array_prop ? i : -1;

    /* If it's already animated by something other than a driver, skip. This is
     * because Blender's UI assumes that properties are either animated *or*
     * driven, and things can get confusing for users otherwise. Additionally,
     * no other parts of Blender's UI allow users to (at least easily) add
     * drivers on already-animated properties, so this keeps things consistent
     * across driver-related operators. */
    bool driven;
    {
      const FCurve *fcu = BKE_fcurve_find_by_rna(
          dst_ptr, dst_prop, dst_index, nullptr, nullptr, &driven, nullptr);
      if (fcu && !driven) {
        continue;
      }
    }

    /* If there's an existing matching driver, remove it first.
     *
     * TODO: in the context of `copy_driver_to_selected_button()` this has
     * quadratic complexity when the drivers are within the same ID, due to this
     * being inside of a loop and doing a linear scan of the drivers to find one
     * that matches.  We should be able to make this more efficient with a
     * little cleverness. */
    if (driven) {
      FCurve *old_driver = BKE_fcurve_find(&dst_adt->drivers, dst_path->c_str(), dst_index);
      if (old_driver) {
        BLI_remlink(&dst_adt->drivers, old_driver);
        BKE_fcurve_free(old_driver);
      }
    }

    /* Create the new driver. */
    FCurve *new_driver = BKE_fcurve_copy(src_drivers[i]);
    BKE_fcurve_rnapath_set(*new_driver, dst_path.value());
    BLI_addtail(&dst_adt->drivers, new_driver);

    paste_count++;
  }

  return paste_count;
}

}  // namespace blender::interface::internal

/**
 * Called from both exec & poll.
 *
 * \note We use this function for both poll and exec because the logic for
 * whether there is a valid selection to copy to is baked into
 * `UI_context_copy_to_selected_list()`, and the setup required to call that
 * would either be duplicated or need to be split out into its own awkward
 * difficult-to-name function with a large number of parameters.  So instead we
 * follow the same pattern as `copy_to_selected_button()` further above, with a
 * bool to switch between exec and poll behavior.  This isn't great, but seems
 * like the lesser evil under the circumstances.
 *
 * \param copy_entire_array: If true, copies drivers of all elements of an array
 * property. Otherwise only copies one specific element.
 * \param poll: If true, only checks if the driver(s) could be copied rather than
 * actually performing the copy.
 *
 * \returns true in exec mode if any copies were successfully made, and false
 * otherwise.  Returns true in poll mode if a copy could be successfully made,
 * and false otherwise.
 */
static bool copy_driver_to_selected_button(bContext *C, bool copy_entire_array, const bool poll)
{
  using namespace blender::interface::internal;

  PropertyRNA *prop;
  PointerRNA ptr;
  int index;

  /* Get the property of the clicked button. */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);
  if (!ptr.data || !ptr.owner_id || !prop) {
    return false;
  }
  copy_entire_array |= index == -1; /* -1 implies `copy_entire_array` for array properties. */

  /* Get the property's driver(s). */
  bool is_array_prop = false;
  const blender::Vector<FCurve *> src_drivers = get_property_drivers(
      &ptr, prop, copy_entire_array, index, &is_array_prop);
  if (src_drivers.is_empty()) {
    return false;
  }

  /* Build the list of properties to copy the driver(s) to, along with relevant
   * side data. */
  std::optional<std::string> path;
  bool use_path_from_id;
  blender::Vector<PointerRNA> target_properties;
  if (!UI_context_copy_to_selected_list(
          C, &ptr, prop, &target_properties, &use_path_from_id, &path))
  {
    return false;
  }

  /* Copy the driver(s) to the list of target properties. */
  int total_copy_count = 0;
  for (PointerRNA &target_prop : target_properties) {
    if (target_prop.data == ptr.data) {
      continue;
    }

    /* Get the target property and ensure that it's appropriate for adding
     * drivers. */
    PropertyRNA *dst_prop;
    PointerRNA dst_ptr;
    if (!UI_context_copy_to_selected_check(&ptr,
                                           &target_prop,
                                           prop,
                                           path.has_value() ? path->c_str() : nullptr,
                                           use_path_from_id,
                                           &dst_ptr,
                                           &dst_prop))
    {
      continue;
    }
    if (!RNA_property_driver_editable(&dst_ptr, dst_prop)) {
      continue;
    }

    /* If we're just polling, then we early-out on the first property we would
     * be able to copy to. */
    if (poll) {
      return true;
    }

    const int paste_count = paste_property_drivers(
        src_drivers.as_span(), is_array_prop, &dst_ptr, dst_prop);
    if (paste_count == 0) {
      continue;
    }

    RNA_property_update(C, &dst_ptr, dst_prop);
    total_copy_count += paste_count;
  }

  return total_copy_count > 0;
}

static bool copy_driver_to_selected_button_poll(bContext *C)
{
  return copy_driver_to_selected_button(C, false, true);
}

static wmOperatorStatus copy_driver_to_selected_button_exec(bContext *C, wmOperator *op)
{
  const bool all = RNA_boolean_get(op->ptr, "all");

  if (!copy_driver_to_selected_button(C, all, false)) {
    return OPERATOR_CANCELLED;
  }

  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, nullptr);
  return OPERATOR_FINISHED;
}

static void UI_OT_copy_driver_to_selected_button(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Copy Driver to Selected";
  ot->idname = "UI_OT_copy_driver_to_selected_button";
  ot->description =
      "Copy the property's driver from the active item to the same property "
      "of all selected items, if the same property exists";

  /* Callbacks. */
  ot->poll = copy_driver_to_selected_button_poll;
  ot->exec = copy_driver_to_selected_button_exec;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  RNA_def_boolean(
      ot->srna, "all", false, "All", "Copy to selected the drivers of all elements of the array");
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
  const StructRNA *target_type = nullptr;

  if (ELEM(ptr.type, &RNA_EditBone, &RNA_PoseBone, &RNA_Bone)) {
    RNA_string_get(&ptr, "name", bone_name);
    if (bone_name[0] != '\0') {
      target_type = &RNA_Bone;
    }
  }
  else if (RNA_struct_is_a(ptr.type, &RNA_Object)) {
    target_type = &RNA_Object;
  }

  if (target_type == nullptr) {
    return false;
  }

  /* Find the containing Object. */
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Base *base = nullptr;
  const short id_type = GS(ptr.owner_id->name);
  if (id_type == ID_OB) {
    BKE_view_layer_synced_ensure(scene, view_layer);
    base = BKE_view_layer_base_find(view_layer, (Object *)ptr.owner_id);
  }
  else if (OB_DATA_SUPPORT_ID(id_type)) {
    base = blender::ed::object::find_first_by_data_id(scene, view_layer, ptr.owner_id);
  }

  bool ok = false;
  if ((base == nullptr) || ((target_type == &RNA_Bone) && (base->object->type != OB_ARMATURE))) {
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
      ok = blender::ed::object::jump_to_bone(C, base->object, bone_name, reveal_hidden);
    }
    else if (target_type == &RNA_Object) {
      ok = blender::ed::object::jump_to_object(C, base->object, reveal_hidden);
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

  const uiBut *but = UI_context_active_but_prop_get(C, &ptr, &prop, &index);

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
      const uiButSearch *search_but = (but->type == ButType::SearchMenu) ? (uiButSearch *)but :
                                                                           nullptr;

      if (search_but && search_but->items_update_fn == ui_rna_collection_search_update_fn) {
        uiRNACollectionSearch *coll_search = static_cast<uiRNACollectionSearch *>(search_but->arg);

        char str_buf[MAXBONENAME];
        char *str_ptr = RNA_property_string_get_alloc(
            &ptr, prop, str_buf, sizeof(str_buf), nullptr);

        bool found = false;
        /* Jump to target only works with search properties currently, not search callbacks yet.
         * See ui_but_add_search. */
        if (coll_search->search_prop != nullptr) {
          found = RNA_property_collection_lookup_string(
              &coll_search->search_ptr, coll_search->search_prop, str_ptr, &target_ptr);
        }

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

static wmOperatorStatus jump_to_target_button_exec(bContext *C, wmOperator * /*op*/)
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
/* EditSource Utility functions and operator,
 * NOTE: this includes utility functions and button matching checks. */

struct uiEditSourceStore {
  uiBut but_orig;
  GHash *hash;
};

struct uiEditSourceButStore {
  char py_dbg_fn[FILE_MAX];
  int py_dbg_line_number;
};

/* should only ever be set while the edit source operator is running */
static uiEditSourceStore *ui_editsource_info = nullptr;

bool UI_editsource_enable_check()
{
  return (ui_editsource_info != nullptr);
}

static void ui_editsource_active_but_set(uiBut *but)
{
  BLI_assert(ui_editsource_info == nullptr);

  ui_editsource_info = MEM_new<uiEditSourceStore>(__func__);
  ui_editsource_info->but_orig = *but;

  ui_editsource_info->hash = BLI_ghash_ptr_new(__func__);
}

static void ui_editsource_active_but_clear()
{
  BLI_ghash_free(ui_editsource_info->hash, nullptr, MEM_freeN);
  MEM_delete(ui_editsource_info);
  ui_editsource_info = nullptr;
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
      (but_a->unit_type == but_b->unit_type) && but_a->drawstr == but_b->drawstr)
  {
    return true;
  }
  return false;
}

extern void PyC_FileAndNum_Safe(const char **r_filename, int *r_lineno);

void UI_editsource_active_but_test(uiBut *but)
{

  uiEditSourceButStore *but_store = MEM_callocN<uiEditSourceButStore>(__func__);

  const char *fn;
  int line_number = -1;

#  if 0
  printf("comparing buttons: '%s' == '%s'\n", but->drawstr, ui_editsource_info->but_orig.drawstr);
#  endif

  PyC_FileAndNum_Safe(&fn, &line_number);

  if (line_number != -1) {
    STRNCPY(but_store->py_dbg_fn, fn);
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
  uiEditSourceButStore *but_store = static_cast<uiEditSourceButStore *>(
      BLI_ghash_lookup(ui_editsource_info->hash, old_but));
  if (but_store) {
    BLI_ghash_remove(ui_editsource_info->hash, old_but, nullptr, nullptr);
    BLI_ghash_insert(ui_editsource_info->hash, new_but, but_store);
  }
}

static wmOperatorStatus editsource_text_edit(bContext *C,
                                             wmOperator * /*op*/,
                                             const char filepath[FILE_MAX],
                                             const int line)
{
  wmOperatorType *ot = WM_operatortype_find("TEXT_OT_jump_to_file_at_point", true);
  PointerRNA op_props;

  WM_operator_properties_create_ptr(&op_props, ot);
  RNA_string_set(&op_props, "filepath", filepath);
  RNA_int_set(&op_props, "line", line - 1);
  RNA_int_set(&op_props, "column", 0);

  wmOperatorStatus result = WM_operator_name_call_ptr(
      C, ot, blender::wm::OpCallContext::ExecDefault, &op_props, nullptr);
  WM_operator_properties_free(&op_props);
  return result;
}

static wmOperatorStatus editsource_exec(bContext *C, wmOperator *op)
{
  uiBut *but = UI_context_active_but_get(C);

  if (but) {
    GHashIterator ghi;
    uiEditSourceButStore *but_store = nullptr;

    ARegion *region = CTX_wm_region(C);
    wmOperatorStatus ret;

    /* needed else the active button does not get tested */
    UI_screen_free_active_but_highlight(C, CTX_wm_screen(C));

    // printf("%s: begin\n", __func__);

    /* take care not to return before calling ui_editsource_active_but_clear */
    ui_editsource_active_but_set(but);

    /* redraw and get active button python info */
    ui_region_redraw_immediately(C, region);

    /* It's possible the key button referenced in `ui_editsource_info` has been freed.
     * This typically happens with popovers but could happen in other situations, see: #140439. */
    blender::Set<const uiBut *> valid_buttons_in_region;
    LISTBASE_FOREACH (uiBlock *, block_base, &region->runtime->uiblocks) {
      uiBlock *block_pair[2] = {block_base, block_base->oldblock};
      for (uiBlock *block : blender::Span(block_pair, block_pair[1] ? 2 : 1)) {
        for (int i = 0; i < block->buttons.size(); i++) {
          const uiBut *but = block->buttons[i].get();
          valid_buttons_in_region.add(but);
        }
      }
    }

    for (BLI_ghashIterator_init(&ghi, ui_editsource_info->hash);
         BLI_ghashIterator_done(&ghi) == false;
         BLI_ghashIterator_step(&ghi))
    {
      uiBut *but_key = static_cast<uiBut *>(BLI_ghashIterator_getKey(&ghi));
      if (but_key == nullptr) {
        continue;
      }

      if (!valid_buttons_in_region.contains(but_key)) {
        continue;
      }

      if (ui_editsource_uibut_match(&ui_editsource_info->but_orig, but_key)) {
        but_store = static_cast<uiEditSourceButStore *>(BLI_ghashIterator_getValue(&ghi));
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

#endif /* WITH_PYTHON */

/* -------------------------------------------------------------------- */
/** \name Reload Translation Operator
 * \{ */

static wmOperatorStatus reloadtranslation_exec(bContext * /*C*/, wmOperator * /*op*/)
{
  BLT_lang_init();
  BLF_cache_clear();
  BLT_lang_set(nullptr);
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

static wmOperatorStatus ui_button_press_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bScreen *screen = CTX_wm_screen(C);
  const bool skip_depressed = RNA_boolean_get(op->ptr, "skip_depressed");
  ARegion *region_prev = CTX_wm_region(C);
  ARegion *region = screen ? BKE_screen_find_region_xy(screen, RGN_TYPE_ANY, event->xy) : nullptr;

  if (region == nullptr) {
    region = region_prev;
  }

  if (region == nullptr) {
    return OPERATOR_PASS_THROUGH;
  }

  CTX_wm_region_set(C, region);
  uiBut *but = UI_context_active_but_get(C);
  CTX_wm_region_set(C, region_prev);

  if (but == nullptr) {
    return OPERATOR_PASS_THROUGH;
  }
  if (skip_depressed && (but->flag & (UI_SELECT | UI_SELECT_DRAW))) {
    return OPERATOR_PASS_THROUGH;
  }

  /* Weak, this is a workaround for 'UI_but_is_tool', which checks the operator type,
   * having this avoids a minor drawing glitch. */
  void *but_optype = but->optype;

  UI_but_execute(C, region, but);

  but->optype = static_cast<wmOperatorType *>(but_optype);

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

  RNA_def_boolean(ot->srna, "skip_depressed", false, "Skip Depressed", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Button Clear Operator
 * \{ */

static wmOperatorStatus button_string_clear_exec(bContext *C, wmOperator * /*op*/)
{
  uiBut *but = UI_context_active_but_get_respect_popup(C);

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
  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drop Color Operator
 * \{ */

bool UI_drop_color_poll(bContext *C, wmDrag *drag, const wmEvent * /*event*/)
{
  /* should only return true for regions that include buttons, for now
   * return true always */
  if (drag->type == WM_DRAG_COLOR) {
    SpaceImage *sima = CTX_wm_space_image(C);
    ARegion *region = CTX_wm_region(C);

    if (UI_but_active_drop_color(C)) {
      return true;
    }

    if (sima && (sima->mode == SI_MODE_PAINT) && sima->image &&
        (region && region->regiontype == RGN_TYPE_WINDOW))
    {
      return true;
    }
  }

  return false;
}

void UI_drop_color_copy(bContext * /*C*/, wmDrag *drag, wmDropBox *drop)
{
  uiDragColorHandle *drag_info = static_cast<uiDragColorHandle *>(drag->poin);

  RNA_float_set_array(drop->ptr, "color", drag_info->color);
  RNA_boolean_set(drop->ptr, "gamma", drag_info->gamma_corrected);
  RNA_boolean_set(drop->ptr, "has_alpha", drag_info->has_alpha);
}

static wmOperatorStatus drop_color_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  uiBut *but = nullptr;

  float color[4];
  RNA_float_get_array(op->ptr, "color", color);

  const bool gamma = RNA_boolean_get(op->ptr, "gamma");
  const bool has_alpha = RNA_boolean_get(op->ptr, "has_alpha");

  /* find button under mouse, check if it has RNA color property and
   * if it does copy the data */
  but = ui_region_find_active_but(region);

  if (but && but->type == ButType::Color && but->rnaprop) {
    if (!has_alpha) {
      color[3] = 1.0f;
    }

    if (RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA) {
      if (!gamma) {
        IMB_colormanagement_scene_linear_to_srgb_v3(color, color);
      }
      RNA_property_float_set_array_at_most(&but->rnapoin, but->rnaprop, color, ARRAY_SIZE(color));
      RNA_property_update(C, &but->rnapoin, but->rnaprop);
    }
    else if (RNA_property_subtype(but->rnaprop) == PROP_COLOR) {
      if (gamma) {
        IMB_colormanagement_srgb_to_scene_linear_v3(color, color);
      }
      RNA_property_float_set_array_at_most(&but->rnapoin, but->rnaprop, color, ARRAY_SIZE(color));
      RNA_property_update(C, &but->rnapoin, but->rnaprop);
    }

    if (UI_but_flag_is_set(but, UI_BUT_UNDO)) {
      ED_undo_push(C, RNA_property_ui_name(but->rnaprop));
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
  ot->poll = ED_operator_regionactive;

  ot->flag = OPTYPE_INTERNAL;

  RNA_def_float_color(
      ot->srna, "color", 4, nullptr, 0.0, FLT_MAX, "Color", "Source color", 0.0, 1.0);
  RNA_def_boolean(
      ot->srna, "gamma", false, "Gamma Corrected", "The source color is gamma corrected");
  RNA_def_boolean(
      ot->srna, "has_alpha", false, "Has Alpha", "The source color contains an Alpha component");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drop Name Operator
 * \{ */

static bool drop_name_poll(bContext *C)
{
  if (!ED_operator_regionactive(C)) {
    return false;
  }

  const uiBut *but = UI_but_active_drop_name_button(C);
  if (!but) {
    return false;
  }

  if (but->flag & UI_BUT_DISABLED) {
    return false;
  }

  return true;
}

static wmOperatorStatus drop_name_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  uiBut *but = UI_but_active_drop_name_button(C);
  std::string str = RNA_string_get(op->ptr, "string");

  ui_but_set_string_interactive(C, but, str.c_str());

  return OPERATOR_FINISHED;
}

static void UI_OT_drop_name(wmOperatorType *ot)
{
  ot->name = "Drop Name";
  ot->idname = "UI_OT_drop_name";
  ot->description = "Drop name to button";

  ot->poll = drop_name_poll;
  ot->invoke = drop_name_invoke;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  RNA_def_string(
      ot->srna, "string", nullptr, 0, "String", "The string value to drop into the button");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI List Search Operator
 * \{ */

static bool ui_list_focused_poll(bContext *C)
{
  const ARegion *region = CTX_wm_region(C);
  if (!region) {
    return false;
  }
  const wmWindow *win = CTX_wm_window(C);
  const uiList *list = UI_list_find_mouse_over(region, win->eventstate);

  return list != nullptr;
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

static wmOperatorStatus ui_list_start_filter_invoke(bContext *C,
                                                    wmOperator * /*op*/,
                                                    const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  uiList *list = UI_list_find_mouse_over(region, event);
  /* Poll should check. */
  BLI_assert(list != nullptr);

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
/** \name UI View Start Filter Operator
 * \{ */

static AbstractView *get_view_focused(bContext *C)
{
  const wmWindow *win = CTX_wm_window(C);
  if (!(win && win->eventstate)) {
    return nullptr;
  }

  const ARegion *region = CTX_wm_region(C);
  if (!region) {
    return nullptr;
  }
  return UI_region_view_find_at(region, win->eventstate->xy, 0);
}

static bool ui_view_focused_poll(bContext *C)
{
  const AbstractView *view = get_view_focused(C);
  return view != nullptr;
}

static wmOperatorStatus ui_view_start_filter_invoke(bContext *C,
                                                    wmOperator * /*op*/,
                                                    const wmEvent *event)
{
  const ARegion *region = CTX_wm_region(C);
  const blender::ui::AbstractView *hovered_view = UI_region_view_find_at(region, event->xy, 0);

  if (!hovered_view->begin_filtering(*C)) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  return OPERATOR_FINISHED;
}

static void UI_OT_view_start_filter(wmOperatorType *ot)
{
  ot->name = "View Filter";
  ot->idname = "UI_OT_view_start_filter";
  ot->description = "Start entering filter text for the data-set in focus";

  ot->invoke = ui_view_start_filter_invoke;
  ot->poll = ui_view_focused_poll;

  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI View Drop Operator
 * \{ */

static bool ui_view_drop_poll(bContext *C)
{
  const wmWindow *win = CTX_wm_window(C);
  if (!(win && win->eventstate)) {
    return false;
  }
  const ARegion *region = CTX_wm_region(C);
  if (region == nullptr) {
    return false;
  }
  return region_views_find_drop_target_at(region, win->eventstate->xy) != nullptr;
}

static wmOperatorStatus ui_view_drop_invoke(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  if (event->custom != EVT_DATA_DRAGDROP) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  ARegion *region = CTX_wm_region(C);
  std::unique_ptr<DropTargetInterface> drop_target = region_views_find_drop_target_at(region,
                                                                                      event->xy);

  if (!drop_target_apply_drop(
          *C, *region, *event, *drop_target, *static_cast<const ListBase *>(event->customdata)))
  {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  ED_region_tag_redraw(region);
  return OPERATOR_FINISHED;
}

static void UI_OT_view_drop(wmOperatorType *ot)
{
  ot->name = "View Drop";
  ot->idname = "UI_OT_view_drop";
  ot->description = "Drag and drop onto a data-set or item within the data-set";

  ot->invoke = ui_view_drop_invoke;
  ot->poll = ui_view_drop_poll;

  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI View Drop Operator
 * \{ */

static bool ui_view_scroll_poll(bContext *C)
{
  const AbstractView *view = get_view_focused(C);
  if (!view) {
    return false;
  }

  return view->supports_scrolling();
}

static wmOperatorStatus ui_view_scroll_invoke(bContext *C,
                                              wmOperator * /*op*/,
                                              const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  int type = event->type;
  bool invert_direction = false;

  if (type == MOUSEPAN) {
    int dummy_val;
    ui_pan_to_scroll(event, &type, &dummy_val);

    /* 'ui_pan_to_scroll' gives the absolute direction. */
    if (event->flag & WM_EVENT_SCROLL_INVERT) {
      invert_direction = true;
    }
  }

  AbstractView *view = get_view_focused(C);
  std::optional<ViewScrollDirection> direction =
      [type, invert_direction]() -> std::optional<ViewScrollDirection> {
    switch (type) {
      case WHEELUPMOUSE:
        return invert_direction ? ViewScrollDirection::DOWN : ViewScrollDirection::UP;
      case WHEELDOWNMOUSE:
        return invert_direction ? ViewScrollDirection::UP : ViewScrollDirection::DOWN;
      default:
        return std::nullopt;
    }
  }();
  if (!direction) {
    return OPERATOR_CANCELLED;
  }

  BLI_assert(view->supports_scrolling());
  if (view->is_fully_visible()) {
    /* The view does not need scrolling currently, so pass the event through. This allows scrolling
     * e.g. the entire region even when hovering a tree-view that supports scrolling generally. */
    return OPERATOR_PASS_THROUGH;
  }
  view->scroll(*direction);

  ED_region_tag_redraw(region);
  return OPERATOR_FINISHED;
}

static void UI_OT_view_scroll(wmOperatorType *ot)
{
  ot->name = "View Scroll";
  ot->idname = "UI_OT_view_scroll";

  ot->invoke = ui_view_scroll_invoke;
  ot->poll = ui_view_scroll_poll;

  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI View Item Rename Operator
 *
 * General purpose renaming operator for views. Thanks to this, to add a rename button to context
 * menus for example, view API users don't have to implement their own renaming operators with the
 * same logic as they already have for their #ui::AbstractViewItem::rename() override.
 *
 * \{ */

static bool ui_view_item_rename_poll(bContext *C)
{
  const ARegion *region = CTX_wm_region(C);
  if (region == nullptr) {
    return false;
  }
  const blender::ui::AbstractViewItem *active_item = UI_region_views_find_active_item(region);
  return active_item != nullptr && UI_view_item_can_rename(*active_item);
}

static wmOperatorStatus ui_view_item_rename_exec(bContext *C, wmOperator * /*op*/)
{
  ARegion *region = CTX_wm_region(C);
  blender::ui::AbstractViewItem *active_item = UI_region_views_find_active_item(region);

  UI_view_item_begin_rename(*active_item);
  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

static void UI_OT_view_item_rename(wmOperatorType *ot)
{
  ot->name = "Rename View Item";
  ot->idname = "UI_OT_view_item_rename";
  ot->description = "Rename the active item in the data-set view";

  ot->exec = ui_view_item_rename_exec;
  ot->poll = ui_view_item_rename_poll;
  /* Could get a custom tooltip via the `get_description()` callback and another overridable
   * function of the view. */

  ot->flag = OPTYPE_INTERNAL;
}

static wmOperatorStatus view_item_click_select(bContext &C,
                                               AbstractViewItem *clicked_item,
                                               const AbstractView &view,
                                               const bool extend,
                                               const bool range_select,
                                               bool wait_to_deselect_others)
{
  const bool already_selected = clicked_item && clicked_item->is_selected();

  if (extend || range_select) {
    wait_to_deselect_others = false;
  }

  if (clicked_item && already_selected && wait_to_deselect_others) {
    return OPERATOR_RUNNING_MODAL;
  }

  if (!extend) {
    view.foreach_view_item([](AbstractViewItem &item) { item.set_selected(false); });
  }

  if (clicked_item == nullptr) {
    /* Only clear selection (if needed). */
    return OPERATOR_FINISHED;
  }

  if (range_select) {
    bool is_inside_range = false;
    view.foreach_view_item([&](AbstractViewItem &item) {
      if (item.is_active() ^ (&item == clicked_item)) {
        is_inside_range = !is_inside_range;
        /* Select end items from the range. */
        item.set_selected(true);
      }
      if (is_inside_range) {
        /* Select items within the range. */
        item.set_selected(true);
      }
    });
    return OPERATOR_FINISHED;
  }

  clicked_item->activate(C);

  return OPERATOR_FINISHED;
}

static std::pair<AbstractView *, AbstractViewItem *> select_operator_view_and_item_find_xy(
    const ARegion &region, const wmOperator &op)
{
  /* Mouse coordinates in window space. */
  int window_xy[2];
  {
    /* Mouse coordinates in region space. */
    int region_xy[2];
    region_xy[0] = RNA_int_get(op.ptr, "mouse_x");
    region_xy[1] = RNA_int_get(op.ptr, "mouse_y");
    ui_region_to_window(&region, region_xy[0], region_xy[1], &window_xy[0], &window_xy[1]);
  }

  AbstractView *view = UI_region_view_find_at(&region, window_xy, 0);
  AbstractViewItem *item = UI_region_views_find_item_at(region, window_xy);
  BLI_assert(!item || &item->get_view() == view);

  return std::make_pair(view, item);
}

static wmOperatorStatus ui_view_item_select_exec(bContext *C, wmOperator *op)
{
  ARegion &region = *CTX_wm_region(C);
  auto [view, clicked_item] = select_operator_view_and_item_find_xy(region, *op);

  if (!view) {
    return OPERATOR_CANCELLED;
  }

  const bool is_multiselect = view->is_multiselect_supported();
  const bool extend = RNA_boolean_get(op->ptr, "extend") && is_multiselect;
  const bool range_select = RNA_boolean_get(op->ptr, "range_select") && is_multiselect;
  const bool wait_to_deselect_others = RNA_boolean_get(op->ptr, "wait_to_deselect_others");

  const wmOperatorStatus status = view_item_click_select(
      *C, clicked_item, *view, extend, range_select, wait_to_deselect_others);

  ED_region_tag_redraw(&region);

  return status;
}

static wmOperatorStatus ui_view_item_select_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  const ARegion &region = *CTX_wm_region(C);
  const AbstractViewItem *clicked_item = UI_region_views_find_item_at(region, event->xy);

  /* Wait with selecting to see if there's a click or drag event, if requested by the view item. */
  if (clicked_item && clicked_item->is_select_on_click()) {
    RNA_boolean_set(op->ptr, "use_select_on_click", true);
  }

  return WM_generic_select_invoke(C, op, event);
}

static void UI_OT_view_item_select(wmOperatorType *ot)
{
  ot->name = "Select View Item";
  ot->idname = "UI_OT_view_item_select";
  ot->description = "Activate selected view item";

  ot->exec = ui_view_item_select_exec;
  ot->invoke = ui_view_item_select_invoke;
  ot->modal = WM_generic_select_modal;
  ot->poll = ui_view_focused_poll;

  ot->flag = OPTYPE_INTERNAL;

  WM_operator_properties_generic_select(ot);
  PropertyRNA *prop = RNA_def_boolean(ot->srna, "extend", false, "extend", "Extend Selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "range_select",
                         false,
                         "Range Select",
                         "Select all between clicked and active items");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static wmOperatorStatus ui_view_item_delete_invoke(bContext *C,
                                                   wmOperator * /*op*/,
                                                   const wmEvent * /*event*/)
{
  AbstractView *view = get_view_focused(C);

  view->foreach_view_item([&](AbstractViewItem &item) {
    if (item.is_active() || item.is_selected()) {
      item.delete_item(C);
    }
  });

  return OPERATOR_FINISHED;
}

static void UI_OT_view_item_delete(wmOperatorType *ot)
{
  ot->name = "Delete";
  ot->idname = "UI_OT_view_item_delete";
  ot->description = "Delete selected list item";

  ot->invoke = ui_view_item_delete_invoke;
  ot->poll = ui_view_focused_poll;

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
  const Object *ob = static_cast<const Object *>(ptr.data);
  if (ob == nullptr) {
    return false;
  }

  PointerRNA mat_slot = CTX_data_pointer_get_type(C, "material_slot", &RNA_MaterialSlot);
  if (RNA_pointer_is_null(&mat_slot)) {
    return false;
  }

  return true;
}

static wmOperatorStatus ui_drop_material_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  Material *ma = (Material *)WM_operator_properties_id_lookup_from_name_or_session_uid(
      bmain, op->ptr, ID_MA);
  if (ma == nullptr) {
    return OPERATOR_CANCELLED;
  }

  PointerRNA ptr = CTX_data_pointer_get_type(C, "object", &RNA_Object);
  Object *ob = static_cast<Object *>(ptr.data);
  BLI_assert(ob);

  PointerRNA mat_slot = CTX_data_pointer_get_type(C, "material_slot", &RNA_MaterialSlot);
  BLI_assert(mat_slot.data);
  const int target_slot = RNA_int_get(&mat_slot, "slot_index") + 1;

  /* only drop grease pencil material on grease pencil objects */
  if ((ma->gp_style != nullptr) && (ob->type != OB_GREASE_PENCIL)) {
    return OPERATOR_CANCELLED;
  }

  BKE_object_material_assign(bmain, ob, ma, target_slot, BKE_MAT_ASSIGN_USERPREF);

  WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, ob);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
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

  WM_operator_properties_id_lookup(ot, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator & Keymap Registration
 * \{ */

void ED_operatortypes_ui()
{
  using namespace blender::ui;
  WM_operatortype_append(UI_OT_copy_data_path_button);
  WM_operatortype_append(UI_OT_copy_as_driver_button);
  WM_operatortype_append(UI_OT_copy_python_command_button);
  WM_operatortype_append(UI_OT_reset_default_button);
  WM_operatortype_append(UI_OT_assign_default_button);
  WM_operatortype_append(UI_OT_unset_property_button);
  WM_operatortype_append(UI_OT_copy_to_selected_button);
  WM_operatortype_append(UI_OT_copy_driver_to_selected_button);
  WM_operatortype_append(UI_OT_jump_to_target_button);
  WM_operatortype_append(UI_OT_drop_color);
  WM_operatortype_append(UI_OT_drop_name);
  WM_operatortype_append(UI_OT_drop_material);
#ifdef WITH_PYTHON
  WM_operatortype_append(UI_OT_editsource);
#endif
  WM_operatortype_append(UI_OT_reloadtranslation);
  WM_operatortype_append(UI_OT_button_execute);
  WM_operatortype_append(UI_OT_button_string_clear);

  WM_operatortype_append(UI_OT_list_start_filter);

  WM_operatortype_append(UI_OT_view_start_filter);
  WM_operatortype_append(UI_OT_view_drop);
  WM_operatortype_append(UI_OT_view_scroll);
  WM_operatortype_append(UI_OT_view_item_rename);
  WM_operatortype_append(UI_OT_view_item_select);
  WM_operatortype_append(UI_OT_view_item_delete);

  WM_operatortype_append(UI_OT_override_add_button);
  WM_operatortype_append(UI_OT_override_remove_button);
  WM_operatortype_append(UI_OT_override_idtemplate_make);
  WM_operatortype_append(UI_OT_override_idtemplate_reset);
  WM_operatortype_append(UI_OT_override_idtemplate_clear);
  override_idtemplate_menu();

  /* external */
  WM_operatortype_append(UI_OT_eyedropper_color);
  WM_operatortype_append(UI_OT_eyedropper_colorramp);
  WM_operatortype_append(UI_OT_eyedropper_colorramp_point);
  WM_operatortype_append(UI_OT_eyedropper_id);
  WM_operatortype_append(UI_OT_eyedropper_depth);
  WM_operatortype_append(UI_OT_eyedropper_driver);
  WM_operatortype_append(UI_OT_eyedropper_bone);
  WM_operatortype_append(UI_OT_eyedropper_grease_pencil_color);
}

void ED_keymap_ui(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "User Interface", SPACE_EMPTY, RGN_TYPE_WINDOW);

  eyedropper_modal_keymap(keyconf);
  eyedropper_colorband_modal_keymap(keyconf);
}

/** \} */
