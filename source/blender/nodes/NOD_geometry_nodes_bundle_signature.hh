/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_node.hh"

namespace blender::nodes {

struct BundleSignature {
  struct Item {
    std::string key;
    const bke::bNodeSocketType *type = nullptr;
  };

  Vector<Item> items;

  bool matches_exactly(const BundleSignature &other) const;

  static bool all_matching_exactly(const Span<BundleSignature> signatures);

  static BundleSignature from_combine_bundle_node(const bNode &node);
  static BundleSignature from_separate_bundle_node(const bNode &node);
};

}  // namespace blender::nodes
