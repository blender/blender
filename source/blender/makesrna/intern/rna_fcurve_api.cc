/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "BLI_utildefines.h"

#include "RNA_define.hh"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "ANIM_fcurve.hh"

#include "rna_internal.hh" /* own include */

#ifdef RNA_RUNTIME

#  include <stddef.h>

#  include "BKE_fcurve.hh"

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

static void rna_FCurve_bake(FCurve *fcu,
                            ReportList *reports,
                            int start_frame,
                            int end_frame,
                            float step,
                            int remove_existing_as_int)
{
  using namespace blender::animrig;
  if (start_frame >= end_frame) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Invalid frame range (%d - %d). End Frame is larger than Start Frame",
                start_frame,
                end_frame);
    return;
  }

  const BakeCurveRemove remove_existing = BakeCurveRemove(remove_existing_as_int);
  bake_fcurve(fcu, {start_frame, end_frame}, step, remove_existing);
  WM_main_add_notifier(NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);
}

#else

static const EnumPropertyItem channel_bake_remove_options[] = {
    {int(blender::animrig::BakeCurveRemove::NONE), "NONE", 0, "None", "Keep all keys"},
    {int(blender::animrig::BakeCurveRemove::IN_RANGE),
     "IN_RANGE",
     0,
     "In Range",
     "Remove all keys within the defined range"},
    {int(blender::animrig::BakeCurveRemove::OUT_RANGE),
     "OUT_RANGE",
     0,
     "Outside Range",
     "Remove all keys outside the defined range"},
    {int(blender::animrig::BakeCurveRemove::ALL), "ALL", 0, "All", "Remove all existing keys"},
    {0, nullptr, 0, nullptr, nullptr},
};

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

  func = RNA_def_function(srna, "bake", "rna_FCurve_bake");
  RNA_def_function_ui_description(func, "Place keys at even intervals on the existing curve.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(func,
                     "start",
                     0,
                     MINAFRAME,
                     MAXFRAME,
                     "Start Frame",
                     "Frame at which to start baking",
                     MINAFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "end",
                     0,
                     MINAFRAME,
                     MAXFRAME,
                     "End Frame",
                     "Frame at which to end baking (inclusive)",
                     MINAFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_float(
      func, "step", 1, 0.01, FLT_MAX, "Step", "At which interval to add keys", 1, 16);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_PYFUNC_OPTIONAL);
  RNA_def_enum(func,
               "remove",
               channel_bake_remove_options,
               int(blender::animrig::BakeCurveRemove::IN_RANGE),
               "Remove Options",
               "Choose which keys should be automatically removed by the bake");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_PYFUNC_OPTIONAL);
}

void RNA_api_drivers(StructRNA * /*srna*/)
{
  // FunctionRNA *func;
  // PropertyRNA *parm;
}

#endif
