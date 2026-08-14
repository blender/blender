/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "DNA_outliner_types.h"

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
  static constexpr eTreeStoreElemType element_type = TSE_GREASE_PENCIL_NODE;

  TreeElementGreasePencilNode(TreeElement &legacy_te,
                              GreasePencil &owner_grease_pencil,
                              bke::greasepencil::TreeNode &node);

  void expand(SpaceOutliner & /*soops*/) const override;

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(GreasePencil &owner_grease_pencil, bke::greasepencil::TreeNode &node);

  bke::greasepencil::TreeNode &node() const;
  std::optional<BIFIconID> get_icon() const override;
};

}  // namespace ed::outliner
}  // namespace blender
