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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "BKE_action.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"

#include "MEM_guardedalloc.h"

namespace blender::bke {

void BKE_action_find_fcurves_with_bones(const bAction *action, FoundFCurveCallback callback)
{
  LISTBASE_FOREACH (FCurve *, fcu, &action->curves) {
    char *bone_name = BLI_str_quoted_substrN(fcu->rna_path, "pose.bones[");
    if (!bone_name) {
      continue;
    }
    callback(fcu, bone_name);
    MEM_freeN(bone_name);
  }
}

}  // namespace blender::bke
