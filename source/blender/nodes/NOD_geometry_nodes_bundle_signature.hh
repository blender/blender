/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_vector_set.hh"

#include "BKE_node.hh"

namespace blender::nodes {

struct BundleSignature {

  struct Item {
    std::string key;
    const bke::bNodeSocketType *type = nullptr;

    uint64_t hash() const
    {
      return get_default_hash(this->key);
    }

    BLI_STRUCT_EQUALITY_OPERATORS_1(Item, key)
  };

  struct ItemKeyGetter {
    std::string operator()(const Item &item)
    {
      return item.key;
    }
  };

  CustomIDVectorSet<Item, ItemKeyGetter> items;

  bool matches_exactly(const BundleSignature &other) const;

  static bool all_matching_exactly(const Span<BundleSignature> signatures);

  static BundleSignature from_combine_bundle_node(const bNode &node);
  static BundleSignature from_separate_bundle_node(const bNode &node);
};

}  // namespace blender::nodes
