/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_node.hh"

#include "NOD_socket_interface_key.hh"

namespace blender::nodes {

/** Describes the names and types of the inputs and outputs of a closure. */
class ClosureSignature {
 public:
  struct Item {
    SocketInterfaceKey key;
    const bke::bNodeSocketType *type = nullptr;
    std::optional<StructureType> structure_type = std::nullopt;
  };

  Vector<Item> inputs;
  Vector<Item> outputs;

  std::optional<int> find_input_index(const SocketInterfaceKey &key) const;
  std::optional<int> find_output_index(const SocketInterfaceKey &key) const;

  bool matches_exactly(const ClosureSignature &other) const;
};

}  // namespace blender::nodes
