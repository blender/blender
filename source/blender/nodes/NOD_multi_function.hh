/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "FN_multi_function.hh"

#include "DNA_node_types.h"

namespace blender::nodes {

class NodeMultiFunctions;

/**
 * Utility class to help nodes build a multi-function for themselves.
 */
class NodeMultiFunctionBuilder : NonCopyable, NonMovable {
 private:
  const bNode &node_;
  const bNodeTree &tree_;
  std::shared_ptr<mf::MultiFunction> owned_built_fn_;
  const mf::MultiFunction *built_fn_ = nullptr;

  friend NodeMultiFunctions;

 public:
  NodeMultiFunctionBuilder(const bNode &node, const bNodeTree &tree);

  /**
   * Assign a multi-function for the current node. The input and output parameters of the function
   * have to match the available sockets in the node.
   */
  void set_matching_fn(const mf::MultiFunction *fn);
  void set_matching_fn(const mf::MultiFunction &fn);

  /**
   * Utility method for creating and assigning a multi-function when it can't have a static
   * lifetime.
   */
  template<typename T, typename... Args> void construct_and_set_matching_fn(Args &&...args);

  const bNode &node();
  const bNodeTree &tree();
};

/**
 * Gives access to multi-functions for all nodes in a node tree that support them.
 */
class NodeMultiFunctions {
 public:
  struct Item {
    const mf::MultiFunction *fn = nullptr;
    std::shared_ptr<mf::MultiFunction> owned_fn;
  };

 private:
  Map<const bNode *, Item> map_;

 public:
  NodeMultiFunctions(const bNodeTree &tree);

  const Item &try_get(const bNode &node) const;
};

/* -------------------------------------------------------------------- */
/** \name #NodeMultiFunctionBuilder Inline Methods
 * \{ */

inline NodeMultiFunctionBuilder::NodeMultiFunctionBuilder(const bNode &node, const bNodeTree &tree)
    : node_(node), tree_(tree)
{
}

inline const bNode &NodeMultiFunctionBuilder::node()
{
  return node_;
}

inline const bNodeTree &NodeMultiFunctionBuilder::tree()
{
  return tree_;
}

inline void NodeMultiFunctionBuilder::set_matching_fn(const mf::MultiFunction *fn)
{
  built_fn_ = fn;
}

inline void NodeMultiFunctionBuilder::set_matching_fn(const mf::MultiFunction &fn)
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

inline const NodeMultiFunctions::Item &NodeMultiFunctions::try_get(const bNode &node) const
{
  static Item empty_item;
  const Item *item = map_.lookup_ptr(&node);
  if (item == nullptr) {
    return empty_item;
  }
  return *item;
}

/** \} */

}  // namespace blender::nodes
