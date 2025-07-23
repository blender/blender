/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node_runtime.hh"

#include "NOD_geometry_nodes_closure.hh"

namespace blender::nodes {

std::optional<int> ClosureSignature::find_input_index(const StringRef key) const
{
  for (const int i : this->inputs.index_range()) {
    const Item &item = this->inputs[i];
    if (item.key == key) {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<int> ClosureSignature::find_output_index(const StringRef key) const
{
  for (const int i : this->outputs.index_range()) {
    const Item &item = this->outputs[i];
    if (item.key == key) {
      return i;
    }
  }
  return std::nullopt;
}

static bool items_equal(const ClosureSignature::Item &a, const ClosureSignature::Item &b)
{
  if (a.key != b.key) {
    return false;
  }
  if (a.type != b.type) {
    return false;
  }
  if (a.structure_type.has_value() && b.structure_type.has_value()) {
    if (*a.structure_type != *b.structure_type) {
      return false;
    }
  }
  return true;
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
          return items_equal(item, other_item);
        }))
    {
      return false;
    }
  }
  for (const Item &item : outputs) {
    if (std::none_of(other.outputs.begin(), other.outputs.end(), [&](const Item &other_item) {
          return items_equal(item, other_item);
        }))
    {
      return false;
    }
  }
  return true;
}

bool ClosureSignature::all_matching_exactly(const Span<ClosureSignature> signatures)
{
  if (signatures.is_empty()) {
    return true;
  }
  for (const ClosureSignature &signature : signatures.drop_front(1)) {
    if (!signatures[0].matches_exactly(signature)) {
      return false;
    }
  }
  return true;
}

ClosureSignature ClosureSignature::from_closure_output_node(const bNode &node)
{
  BLI_assert(node.is_type("GeometryNodeClosureOutput"));
  const auto &storage = *static_cast<const NodeGeometryClosureOutput *>(node.storage);
  nodes::ClosureSignature signature;
  for (const int i : IndexRange(storage.input_items.items_num)) {
    const NodeGeometryClosureInputItem &item = storage.input_items.items[i];
    if (const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type)) {
      signature.inputs.append({item.name, stype});
    }
  }
  for (const int i : IndexRange(storage.output_items.items_num)) {
    const NodeGeometryClosureOutputItem &item = storage.output_items.items[i];
    if (const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type)) {
      signature.outputs.append({item.name, stype});
    }
  }
  return signature;
}

ClosureSignature ClosureSignature::from_evaluate_closure_node(const bNode &node)
{
  BLI_assert(node.is_type("GeometryNodeEvaluateClosure"));
  const auto &storage = *static_cast<const NodeGeometryEvaluateClosure *>(node.storage);
  nodes::ClosureSignature signature;
  for (const int i : IndexRange(storage.input_items.items_num)) {
    const NodeGeometryEvaluateClosureInputItem &item = storage.input_items.items[i];
    if (const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type)) {
      signature.inputs.append({item.name, stype, nodes::StructureType(item.structure_type)});
    }
  }
  for (const int i : IndexRange(storage.output_items.items_num)) {
    const NodeGeometryEvaluateClosureOutputItem &item = storage.output_items.items[i];
    if (const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type)) {
      signature.outputs.append({item.name, stype, nodes::StructureType(item.structure_type)});
    }
  }
  return signature;
}

}  // namespace blender::nodes
