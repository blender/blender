/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_action.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"

#include "MEM_guardedalloc.h"

namespace blender::bke {

void BKE_action_find_fcurves_with_bones(const bAction *action, FoundFCurveCallback callback)
{
  LISTBASE_FOREACH (FCurve *, fcu, &action->curves) {
    char bone_name[MAXBONENAME];
    if (!BLI_str_quoted_substr(fcu->rna_path, "pose.bones[", bone_name, sizeof(bone_name))) {
      continue;
    }
    callback(fcu, bone_name);
  }
}

}  // namespace blender::bke
