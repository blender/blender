/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_animdata.hh"
#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_lib_id.h"
#include "BLI_listbase.h"
#include "DNA_anim_types.h"
#include "ED_anim_api.hh"

namespace blender::animrig {

/* -------------------------------------------------------------------- */
/** \name Public F-Curves API
 * \{ */

void ANIM_fcurve_delete_from_animdata(bAnimContext *ac, AnimData *adt, FCurve *fcu)
{
  /* - if no AnimData, we've got nowhere to remove the F-Curve from
   *   (this doesn't guarantee that the F-Curve is in there, but at least we tried
   * - if no F-Curve, there is nothing to remove
   */
  if (ELEM(nullptr, adt, fcu)) {
    return;
  }

  /* remove from whatever list it came from
   * - Action Group
   * - Action
   * - Drivers
   * - TODO... some others?
   */
  if ((ac) && (ac->datatype == ANIMCONT_DRIVERS)) {
    /* driver F-Curve */
    BLI_remlink(&adt->drivers, fcu);
  }
  else if (adt->action) {
    bAction *act = adt->action;

    /* remove from group or action, whichever one "owns" the F-Curve */
    if (fcu->grp) {
      bActionGroup *agrp = fcu->grp;

      /* remove F-Curve from group+action */
      action_groups_remove_channel(act, fcu);

      /* if group has no more channels, remove it too,
       * otherwise can have many dangling groups #33541.
       */
      if (BLI_listbase_is_empty(&agrp->channels)) {
        BLI_freelinkN(&act->groups, agrp);
      }
    }
    else {
      BLI_remlink(&act->curves, fcu);
    }

    /* if action has no more F-Curves as a result of this, unlink it from
     * AnimData if it did not come from a NLA Strip being tweaked.
     *
     * This is done so that we don't have dangling Object+Action entries in
     * channel list that are empty, and linger around long after the data they
     * are for has disappeared (and probably won't come back).
     */
    ANIM_remove_empty_action_from_animdata(adt);
  }

  /* free the F-Curve itself */
  BKE_fcurve_free(fcu);
}

bool ANIM_remove_empty_action_from_animdata(AnimData *adt)
{
  if (adt->action != nullptr) {
    bAction *act = adt->action;

    if (BLI_listbase_is_empty(&act->curves) && (adt->flag & ADT_NLA_EDIT_ON) == 0) {
      id_us_min(&act->id);
      adt->action = nullptr;
      return true;
    }
  }

  return false;
}

/** \} */

}  // namespace blender::animrig