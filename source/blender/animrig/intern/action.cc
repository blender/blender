/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_action.hh"
#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "DEG_depsgraph_build.hh"
#include "DNA_anim_types.h"
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
  if (ELEM(nullptr, act, rna_path)) {
    return nullptr;
  }

  /* Try to find f-curve matching for this setting.
   * - add if not found and allowed to add one
   *   TODO: add auto-grouping support? how this works will need to be resolved
   */
  FCurve *fcu = BKE_fcurve_find(&act->curves, rna_path, array_index);

  if (fcu != nullptr) {
    return fcu;
  }

  fcu = BKE_fcurve_create();

  fcu->flag = (FCURVE_VISIBLE | FCURVE_SELECTED);
  fcu->auto_smoothing = U.auto_smoothing_new;
  if (BLI_listbase_is_empty(&act->curves)) {
    fcu->flag |= FCURVE_ACTIVE;
  }

  fcu->rna_path = BLI_strdup(rna_path);
  fcu->array_index = array_index;

  if (group) {
    bActionGroup *agrp = BKE_action_group_find_name(act, group);

    if (agrp == nullptr) {
      agrp = action_groups_add_new(act, group);

      /* Sync bone group colors if applicable. */
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

  return fcu;
}
}  // namespace blender::animrig
