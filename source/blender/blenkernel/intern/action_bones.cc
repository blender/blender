/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_action.hh"

#include "BLI_string.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"

#include "ANIM_action_legacy.hh"

namespace blender::bke {

void BKE_action_find_fcurves_with_bones(bAction *action,
                                        const blender::animrig::slot_handle_t slot_handle,
                                        FoundFCurveCallback callback)
{
  auto const_callback = [&](const FCurve *fcurve, const char *bone_name) {
    callback(const_cast<FCurve *>(fcurve), bone_name);
  };
  BKE_action_find_fcurves_with_bones(
      const_cast<const bAction *>(action), slot_handle, const_callback);
}

void BKE_action_find_fcurves_with_bones(const bAction *action,
                                        const blender::animrig::slot_handle_t slot_handle,
                                        FoundFCurveCallbackConst callback)
{
  for (const FCurve *fcu : animrig::legacy::fcurves_for_action_slot(action, slot_handle)) {
    char bone_name[MAXBONENAME];
    if (!BLI_str_quoted_substr(fcu->rna_path, "pose.bones[", bone_name, sizeof(bone_name))) {
      continue;
    }
    callback(fcu, bone_name);
  }
}

}  // namespace blender::bke
