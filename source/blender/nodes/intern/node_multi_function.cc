/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_multi_function.hh"

#include "BKE_node_runtime.hh"

namespace blender::nodes {

NodeMultiFunctions::NodeMultiFunctions(const bNodeTree &tree,
                                       const std::shared_ptr<const bNodeTree> &shared_tree)
{
  tree.ensure_topology_cache();
  for (const bNode *bnode : tree.all_nodes()) {
    if (bnode->typeinfo->build_multi_function == nullptr) {
      continue;
    }
    NodeMultiFunctionBuilder builder{*bnode, tree, shared_tree};
    bnode->typeinfo->build_multi_function(builder);
    if (builder.built_fn_ != nullptr) {
      map_.add_new(bnode, {builder.built_fn_, std::move(builder.owned_built_fn_)});
    }
  }
}

}  // namespace blender::nodes
