/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <functional>

#include "BLI_function_ref.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h" /* Necessary for eNodeSocketInOut. */

#include "NOD_node_declaration.hh"

struct bContext;

namespace blender::nodes {

struct AddNodeItem {
  using AddFn =
      std::function<Vector<bNode *>(const bContext &C, bNodeTree &node_tree, float2 cursor)>;
  std::string ui_name;
  std::string description;
  int weight = 0;
  AddFn add_fn;
};

class GatherAddNodeSearchParams {
  using AfterAddFn = std::function<void(const bContext &C, bNodeTree &node_tree, bNode &node)>;
  const bContext &C_;
  const bNodeType &node_type_;
  const bNodeTree &node_tree_;
  Vector<AddNodeItem> &r_items;

 public:
  GatherAddNodeSearchParams(const bContext &C,
                            const bNodeType &node_type,
                            const bNodeTree &node_tree,
                            Vector<AddNodeItem> &r_items)
      : C_(C), node_type_(node_type), node_tree_(node_tree), r_items(r_items)
  {
  }

  const bContext &context() const
  {
    return C_;
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
  void add_single_node_item(std::string ui_name,
                            std::string description,
                            AfterAddFn after_add_fn = {},
                            int weight = 0);
  void add_item(AddNodeItem item);
};

void search_node_add_ops_for_basic_node(GatherAddNodeSearchParams &params);

}  // namespace blender::nodes
