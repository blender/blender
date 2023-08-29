/* SPDX-FileCopyrightText: 2023 Blender Authors
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
  void expand_view_layers(SpaceOutliner &) const;
  void expand_world(SpaceOutliner &) const;
  void expand_collections(SpaceOutliner &) const;
  void expand_objects(SpaceOutliner &) const;
};

}  // namespace blender::ed::outliner
