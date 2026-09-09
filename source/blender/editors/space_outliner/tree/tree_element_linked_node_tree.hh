/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "DNA_outliner_types.h"

#include "tree_element.hh"

namespace blender::ed::outliner {

class TreeElementLinkedNodeTree final : public AbstractTreeElement {

 public:
  static constexpr eTreeStoreElemType element_type = TSE_LINKED_NODE_TREE;

  TreeElementLinkedNodeTree(TreeElement &legacy_te, ID &id);

  /** The ID identifying this element in the tree-store, see #AbstractTreeDisplay::add_element().
   */
  static ID *owner_id(ID &id)
  {
    return &id;
  }

  std::optional<BIFIconID> get_icon() const override
  {
    return ICON_NODETREE;
  }
};

}  // namespace blender::ed::outliner
