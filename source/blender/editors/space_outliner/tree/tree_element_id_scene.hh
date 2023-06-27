/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

 private:
  void expandViewLayers(SpaceOutliner &) const;
  void expandWorld(SpaceOutliner &) const;
  void expandCollections(SpaceOutliner &) const;
  void expandObjects(SpaceOutliner &) const;
};

}  // namespace blender::ed::outliner
