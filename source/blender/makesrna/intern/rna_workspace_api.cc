/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>
#include <cstring>
#include <ctime>

#include "RNA_define.hh"

#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "RNA_enum_types.hh" /* own include */

#include "rna_internal.hh" /* own include */

#ifdef RNA_RUNTIME

#  include "BLI_string.h"

#  include "BKE_paint.hh"
#  include "BKE_report.hh"

#  include "ED_screen.hh"

#  include "WM_api.hh"
#  include "WM_toolsystem.hh"

static void rna_WorkSpaceTool_setup(ID *id,
                                    bToolRef *tref,
                                    bContext *C,
                                    const char *idname,
                                    /* Args for: 'bToolRef_Runtime'. */
                                    int cursor,
                                    const char *keymap,
                                    const char *gizmo_group,
                                    int brush_type,
                                    const char *data_block,
                                    const char *op_idname,
                                    int index,
                                    int options,
                                    const char *idname_fallback,
                                    const char *keymap_fallback)
{
  bToolRef_Runtime tref_rt = {0};

  tref_rt.cursor = cursor;
  STRNCPY(tref_rt.keymap, keymap);
  STRNCPY(tref_rt.gizmo_group, gizmo_group);
  STRNCPY(tref_rt.data_block, data_block);
  STRNCPY(tref_rt.op, op_idname);
  tref_rt.brush_type = brush_type;
  tref_rt.index = index;
  tref_rt.flag = options;

  /* While it's logical to assign both these values from setup,
   * it's useful to stored this in DNA for re-use, exceptional case: write to the 'tref'. */
  STRNCPY(tref->idname_fallback, idname_fallback);
  STRNCPY(tref_rt.keymap_fallback, keymap_fallback);

  WM_toolsystem_ref_set_from_runtime(C, (WorkSpace *)id, tref, &tref_rt, idname);
}

static void rna_WorkSpaceTool_refresh_from_context(ID *id, bToolRef *tref, Main *bmain)
{
  WM_toolsystem_ref_sync_from_context(bmain, (WorkSpace *)id, tref);
}

static PointerRNA rna_WorkSpaceTool_operator_properties(bToolRef *tref,
                                                        ReportList *reports,
                                                        const char *idname)
{
  wmOperatorType *ot = WM_operatortype_find(idname, true);

  if (ot != nullptr) {
    PointerRNA ptr;
    WM_toolsystem_ref_properties_ensure_from_operator(tref, ot, &ptr);
    return ptr;
  }

  BKE_reportf(reports, RPT_ERROR, "Operator '%s' not found!", idname);
  return PointerRNA_NULL;
}

static PointerRNA rna_WorkSpaceTool_gizmo_group_properties(bToolRef *tref,
                                                           ReportList *reports,
                                                           const char *idname)
{
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
  if (gzgt != nullptr) {
    PointerRNA ptr;
    WM_toolsystem_ref_properties_ensure_from_gizmo_group(tref, gzgt, &ptr);
    return ptr;
  }
  BKE_reportf(reports, RPT_ERROR, "Gizmo group '%s' not found!", idname);

  return PointerRNA_NULL;
}

#else

void RNA_api_workspace(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "status_text_set_internal", "ED_workspace_status_text");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(
      func, "Set the status bar text, typically key shortcuts for modal operators");
  parm = RNA_def_string(
      func, "text", nullptr, 0, "Text", "New string for the status bar, None clears the text");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_property_clear_flag(parm, PROP_NEVER_NULL);
}

void RNA_api_workspace_tool(StructRNA *srna)
{
  PropertyRNA *parm;
  FunctionRNA *func;

  static const EnumPropertyItem options_items[] = {
      {TOOLREF_FLAG_FALLBACK_KEYMAP, "KEYMAP_FALLBACK", 0, "Fallback", ""},
      {TOOLREF_FLAG_USE_BRUSHES,
       "USE_BRUSHES",
       0,
       "Uses Brushes",
       "Allow this tool to use brushes via the asset system"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  func = RNA_def_function(srna, "setup", "rna_WorkSpaceTool_setup");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Set the tool settings");

  parm = RNA_def_string(func, "idname", nullptr, MAX_NAME, "Identifier", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* 'bToolRef_Runtime' */
  parm = RNA_def_property(func, "cursor", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(parm, rna_enum_window_cursor_items);
  RNA_def_string(func, "keymap", nullptr, KMAP_MAX_NAME, "Key Map", "");
  RNA_def_string(func, "gizmo_group", nullptr, MAX_NAME, "Gizmo Group", "");
  parm = RNA_def_property(func, "brush_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(parm, rna_enum_dummy_NULL_items);
  RNA_def_property_enum_funcs(parm, nullptr, nullptr, "rna_WorkSpaceTool_brush_type_itemf");
  RNA_def_property_enum_default(parm, -1);
  RNA_def_property_ui_text(parm, "Brush Type", "Limit this tool to a specific type of brush");
  RNA_def_string(func, "data_block", nullptr, MAX_NAME, "Data Block", "");
  RNA_def_string(func, "operator", nullptr, MAX_NAME, "Operator", "");
  RNA_def_int(func, "index", 0, INT_MIN, INT_MAX, "Index", "", INT_MIN, INT_MAX);
  RNA_def_enum_flag(func, "options", options_items, 0, "Tool Options", "");

  RNA_def_string(func, "idname_fallback", nullptr, MAX_NAME, "Fallback Identifier", "");
  RNA_def_string(func, "keymap_fallback", nullptr, KMAP_MAX_NAME, "Fallback Key Map", "");

  /* Access tool operator options (optionally create). */
  func = RNA_def_function(srna, "operator_properties", "rna_WorkSpaceTool_operator_properties");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "operator", nullptr, 0, "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return */
  parm = RNA_def_pointer(func, "result", "OperatorProperties", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  /* Access gizmo-group options (optionally create). */
  func = RNA_def_function(
      srna, "gizmo_group_properties", "rna_WorkSpaceTool_gizmo_group_properties");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "group", nullptr, 0, "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return */
  parm = RNA_def_pointer(func, "result", "GizmoGroupProperties", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "refresh_from_context", "rna_WorkSpaceTool_refresh_from_context");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
}

#endif
