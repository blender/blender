/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender::ed::outliner {

class TreeElementViewLayerBase final : public AbstractTreeElement {
  Scene &scene_;

 public:
  TreeElementViewLayerBase(TreeElement &legacy_te, Scene &scene);

  void expand(SpaceOutliner &) const override;
};

}  // namespace blender::ed::outliner
