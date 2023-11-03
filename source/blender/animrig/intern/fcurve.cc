/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_fcurve.hh"
#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "DEG_depsgraph_build.hh"
#include "DNA_anim_types.h"
#include "ED_anim_api.hh"
#include "RNA_prototypes.h"

namespace blender::animrig {

FCurve *action_fcurve_find(bAction *act, const char rna_path[], const int array_index)
{
  if (ELEM(nullptr, act, rna_path)) {
    return nullptr;
  }
  return BKE_fcurve_find(&act->curves, rna_path, array_index);
}

FCurve *action_fcurve_ensure(Main *bmain,
                             bAction *act,
                             const char group[],
                             PointerRNA *ptr,
                             const char rna_path[],
                             const int array_index)
{
  bActionGroup *agrp;
  FCurve *fcu;

  if (ELEM(nullptr, act, rna_path)) {
    return nullptr;
  }

  /* try to find f-curve matching for this setting
   * - add if not found and allowed to add one
   *   TODO: add auto-grouping support? how this works will need to be resolved
   */
  fcu = BKE_fcurve_find(&act->curves, rna_path, array_index);

  if (fcu == nullptr) {
    fcu = BKE_fcurve_create();

    fcu->flag = (FCURVE_VISIBLE | FCURVE_SELECTED);
    fcu->auto_smoothing = U.auto_smoothing_new;
    if (BLI_listbase_is_empty(&act->curves)) {
      fcu->flag |= FCURVE_ACTIVE;
    }

    fcu->rna_path = BLI_strdup(rna_path);
    fcu->array_index = array_index;

    if (group) {
      agrp = BKE_action_group_find_name(act, group);

      if (agrp == nullptr) {
        agrp = action_groups_add_new(act, group);

        /* sync bone group colors if applicable */
        if (ptr && (ptr->type == &RNA_PoseBone)) {
          bPoseChannel *pchan = static_cast<bPoseChannel *>(ptr->data);
          action_group_colors_set_from_posebone(agrp, pchan);
        }
      }

      action_groups_add_channel(act, agrp, fcu);
    }
    else {
      BLI_addtail(&act->curves, fcu);
    }

    /* New f-curve was added, meaning it's possible that it affects
     * dependency graph component which wasn't previously animated.
     */
    DEG_relations_tag_update(bmain);
  }

  return fcu;
}

bool delete_keyframe_fcurve(AnimData *adt, FCurve *fcu, float cfra)
{
  bool found;
  int i;

  i = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, cfra, fcu->totvert, &found);
  if (found) {
    /* Delete the key at the index (will sanity check + do recalc afterwards). */
    BKE_fcurve_delete_key(fcu, i);
    BKE_fcurve_handles_recalc(fcu);

    /* Empty curves get automatically deleted. */
    if (BKE_fcurve_is_empty(fcu)) {
      ANIM_fcurve_delete_from_animdata(nullptr, adt, fcu);
    }

    return true;
  }
  return false;
}
}  // namespace blender::animrig
