/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_nodes_closure.hh"

namespace blender::nodes {

std::optional<int> ClosureSignature::find_input_index(const SocketInterfaceKey &key) const
{
  for (const int i : this->inputs.index_range()) {
    const Item &item = this->inputs[i];
    if (item.key.matches(key)) {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<int> ClosureSignature::find_output_index(const SocketInterfaceKey &key) const
{
  for (const int i : this->outputs.index_range()) {
    const Item &item = this->outputs[i];
    if (item.key.matches(key)) {
      return i;
    }
  }
  return std::nullopt;
}

}  // namespace blender::nodes
