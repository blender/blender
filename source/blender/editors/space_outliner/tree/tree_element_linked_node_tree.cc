/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_ID.h"
#include "DNA_outliner_types.h"

#include "../outliner_intern.hh"

#include "tree_element_linked_node_tree.hh"

namespace blender::ed::outliner {

TreeElementLinkedNodeTree::TreeElementLinkedNodeTree(TreeElement &legacy_te, ID &id)
    : AbstractTreeElement(legacy_te)
{
  BLI_assert(legacy_te.store_elem->type == TSE_LINKED_NODE_TREE);
  legacy_te.name = id.name + 2;
  legacy_te.idcode = GS(id.name);
}

}  // namespace blender::ed::outliner
