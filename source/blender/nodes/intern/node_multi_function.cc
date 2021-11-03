/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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
