/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase.h"

#include "BKE_grease_pencil.hh"

#include "DNA_outliner_types.h"

#include "../outliner_intern.hh"

#include "tree_element_grease_pencil_node.hh"

namespace blender::ed::outliner {

TreeElementGreasePencilNode::TreeElementGreasePencilNode(TreeElement &legacy_te,
                                                         GreasePencil &owner_grease_pencil,
                                                         bke::greasepencil::TreeNode &node)
    : AbstractTreeElement(legacy_te), owner_grease_pencil_(owner_grease_pencil), node_(node)
{
  BLI_assert(legacy_te.store_elem->type == TSE_GREASE_PENCIL_NODE);
  legacy_te.name = node.name().c_str();
}

void TreeElementGreasePencilNode::expand(SpaceOutliner & /*space_outliner*/) const
{
  if (!node_.is_group()) {
    return;
  }
  LISTBASE_FOREACH_BACKWARD (GreasePencilLayerTreeNode *, child, &node_.as_group().children) {
    add_element(&legacy_te_.subtree,
                &owner_grease_pencil_.id,
                child,
                &legacy_te_,
                TSE_GREASE_PENCIL_NODE,
                0);
  }
}

blender::bke::greasepencil::TreeNode &TreeElementGreasePencilNode::node() const
{
  return node_;
}

}  // namespace blender::ed::outliner
