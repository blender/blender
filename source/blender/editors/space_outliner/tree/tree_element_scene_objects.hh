/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

struct Scene;

namespace blender::ed::outliner {

class TreeElementSceneObjectsBase final : public AbstractTreeElement {
  Scene &scene_;

 public:
  TreeElementSceneObjectsBase(TreeElement &legacy_te, Scene &scene);

  void expand(SpaceOutliner &) const override;
};

}  // namespace blender::ed::outliner
