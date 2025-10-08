/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"

#include "rna_internal.hh" /* own include */

#ifdef RNA_RUNTIME

#  include "BLT_translation.hh"

#  include "BKE_context.hh"
#  include "BKE_idprop.hh"
#  include "BKE_report.hh"

#  include "UI_interface.hh"

#  include "ED_gizmo_library.hh"

static void rna_gizmo_draw_preset_box(wmGizmo *gz, const float matrix[16], int select_id)
{
  ED_gizmo_draw_preset_box(gz, (const float (*)[4])matrix, select_id);
}

static void rna_gizmo_draw_preset_arrow(wmGizmo *gz,
                                        const float matrix[16],
                                        int axis,
                                        int select_id)
{
  ED_gizmo_draw_preset_arrow(gz, (const float (*)[4])matrix, axis, select_id);
}

static void rna_gizmo_draw_preset_circle(wmGizmo *gz,
                                         const float matrix[16],
                                         int axis,
                                         int select_id)
{
  ED_gizmo_draw_preset_circle(gz, (const float (*)[4])matrix, axis, select_id);
}

/* -------------------------------------------------------------------- */
/** \name Gizmo Property Define
 * \{ */

static void rna_gizmo_target_set_prop(wmGizmo *gz,
                                      ReportList *reports,
                                      const char *target_propname,
                                      PointerRNA *ptr,
                                      const char *propname,
                                      int index)
{
  const wmGizmoPropertyType *gz_prop_type = WM_gizmotype_target_property_find(gz->type,
                                                                              target_propname);
  if (gz_prop_type == nullptr) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Gizmo target property '%s.%s' not found",
                gz->type->idname,
                target_propname);
    return;
  }

  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
  if (prop == nullptr) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Property '%s.%s' not found",
                RNA_struct_identifier(ptr->type),
                propname);
    return;
  }

  if (gz_prop_type->data_type != RNA_property_type(prop)) {
    const int gizmo_type_index = RNA_enum_from_value(rna_enum_property_type_items,
                                                     gz_prop_type->data_type);
    const int prop_type_index = RNA_enum_from_value(rna_enum_property_type_items,
                                                    RNA_property_type(prop));
    BLI_assert((gizmo_type_index != -1) && (prop_type_index == -1));

    BKE_reportf(reports,
                RPT_ERROR,
                "Gizmo target '%s.%s' expects '%s', '%s.%s' is '%s'",
                gz->type->idname,
                target_propname,
                rna_enum_property_type_items[gizmo_type_index].identifier,
                RNA_struct_identifier(ptr->type),
                propname,
                rna_enum_property_type_items[prop_type_index].identifier);
    return;
  }

  if (RNA_property_array_check(prop)) {
    if (index == -1) {
      const int prop_array_length = RNA_property_array_length(ptr, prop);
      if (gz_prop_type->array_length != prop_array_length) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Gizmo target property '%s.%s' expects an array of length %d, found %d",
                    gz->type->idname,
                    target_propname,
                    gz_prop_type->array_length,
                    prop_array_length);
        return;
      }
    }
  }
  else {
    if (gz_prop_type->array_length != 1) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Gizmo target property '%s.%s' expects an array of length %d",
                  gz->type->idname,
                  target_propname,
                  gz_prop_type->array_length);
      return;
    }
  }

  if (index >= gz_prop_type->array_length) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Gizmo target property '%s.%s', index %d must be below %d",
                gz->type->idname,
                target_propname,
                index,
                gz_prop_type->array_length);
    return;
  }

  WM_gizmo_target_property_def_rna_ptr(gz, gz_prop_type, ptr, prop, index);
}

static PointerRNA rna_gizmo_target_set_operator(wmGizmo *gz,
                                                ReportList *reports,
                                                const char *opname,
                                                int part_index)
{
  wmOperatorType *ot;

  ot = WM_operatortype_find(opname, false); /* print error next */
  if (!ot || !ot->srna) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s '%s'",
                ot ? RPT_("Operator missing srna") : RPT_("Unknown operator"),
                opname);
    return PointerRNA_NULL;
  }

  /* For the return value to be usable, we need 'PointerRNA.data' to be set. */
  IDProperty *properties = blender::bke::idprop::create_group("wmGizmoProperties").release();

  return *WM_gizmo_operator_set(gz, part_index, ot, properties);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gizmo Property Access
 * \{ */

static bool rna_gizmo_target_is_valid(wmGizmo *gz,
                                      ReportList *reports,
                                      const char *target_propname)
{
  wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, target_propname);
  if (gz_prop == nullptr) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Gizmo target property '%s.%s' not found",
                gz->type->idname,
                target_propname);
    return false;
  }
  return WM_gizmo_target_property_is_valid(gz_prop);
}

/** \} */

#else

void RNA_api_gizmo(StructRNA *srna)
{
  /* Utility draw functions, since we don't expose new OpenGL drawing wrappers via Python yet.
   * exactly how these should be exposed isn't totally clear.
   * However it's probably good to have some high level API's for this anyway.
   * Just note that this could be re-worked once tests are done.
   */

  FunctionRNA *func;
  PropertyRNA *parm;

  /* -------------------------------------------------------------------- */
  /* Primitive Shapes */

  /* draw_preset_box */
  func = RNA_def_function(srna, "draw_preset_box", "rna_gizmo_draw_preset_box");
  RNA_def_function_ui_description(func, "Draw a box");
  parm = RNA_def_property(func, "matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(parm, "", "The matrix to transform");
  RNA_def_int(func,
              "select_id",
              -1,
              -1,
              INT_MAX,
              "ID to use when gizmo is selectable. Use -1 when not selecting.",
              "",
              -1,
              INT_MAX);

  /* draw_preset_box */
  func = RNA_def_function(srna, "draw_preset_arrow", "rna_gizmo_draw_preset_arrow");
  RNA_def_function_ui_description(func, "Draw a box");
  parm = RNA_def_property(func, "matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(parm, "", "The matrix to transform");
  RNA_def_enum(func, "axis", rna_enum_object_axis_items, 2, "", "Arrow Orientation");
  RNA_def_int(func,
              "select_id",
              -1,
              -1,
              INT_MAX,
              "ID to use when gizmo is selectable. Use -1 when not selecting.",
              "",
              -1,
              INT_MAX);

  func = RNA_def_function(srna, "draw_preset_circle", "rna_gizmo_draw_preset_circle");
  RNA_def_function_ui_description(func, "Draw a box");
  parm = RNA_def_property(func, "matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(parm, "", "The matrix to transform");
  RNA_def_enum(func, "axis", rna_enum_object_axis_items, 2, "", "Arrow Orientation");
  RNA_def_int(func,
              "select_id",
              -1,
              -1,
              INT_MAX,
              "ID to use when gizmo is selectable. Use -1 when not selecting.",
              "",
              -1,
              INT_MAX);

  /* -------------------------------------------------------------------- */
  /* Other Shapes */

  /* -------------------------------------------------------------------- */
  /* Property API */

  /* Define Properties */
  /* NOTE: 'target_set_handler' is defined in `bpy_rna_gizmo.cc`. */
  func = RNA_def_function(srna, "target_set_prop", "rna_gizmo_target_set_prop");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "");
  parm = RNA_def_string(func, "target", nullptr, 0, "", "Target property");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* similar to UILayout.prop */
  parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_string(func, "property", nullptr, 0, "", "Identifier of property in data");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func, "index", -1, -1, INT_MAX, "", "", -1, INT_MAX); /* RNA_NO_INDEX == -1 */

  func = RNA_def_function(srna, "target_set_operator", "rna_gizmo_target_set_operator");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func,
                                  "Operator to run when activating the gizmo "
                                  "(overrides property targets)");
  parm = RNA_def_string(func, "operator", nullptr, 0, "", "Target operator");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func, "index", 0, 0, 255, "Part index", "", 0, 255);

  /* similar to UILayout.operator */
  parm = RNA_def_pointer(
      func, "properties", "OperatorProperties", "", "Operator properties to fill in");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  /* Access Properties */
  /* NOTE: 'target_get', 'target_set' is defined in `bpy_rna_gizmo.cc`. */
  func = RNA_def_function(srna, "target_is_valid", "rna_gizmo_target_is_valid");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "property", nullptr, 0, "", "Property identifier");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_function_ui_description(func, "");
  parm = RNA_def_boolean(func, "result", false, "", "");
  RNA_def_function_return(func, parm);
}

void RNA_api_gizmogroup(StructRNA * /*srna*/)
{
  /* nothing yet */
}

#endif
