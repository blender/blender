/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <functional>

#include "BLI_function_ref.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h" /* Necessary for eNodeSocketInOut. */

#include "NOD_node_declaration.hh"

struct bContext;

namespace blender::nodes {

struct AddNodeInfo {
  using AfterAddFn = std::function<void(const bContext &C, bNodeTree &node_tree, bNode &node)>;
  std::string ui_name;
  std::string description;
  AfterAddFn after_add_fn;
  int weight = 0;
};

class GatherAddNodeSearchParams {
  const bNodeType &node_type_;
  const bNodeTree &node_tree_;
  Vector<AddNodeInfo> &r_items;

 public:
  GatherAddNodeSearchParams(const bNodeType &node_type,
                            const bNodeTree &node_tree,
                            Vector<AddNodeInfo> &r_items)
      : node_type_(node_type), node_tree_(node_tree), r_items(r_items)
  {
  }

  const bNodeTree &node_tree() const
  {
    return node_tree_;
  }

  const bNodeType &node_type() const
  {
    return node_type_;
  }

  /**
   * \param weight: Used to customize the order when multiple search items match.
   */
  void add_item(std::string ui_name,
                std::string description,
                AddNodeInfo::AfterAddFn fn = {},
                int weight = 0);
};

void search_node_add_ops_for_basic_node(GatherAddNodeSearchParams &params);

}  // namespace blender::nodes
