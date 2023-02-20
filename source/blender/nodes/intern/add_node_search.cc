/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node.h"

#include "BLT_translation.h"

#include "NOD_add_node_search.hh"
#include "NOD_node_declaration.hh"

namespace blender::nodes {

void GatherAddNodeSearchParams::add_item(std::string ui_name,
                                         std::string description,
                                         AddNodeInfo::AfterAddFn fn,
                                         int weight)
{
  r_items.append(AddNodeInfo{std::move(ui_name), std::move(description), std::move(fn), weight});
}

void search_node_add_ops_for_basic_node(GatherAddNodeSearchParams &params)
{
  params.add_item(IFACE_(params.node_type().ui_name), TIP_(params.node_type().ui_description));
}

}  // namespace blender::nodes
