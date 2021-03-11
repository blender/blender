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
 */

#pragma once

#include "tree_element.hh"

struct TreeElement;

namespace blender::ed::outliner {

class TreeElementAnimData final : public AbstractTreeElement {
  AnimData &anim_data_;

 public:
  TreeElementAnimData(TreeElement &legacy_te, AnimData &anim_data);

  void expand(SpaceOutliner &space_outliner) const override;

 private:
  void expand_drivers(SpaceOutliner &space_outliner) const;
  void expand_NLA_tracks(SpaceOutliner &space_outliner) const;
};

}  // namespace blender::ed::outliner
