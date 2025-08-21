/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

#include "ANIM_action.hh"

struct bAction;

namespace blender::ed::outliner {

class TreeElementIDAction final : public TreeElementID {
  bAction &action_;
  animrig::slot_handle_t slot_handle_ = animrig::Slot::unassigned;

 public:
  TreeElementIDAction(TreeElement &legacy_te, bAction &action);

  /**
   * When displaying this tree element in a "flat" tree view (so each Action is
   * only listed once, like in the Blender File outliner mode), this expands to
   * show all the Action's slots Otherwise, when using a data-hierarchical tree
   * view (like Scene or View Layer), only the assigned slot is shown.
   */
  void expand(SpaceOutliner &space_outliner) const override;
};

}  // namespace blender::ed::outliner
