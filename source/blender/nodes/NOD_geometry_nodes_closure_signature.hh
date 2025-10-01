/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_node.hh"

#include "BLI_vector_set.hh"

#include "NOD_node_in_compute_context.hh"

namespace blender::nodes {

/** Describes the names and types of the inputs and outputs of a closure. */
class ClosureSignature {
 public:
  struct Item {
    std::string key;
    const bke::bNodeSocketType *type = nullptr;
    NodeSocketInterfaceStructureType structure_type;

    BLI_STRUCT_EQUALITY_OPERATORS_3(Item, key, type, structure_type);
  };

  struct ItemKeyGetter {
    std::string operator()(const Item &item)
    {
      return item.key;
    }
  };

  CustomIDVectorSet<Item, ItemKeyGetter> inputs;
  CustomIDVectorSet<Item, ItemKeyGetter> outputs;

  std::optional<int> find_input_index(StringRef key) const;
  std::optional<int> find_output_index(StringRef key) const;

  friend bool operator==(const ClosureSignature &a, const ClosureSignature &b);
  friend bool operator!=(const ClosureSignature &a, const ClosureSignature &b);

  static ClosureSignature from_closure_output_node(const bNode &node,
                                                   bool allow_auto_structure_type);
  static ClosureSignature from_evaluate_closure_node(const bNode &node,
                                                     bool allow_auto_structure_type);

  void set_auto_structure_types();
};

/**
 * Multiple closure signatures that may be linked to a single node.
 */
struct LinkedClosureSignatures {
  struct Item {
    ClosureSignature signature;
    bool define_signature = false;
    SocketInContext socket;
  };
  Vector<Item> items;
  bool has_type_definition() const;

  std::optional<ClosureSignature> get_merged_signature() const;
};

}  // namespace blender::nodes
