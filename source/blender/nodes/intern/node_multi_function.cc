/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_multi_function.hh"

namespace blender::nodes {

NodeMultiFunctions::NodeMultiFunctions(const DerivedNodeTree &tree)
{
  for (const NodeTreeRef *tree_ref : tree.used_node_tree_refs()) {
    bNodeTree *btree = tree_ref->btree();
    for (const NodeRef *node : tree_ref->nodes()) {
      bNode *bnode = node->bnode();
      if (bnode->typeinfo->build_multi_function == nullptr) {
        continue;
      }
      NodeMultiFunctionBuilder builder{*bnode, *btree};
      bnode->typeinfo->build_multi_function(builder);
      if (builder.built_fn_ != nullptr) {
        map_.add_new(bnode, {builder.built_fn_, std::move(builder.owned_built_fn_)});
      }
    }
  }
}

}  // namespace blender::nodes
