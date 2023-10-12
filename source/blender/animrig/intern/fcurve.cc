/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_fcurve.hh"
#include "BKE_fcurve.h"
#include "DNA_anim_types.h"
#include "ED_anim_api.hh"

namespace blender::animrig {

/**
 * \note caller needs to run #BKE_nla_tweakedit_remap to get NLA relative frame.
 *       caller should also check #BKE_fcurve_is_protected before keying.
 */
bool delete_keyframe_fcurve(AnimData *adt, FCurve *fcu, float cfra)
{
  bool found;
  int i;

  /* try to find index of beztriple to get rid of */
  i = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, cfra, fcu->totvert, &found);
  if (found) {
    /* delete the key at the index (will sanity check + do recalc afterwards) */
    BKE_fcurve_delete_key(fcu, i);
    BKE_fcurve_handles_recalc(fcu);

    /* Only delete curve too if it won't be doing anything anymore */
    if (BKE_fcurve_is_empty(fcu)) {
      ANIM_fcurve_delete_from_animdata(nullptr, adt, fcu);
    }

    /* return success */
    return true;
  }
  return false;
}
}  // namespace blender::animrig
