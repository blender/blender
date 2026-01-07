/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "tree_element.hh"

namespace blender {

namespace bke::greasepencil {
class TreeNode;
}  // namespace bke::greasepencil
struct GreasePencil;

namespace ed::outliner {

class TreeElementGreasePencilNode final : public AbstractTreeElement {
  GreasePencil &owner_grease_pencil_;
  bke::greasepencil::TreeNode &node_;

 public:
  TreeElementGreasePencilNode(TreeElement &legacy_te,
                              GreasePencil &owner_grease_pencil,
                              bke::greasepencil::TreeNode &node);

  void expand(SpaceOutliner & /*soops*/) const override;

  bke::greasepencil::TreeNode &node() const;
};

}  // namespace ed::outliner
}  // namespace blender
