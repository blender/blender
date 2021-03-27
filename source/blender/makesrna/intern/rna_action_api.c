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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "BLI_utildefines.h"

#include "RNA_define.h"

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
  WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
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
