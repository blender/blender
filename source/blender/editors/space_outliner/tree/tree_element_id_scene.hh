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

#include "tree_element_id.hh"

namespace blender::ed::outliner {

class TreeElementIDScene final : public TreeElementID {
  Scene &scene_;

 public:
  TreeElementIDScene(TreeElement &legacy_te, Scene &scene);

  void expand(SpaceOutliner &) const override;
  bool isExpandValid() const override;

 private:
  void expandViewLayers(SpaceOutliner &) const;
  void expandWorld(SpaceOutliner &) const;
  void expandCollections(SpaceOutliner &) const;
  void expandObjects(SpaceOutliner &) const;
};

}  // namespace blender::ed::outliner
