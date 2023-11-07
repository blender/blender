/* SPDX-FileCopyrightText: 2023 Blender Authors
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

bool delete_keyframe_fcurve(AnimData *adt, FCurve *fcu, float cfra)
{
  bool found;

  const int index = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, cfra, fcu->totvert, &found);
  if (!found) {
    return false;
  }

  /* Delete the key at the index (will sanity check + do recalc afterwards). */
  BKE_fcurve_delete_key(fcu, index);
  BKE_fcurve_handles_recalc(fcu);

  /* Empty curves get automatically deleted. */
  if (BKE_fcurve_is_empty(fcu)) {
    ANIM_fcurve_delete_from_animdata(nullptr, adt, fcu);
  }

  return true;
}
}  // namespace blender::animrig
