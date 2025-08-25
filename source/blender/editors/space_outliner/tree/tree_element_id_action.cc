/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_action_types.h"
#include "DNA_outliner_types.h"
#include "DNA_space_types.h"

#include "../outliner_intern.hh"

#include "tree_element_anim_data.hh"
#include "tree_element_id_action.hh"

#include "ANIM_action.hh"

namespace blender::ed::outliner {

TreeElementIDAction::TreeElementIDAction(TreeElement &legacy_te, bAction &action)
    : TreeElementID(legacy_te, action.id), action_(action)
{
  /* If the outliner is showing the Action because it's in some hierarchical data mode, only show
   * the slot that is used by the parent ID tree element. Showing all slots will create quadratic
   * complexity, as each user of the Action has a child tree element for the Action. This means the
   * complexity is O(U x S), where U = the number of users of the Action, and S = the number of
   * slots. Typically U = S.
   *
   * In `SO_LIBRARIES` mode, the outliner shows Actions as a flat list in the 'Actions' subtree,
   * and also (just like `SO_SCENES` and `SO_VIEW_LAYER`) underneath each user. The former should
   * show all slots, whereas the latter should only show the assigned one. The difference is
   * detected by the type of the parent tree element.
   *
   * To simplify the code, the mode of the Outliner is ignored, and whether to show all slots or
   * not is determined purely by the type of the parent tree element. See the constructor. */

  /* Fetch the assigned slot handle from the parent node in the tree. This is done this way,
   * because AbstractTreeElement::add_element() constructs the element and immediately calls its
   * expand() function. That means that there is no time for the creator of this
   * TreeElementIDAction to pass us the slot handle explicitly.
   *
   * Adding a constructor parameter for this is also not feasible, due to the generic nature of the
   * code that constructs this TreeElement. */
  const TreeElement *legacy_parent = legacy_te.parent;
  if (!legacy_parent) {
    return;
  }

  const TreeElementAnimData *parent_anim_te = tree_element_cast<const TreeElementAnimData>(
      legacy_parent);
  if (!parent_anim_te) {
    return;
  }

  this->slot_handle_.emplace(parent_anim_te->get_slot_handle());
}

void TreeElementIDAction::expand(SpaceOutliner & /*space_outliner*/) const
{
  animrig::Action &action = action_.wrap();
  if (!this->slot_handle_.has_value()) {
    /* Show all slots of the Action. */
    for (animrig::Slot *slot : action.slots()) {
      add_element(&legacy_te_.subtree,
                  reinterpret_cast<ID *>(&action_),
                  slot,
                  &legacy_te_,
                  TSE_ACTION_SLOT,
                  0);
    }
    return;
  }

  /* Only show a single slot. Note that the slot handle value itself could be animrig::unassigned,
   * in which case there will not be a slot to show. */
  animrig::Slot *slot = action.slot_for_handle(this->slot_handle_.value());
  if (!slot) {
    return;
  }
  add_element(&legacy_te_.subtree,
              reinterpret_cast<ID *>(&action_),
              slot,
              &legacy_te_,
              TSE_ACTION_SLOT,
              0);
}

}  // namespace blender::ed::outliner
