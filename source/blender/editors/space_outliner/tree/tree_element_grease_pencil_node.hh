/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender::bke::greasepencil {
class TreeNode;
}  // namespace blender::bke::greasepencil

namespace blender::ed::outliner {

class TreeElementGreasePencilNode final : public AbstractTreeElement {
  blender::bke::greasepencil::TreeNode &node_;
 public:
  TreeElementGreasePencilNode(TreeElement &legacy_te, blender::bke::greasepencil::TreeNode &node);

  void expand(SpaceOutliner &) const override;

  blender::bke::greasepencil::TreeNode &node() const;
};

}  // namespace blender::ed::outliner
