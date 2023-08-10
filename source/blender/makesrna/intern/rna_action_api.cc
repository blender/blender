/* SPDX-FileCopyrightText: 2009 Blender Foundation
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

#include "DNA_action_types.h"

#include "rna_internal.h" /* own include */

#ifdef RNA_RUNTIME

#  include "BKE_action.h"

#  include "DNA_anim_types.h"
#  include "DNA_curve_types.h"

static void rna_Action_flip_with_pose(bAction *act, ReportList *reports, Object *ob)
{
  if (ob->type != OB_ARMATURE) {
    BKE_report(reports, RPT_ERROR, "Only armature objects are supported");
    return;
  }
  BKE_action_flip_with_pose(act, ob);

  /* Only for redraw. */
  WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

#else

void RNA_api_action(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "flip_with_pose", "rna_Action_flip_with_pose");
  RNA_def_function_ui_description(func, "Flip the action around the X axis using a pose");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);

  parm = RNA_def_pointer(
      func, "object", "Object", "", "The reference armature object to use when flipping");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
}

#endif
