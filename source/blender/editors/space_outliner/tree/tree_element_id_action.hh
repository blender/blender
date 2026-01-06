/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element_id.hh"

#include "ANIM_action.hh"

#include <optional>

namespace blender {

struct bAction;

namespace ed::outliner {

class TreeElementIDAction final : public TreeElementID {
  bAction &action_;

  /**
   * Handle of the slot to show underneath this Action tree element.
   *
   * If this has no value, all slots are shown. Otherwise only the slot with
   * this handle is shown. If the handle is set to animrig::unassigned, no slot
   * is shown at all.
   */
  std::optional<animrig::slot_handle_t> slot_handle_ = std::nullopt;

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

}  // namespace ed::outliner
}  // namespace blender
