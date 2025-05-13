/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_action_types.h"
#include "DNA_outliner_types.h"

#include "../outliner_intern.hh"

#include "tree_element_id_action.hh"

#include "ANIM_action.hh"

namespace blender::ed::outliner {

TreeElementIDAction::TreeElementIDAction(TreeElement &legacy_te, bAction &action)
    : TreeElementID(legacy_te, action.id), action_(action)
{
}

void TreeElementIDAction::expand(SpaceOutliner & /* space_outliner */) const
{
  blender::animrig::Action &action = action_.wrap();
  for (blender::animrig::Slot *slot : action.slots()) {
    add_element(&legacy_te_.subtree,
                reinterpret_cast<ID *>(&action_),
                slot,
                &legacy_te_,
                TSE_ACTION_SLOT,
                0);
  }
}

}  // namespace blender::ed::outliner
