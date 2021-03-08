/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup spoutliner
 *
 * Tree element classes for the tree elements directly representing an ID (#TSE_SOME_ID).
 */

#pragma once

#include "tree_element.hh"

namespace blender::ed::outliner {

class TreeElementID : public AbstractTreeElement {
 protected:
  ID &id_;

 public:
  TreeElementID(TreeElement &legacy_te, ID &id);

  static TreeElementID *createFromID(TreeElement &legacy_te, ID &id);

  void postExpand(SpaceOutliner &) const override;
  bool expandPoll(const SpaceOutliner &) const override;

  /**
   * Expanding not implemented for all types yet. Once it is, this can be set to true or
   * `AbstractTreeElement::expandValid()` can be removed altogether.
   */
  bool isExpandValid() const override
  {
    return false;
  }

 protected:
  /* ID types with animation data can use this. */
  void expand_animation_data(SpaceOutliner &, const AnimData *) const;

 private:
  void expand_library_overrides(SpaceOutliner &) const;
};

}  // namespace blender::ed::outliner
