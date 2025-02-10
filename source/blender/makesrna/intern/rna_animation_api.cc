/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "DNA_scene_types.h"

#include "rna_internal.hh" /* own include */

#ifdef RNA_RUNTIME

#  include "BKE_animsys.h"
#  include "BKE_context.hh"
#  include "BKE_nla.hh"
#  include "BKE_report.hh"

#  include "ANIM_keyingsets.hh"
#  include "ED_keyframing.hh"

static void rna_KeyingSet_context_refresh(KeyingSet *ks, bContext *C, ReportList *reports)
{
  using namespace blender::animrig;
  /* TODO: enable access to providing a list of overrides (dsources)? */
  const ModifyKeyReturn error = validate_keyingset(C, nullptr, ks);

  if (error == ModifyKeyReturn::SUCCESS) {
    return;
  }

  switch (error) {
    case ModifyKeyReturn::INVALID_CONTEXT:
      BKE_report(reports, RPT_ERROR, "Invalid context for keying set");
      break;

    case ModifyKeyReturn::MISSING_TYPEINFO:
      BKE_report(
          reports, RPT_ERROR, "Incomplete built-in keying set, appears to be missing type info");
      break;

    default:
      break;
  }
}

static float rna_AnimData_nla_tweak_strip_time_to_scene(AnimData *adt, float frame, bool invert)
{
  return BKE_nla_tweakedit_remap(adt, frame, invert ? NLATIME_CONVERT_UNMAP : NLATIME_CONVERT_MAP);
}

void rna_id_animdata_fix_paths_rename_all(ID *id,
                                          AnimData * /*adt*/,
                                          Main *bmain,
                                          const char *prefix,
                                          const char *oldName,
                                          const char *newName)
{
  BKE_animdata_fix_paths_rename_all_ex(bmain, id, prefix, oldName, newName, 0, 0, true);
}

#else

void RNA_api_keyingset(StructRNA *srna)
{
  FunctionRNA *func;
  // PropertyRNA *parm;

  /* validate relative Keying Set (used to ensure paths are ok for context) */
  func = RNA_def_function(srna, "refresh", "rna_KeyingSet_context_refresh");
  RNA_def_function_ui_description(
      func,
      "Refresh Keying Set to ensure that it is valid for the current context "
      "(call before each use of one)");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
}

void RNA_api_animdata(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  /* Convert between action time and scene time when tweaking a NLA strip. */
  func = RNA_def_function(
      srna, "nla_tweak_strip_time_to_scene", "rna_AnimData_nla_tweak_strip_time_to_scene");
  RNA_def_function_ui_description(func,
                                  "Convert a time value from the local time of the tweaked strip "
                                  "to scene time, exactly as done by built-in key editing tools. "
                                  "Returns the input time unchanged if not tweaking.");
  parm = RNA_def_float(
      func, "frame", 0.0, MINAFRAME, MAXFRAME, "", "Input time", MINAFRAME, MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func, "invert", false, "Invert", "Convert scene time to action time");
  parm = RNA_def_float(
      func, "result", 0.0, MINAFRAME, MAXFRAME, "", "Converted time", MINAFRAME, MAXFRAME);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "fix_paths_rename_all", "rna_id_animdata_fix_paths_rename_all");
  RNA_def_string(func, "prefix", nullptr, MAX_IDPROP_NAME, "Prefix", "Name prefix");
  RNA_def_string(func, "old_name", nullptr, MAX_IDPROP_NAME, "Old Name", "Old name");
  RNA_def_string(func, "new_name", nullptr, MAX_IDPROP_NAME, "New Name", "New name");
  RNA_def_function_ui_description(
      func,
      "Rename the property paths in the animation system, since properties are animated via "
      "string paths, it's needed to keep them valid after properties has been renamed");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_SELF_ID);
}

#endif
