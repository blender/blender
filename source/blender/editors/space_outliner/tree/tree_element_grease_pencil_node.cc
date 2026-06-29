/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase.hh"

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
  int index = 0;
  for (GreasePencilLayerTreeNode &child : node_.as_group().children.items_reversed()) {
    add_element(&legacy_te_.subtree,
                &owner_grease_pencil_.id,
                &child,
                &legacy_te_,
                TSE_GREASE_PENCIL_NODE,
                index++);
  }
}

bke::greasepencil::TreeNode &TreeElementGreasePencilNode::node() const
{
  return node_;
}

std::optional<BIFIconID> TreeElementGreasePencilNode::get_icon() const
{
  BIFIconID icon = ICON_OUTLINER_DATA_GP_LAYER;
  if (node_.is_group()) {
    const bke::greasepencil::LayerGroup &group = node_.as_group();

    icon = ICON_GREASEPENCIL_LAYER_GROUP;
    if (group.color_tag != LAYERGROUP_COLOR_NONE) {
      icon = ICON_LAYERGROUP_COLOR_01 + int(group.color_tag);
    }
  }
  return icon;
}
}  // namespace blender::ed::outliner
