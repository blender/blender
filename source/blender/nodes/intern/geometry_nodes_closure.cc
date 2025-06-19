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

bool ClosureSignature::matches_exactly(const ClosureSignature &other) const
{
  if (inputs.size() != other.inputs.size()) {
    return false;
  }
  if (outputs.size() != other.outputs.size()) {
    return false;
  }
  for (const Item &item : inputs) {
    if (std::none_of(other.inputs.begin(), other.inputs.end(), [&](const Item &other_item) {
          return item.key.matches(other_item.key) && item.type == other_item.type &&
                 item.structure_type == other_item.structure_type;
        }))
    {
      return false;
    }
  }
  for (const Item &item : outputs) {
    if (std::none_of(other.outputs.begin(), other.outputs.end(), [&](const Item &other_item) {
          return item.key.matches(other_item.key) && item.type == other_item.type &&
                 item.structure_type == other_item.structure_type;
        }))
    {
      return false;
    }
  }
  return true;
}

}  // namespace blender::nodes
