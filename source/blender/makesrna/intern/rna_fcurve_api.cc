/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "BLI_utildefines.h"

#include "RNA_define.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "rna_internal.h" /* own include */

#ifdef RNA_RUNTIME

#  include <stddef.h>

#  include "BKE_fcurve.h"

#  include "BLI_math.h"

static void rna_FCurve_convert_to_samples(FCurve *fcu, ReportList *reports, int start, int end)
{
  /* XXX fcurve_store_samples uses end frame included,
   * which is not consistent with usual behavior in Blender,
   * nor python slices, etc. Let have public py API be consistent here at least. */
  end--;
  if (start > end) {
    BKE_reportf(reports, RPT_ERROR, "Invalid frame range (%d - %d)", start, end + 1);
  }
  else if (fcu->fpt) {
    BKE_report(reports, RPT_WARNING, "F-Curve already has sample points");
  }
  else if (!fcu->bezt) {
    BKE_report(reports, RPT_WARNING, "F-Curve has no keyframes");
  }
  else {
    fcurve_store_samples(fcu, nullptr, start, end, fcurve_samplingcb_evalcurve);
    WM_main_add_notifier(NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);
  }
}

static void rna_FCurve_convert_to_keyframes(FCurve *fcu, ReportList *reports, int start, int end)
{
  if (start >= end) {
    BKE_reportf(reports, RPT_ERROR, "Invalid frame range (%d - %d)", start, end);
  }
  else if (fcu->bezt) {
    BKE_report(reports, RPT_WARNING, "F-Curve already has keyframes");
  }
  else if (!fcu->fpt) {
    BKE_report(reports, RPT_WARNING, "F-Curve has no sample points");
  }
  else {
    fcurve_samples_to_keyframes(fcu, start, end);
    WM_main_add_notifier(NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);
  }
}

#else

void RNA_api_fcurves(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "convert_to_samples", "rna_FCurve_convert_to_samples");
  RNA_def_function_ui_description(
      func, "Convert current FCurve from keyframes to sample points, if necessary");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "start", 0, MINAFRAME, MAXFRAME, "Start Frame", "", MINAFRAME, MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "end", 0, MINAFRAME, MAXFRAME, "End Frame", "", MINAFRAME, MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "convert_to_keyframes", "rna_FCurve_convert_to_keyframes");
  RNA_def_function_ui_description(
      func,
      "Convert current FCurve from sample points to keyframes (linear interpolation), "
      "if necessary");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "start", 0, MINAFRAME, MAXFRAME, "Start Frame", "", MINAFRAME, MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "end", 0, MINAFRAME, MAXFRAME, "End Frame", "", MINAFRAME, MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_api_drivers(StructRNA * /*srna*/)
{
  /*  FunctionRNA *func; */
  /*  PropertyRNA *parm; */
}

#endif