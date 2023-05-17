/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node.hh"

#include "BLT_translation.h"

#include "NOD_add_node_search.hh"
#include "NOD_node_declaration.hh"

namespace blender::nodes {

void GatherAddNodeSearchParams::add_single_node_item(std::string ui_name,
                                                     std::string description,
                                                     AfterAddFn after_add_fn,
                                                     int weight)
{
  AddNodeItem item;
  item.ui_name = std::move(ui_name);
  item.description = std::move(description);
  item.weight = weight;
  item.add_fn = [after_add_fn = std::move(after_add_fn), node_type = node_type_](
                    const bContext &C, bNodeTree &node_tree, const float2 cursor) {
    bNode *new_node = nodeAddNode(&C, &node_tree, node_type.idname);
    new_node->locx = cursor.x / UI_SCALE_FAC;
    new_node->locy = cursor.y / UI_SCALE_FAC + 20;
    nodeSetSelected(new_node, true);
    nodeSetActive(&node_tree, new_node);
    if (after_add_fn) {
      after_add_fn(C, node_tree, *new_node);
    }
    return Vector<bNode *>{new_node};
  };
  r_items.append(std::move(item));
}

void GatherAddNodeSearchParams::add_item(AddNodeItem item)
{
  r_items.append(std::move(item));
}

void search_node_add_ops_for_basic_node(GatherAddNodeSearchParams &params)
{
  params.add_single_node_item(IFACE_(params.node_type().ui_name),
                              TIP_(params.node_type().ui_description));
}

}  // namespace blender::nodes
