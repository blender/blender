/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 *
 * Tree element classes for the tree elements directly representing an ID (#TSE_SOME_ID).
 */

#pragma once

#include "tree_element.hh"

struct AnimData;
struct ID;

namespace blender::ed::outliner {

class TreeElementID : public AbstractTreeElement {
 protected:
  ID &id_;

 public:
  TreeElementID(TreeElement &legacy_te, ID &id);

  static std::unique_ptr<TreeElementID> createFromID(TreeElement &legacy_te, ID &id);

  bool expandPoll(const SpaceOutliner &) const override;

  /**
   * Expanding not implemented for all types yet. Once it is, this can be set to true or
   * `AbstractTreeElement::expandValid()` can be removed altogether.
   */
  bool isExpandValid() const override
  {
    return false;
  }

  ID &get_ID()
  {
    return id_;
  }

 protected:
  /* ID types with animation data can use this. */
  void expand_animation_data(SpaceOutliner &, const AnimData *) const;
};

}  // namespace blender::ed::outliner
