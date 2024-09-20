/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "DNA_action_types.h"
#include "DNA_anim_types.h"

#include "BLI_listbase.h"
#include "BLI_set.hh"

#include "BKE_anim_data.hh"
#include "BKE_fcurve.hh"

#include "ANIM_action.hh"
#include "ANIM_action_legacy.hh"
#include "ANIM_fcurve.hh"

namespace blender::animrig {

void action_deselect_keys(Action &action)
{
  for (FCurve *fcu : legacy::fcurves_all(&action)) {
    BKE_fcurve_deselect_all_keys(*fcu);
  }
}

void deselect_keys_actions(Span<bAction *> actions)
{
  Set<bAction *> visited_actions;
  for (bAction *action : actions) {
    if (!visited_actions.add(action)) {
      continue;
    }
    action_deselect_keys(action->wrap());
  }
}

}  // namespace blender::animrig
