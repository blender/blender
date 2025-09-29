/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_vector_set.hh"

#include "BKE_node.hh"

#include "NOD_node_in_compute_context.hh"

namespace blender::nodes {

struct BundleSignature {
  struct Item {
    std::string key;
    const bke::bNodeSocketType *type = nullptr;
    NodeSocketInterfaceStructureType structure_type;

    BLI_STRUCT_EQUALITY_OPERATORS_3(Item, key, type, structure_type);
  };

  struct ItemKeyGetter {
    StringRefNull operator()(const Item &item)
    {
      return item.key;
    }
  };

  CustomIDVectorSet<Item, ItemKeyGetter> items;

  friend bool operator==(const BundleSignature &a, const BundleSignature &b);
  friend bool operator!=(const BundleSignature &a, const BundleSignature &b);

  static BundleSignature from_combine_bundle_node(const bNode &node,
                                                  bool allow_auto_structure_type);
  static BundleSignature from_separate_bundle_node(const bNode &node,
                                                   bool allow_auto_structure_type);

  void set_auto_structure_types();
};

/**
 * Multiple bundle signatures that may be linked to a single node.
 */
struct LinkedBundleSignatures {
  struct Item {
    BundleSignature signature;
    bool is_signature_definition = false;
    SocketInContext source_socket;
  };
  Vector<Item> items;
  bool has_type_definition() const;

  std::optional<BundleSignature> get_merged_signature() const;
};

NodeSocketInterfaceStructureType get_structure_type_for_bundle_signature(
    const bNodeSocket &socket,
    const NodeSocketInterfaceStructureType stored_structure_type,
    const bool allow_auto_structure_type);

}  // namespace blender::nodes
