/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_node.hh"

namespace blender::nodes {

/** Describes the names and types of the inputs and outputs of a closure. */
class ClosureSignature {
 public:
  struct Item {
    std::string key;
    const bke::bNodeSocketType *type = nullptr;
    std::optional<StructureType> structure_type = std::nullopt;
  };

  Vector<Item> inputs;
  Vector<Item> outputs;

  std::optional<int> find_input_index(StringRef key) const;
  std::optional<int> find_output_index(StringRef key) const;

  bool matches_exactly(const ClosureSignature &other) const;
  static bool all_matching_exactly(Span<ClosureSignature> signatures);

  static ClosureSignature from_closure_output_node(const bNode &node);
  static ClosureSignature from_evaluate_closure_node(const bNode &node);
};

}  // namespace blender::nodes
