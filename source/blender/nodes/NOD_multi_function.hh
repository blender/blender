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

#pragma once

#include "FN_multi_function.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"

namespace blender::nodes {

using namespace fn::multi_function_types;

class NodeMultiFunctions;

/**
 * Utility class to help nodes build a multi-function for themselves.
 */
class NodeMultiFunctionBuilder : NonCopyable, NonMovable {
 private:
  bNode &node_;
  bNodeTree &tree_;
  std::shared_ptr<MultiFunction> owned_built_fn_;
  const MultiFunction *built_fn_ = nullptr;

  friend NodeMultiFunctions;

 public:
  NodeMultiFunctionBuilder(bNode &node, bNodeTree &tree);

  /**
   * Assign a multi-function for the current node. The input and output parameters of the function
   * have to match the available sockets in the node.
   */
  void set_matching_fn(const MultiFunction *fn);
  void set_matching_fn(const MultiFunction &fn);

  /**
   * Utility method for creating and assigning a multi-function when it can't have a static
   * lifetime.
   */
  template<typename T, typename... Args> void construct_and_set_matching_fn(Args &&...args);

  bNode &node();
  bNodeTree &tree();
};

/**
 * Gives access to multi-functions for all nodes in a node tree that support them.
 */
class NodeMultiFunctions {
 public:
  struct Item {
    const MultiFunction *fn = nullptr;
    std::shared_ptr<MultiFunction> owned_fn;
  };

 private:
  Map<const bNode *, Item> map_;

 public:
  NodeMultiFunctions(const DerivedNodeTree &tree);

  const Item &try_get(const DNode &node) const;
};

/* -------------------------------------------------------------------- */
/** \name #NodeMultiFunctionBuilder Inline Methods
 * \{ */

inline NodeMultiFunctionBuilder::NodeMultiFunctionBuilder(bNode &node, bNodeTree &tree)
    : node_(node), tree_(tree)
{
}

inline bNode &NodeMultiFunctionBuilder::node()
{
  return node_;
}

inline bNodeTree &NodeMultiFunctionBuilder::tree()
{
  return tree_;
}

inline void NodeMultiFunctionBuilder::set_matching_fn(const MultiFunction *fn)
{
  built_fn_ = fn;
}

inline void NodeMultiFunctionBuilder::set_matching_fn(const MultiFunction &fn)
{
  built_fn_ = &fn;
}

template<typename T, typename... Args>
inline void NodeMultiFunctionBuilder::construct_and_set_matching_fn(Args &&...args)
{
  owned_built_fn_ = std::make_shared<T>(std::forward<Args>(args)...);
  built_fn_ = &*owned_built_fn_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #NodeMultiFunctions Inline Methods
 * \{ */

inline const NodeMultiFunctions::Item &NodeMultiFunctions::try_get(const DNode &node) const
{
  static Item empty_item;
  const Item *item = map_.lookup_ptr(node->bnode());
  if (item == nullptr) {
    return empty_item;
  }
  return *item;
}

/** \} */

}  // namespace blender::nodes
