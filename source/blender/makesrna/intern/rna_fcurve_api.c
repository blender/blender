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
#include <string.h>
#include <time.h>

#include "BLI_utildefines.h"

#include "RNA_define.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "rna_internal.h" /* own include */

#ifdef RNA_RUNTIME

#  include <stddef.h>

#  include "BKE_animsys.h"
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
    BKE_report(reports, RPT_WARNING, "FCurve has already sample points");
  }
  else if (!fcu->bezt) {
    BKE_report(reports, RPT_WARNING, "FCurve has no keyframes");
  }
  else {
    fcurve_store_samples(fcu, NULL, start, end, fcurve_samplingcb_evalcurve);
    WM_main_add_notifier(NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
  }
}

static void rna_FCurve_convert_to_keyframes(FCurve *fcu, ReportList *reports, int start, int end)
{
  if (start >= end) {
    BKE_reportf(reports, RPT_ERROR, "Invalid frame range (%d - %d)", start, end);
  }
  else if (fcu->bezt) {
    BKE_report(reports, RPT_WARNING, "FCurve has already keyframes");
  }
  else if (!fcu->fpt) {
    BKE_report(reports, RPT_WARNING, "FCurve has no sample points");
  }
  else {
    BezTriple *bezt;
    FPoint *fpt = fcu->fpt;
    int tot_kf = end - start;
    int tot_sp = fcu->totvert;

    bezt = fcu->bezt = MEM_callocN(sizeof(*fcu->bezt) * (size_t)tot_kf, __func__);
    fcu->totvert = tot_kf;

    /* Get first sample point to 'copy' as keyframe. */
    for (; tot_sp && (fpt->vec[0] < (float)start); fpt++, tot_sp--) {
      /* pass */
    }

    /* Add heading dummy flat points if needed. */
    for (; tot_kf && (fpt->vec[0] > (float)start); start++, bezt++, tot_kf--) {
      /* Linear interpolation, of course. */
      bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
      bezt->ipo = BEZT_IPO_LIN;
      bezt->h1 = bezt->h2 = HD_AUTO_ANIM;
      bezt->vec[1][0] = (float)start;
      bezt->vec[1][1] = fpt->vec[1];
    }

    /* Copy actual sample points. */
    for (; tot_kf && tot_sp; start++, bezt++, tot_kf--, fpt++, tot_sp--) {
      /* Linear interpolation, of course. */
      bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
      bezt->ipo = BEZT_IPO_LIN;
      bezt->h1 = bezt->h2 = HD_AUTO_ANIM;
      copy_v2_v2(bezt->vec[1], fpt->vec);
    }

    /* Add leading dummy flat points if needed. */
    for (fpt--; tot_kf; start++, bezt++, tot_kf--) {
      /* Linear interpolation, of course. */
      bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
      bezt->ipo = BEZT_IPO_LIN;
      bezt->h1 = bezt->h2 = HD_AUTO_ANIM;
      bezt->vec[1][0] = (float)start;
      bezt->vec[1][1] = fpt->vec[1];
    }

    MEM_SAFE_FREE(fcu->fpt);

    /* Not strictly needed since we use linear interpolation, but better be consistent here. */
    calchandles_fcurve(fcu);
    WM_main_add_notifier(NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
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
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func, "end", 0, MINAFRAME, MAXFRAME, "End Frame", "", MINAFRAME, MAXFRAME);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  func = RNA_def_function(srna, "convert_to_keyframes", "rna_FCurve_convert_to_keyframes");
  RNA_def_function_ui_description(
      func,
      "Convert current FCurve from sample points to keyframes (linear interpolation), "
      "if necessary");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "start", 0, MINAFRAME, MAXFRAME, "Start Frame", "", MINAFRAME, MAXFRAME);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func, "end", 0, MINAFRAME, MAXFRAME, "End Frame", "", MINAFRAME, MAXFRAME);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
}

void RNA_api_drivers(StructRNA *UNUSED(srna))
{
  /*  FunctionRNA *func; */
  /*  PropertyRNA *parm; */
}

#endif
