/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup RNA
 */

#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "rna_internal.h" /* own include */

#ifdef RNA_RUNTIME

#  include "BKE_context.h"
#  include "BKE_report.h"
#  include "BKE_nla.h"

#  include "ED_keyframing.h"

static void rna_KeyingSet_context_refresh(KeyingSet *ks, bContext *C, ReportList *reports)
{
  /* TODO: enable access to providing a list of overrides (dsources)? */
  int success = ANIM_validate_keyingset(C, NULL, ks);

  if (success != 0) {
    switch (success) {
      case MODIFYKEY_INVALID_CONTEXT:
        BKE_report(reports, RPT_ERROR, "Invalid context for keying set");
        break;

      case MODIFYKEY_MISSING_TYPEINFO:
        BKE_report(
            reports, RPT_ERROR, "Incomplete built-in keying set, appears to be missing type info");
        break;
    }
  }
}

static float rna_AnimData_nla_tweak_strip_time_to_scene(AnimData *adt, float frame, bool invert)
{
  return BKE_nla_tweakedit_remap(adt, frame, invert ? NLATIME_CONVERT_UNMAP : NLATIME_CONVERT_MAP);
}

#else

void RNA_api_keyingset(StructRNA *srna)
{
  FunctionRNA *func;
  /*PropertyRNA *parm; */

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
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  RNA_def_boolean(func, "invert", false, "Invert", "Convert scene time to action time");
  parm = RNA_def_float(
      func, "result", 0.0, MINAFRAME, MAXFRAME, "", "Converted time", MINAFRAME, MAXFRAME);
  RNA_def_function_return(func, parm);
}

#endif
