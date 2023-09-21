/* SPDX-FileCopyrightText: 2023 Blender Authors
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
struct GreasePencil;

namespace blender::ed::outliner {

class TreeElementGreasePencilNode final : public AbstractTreeElement {
  GreasePencil &owner_grease_pencil_;
  blender::bke::greasepencil::TreeNode &node_;

 public:
  TreeElementGreasePencilNode(TreeElement &legacy_te,
                              GreasePencil &owner_grease_pencil,
                              blender::bke::greasepencil::TreeNode &node);

  void expand(SpaceOutliner &) const override;

  blender::bke::greasepencil::TreeNode &node() const;
};

}  // namespace blender::ed::outliner
