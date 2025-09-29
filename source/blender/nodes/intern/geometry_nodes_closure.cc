/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node_runtime.hh"

#include "NOD_geometry_nodes_bundle_signature.hh"
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

void ClosureSignature::set_auto_structure_types()
{
  for (const Item &item : this->inputs) {
    const_cast<Item &>(item).structure_type = NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO;
  }
  for (const Item &item : this->outputs) {
    const_cast<Item &>(item).structure_type = NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO;
  }
}

bool operator==(const ClosureSignature &a, const ClosureSignature &b)
{
  return a.inputs.as_span() == b.inputs.as_span() && a.outputs.as_span() == b.outputs.as_span();
}

bool operator!=(const ClosureSignature &a, const ClosureSignature &b)
{
  return !(a == b);
}

ClosureSignature ClosureSignature::from_closure_output_node(const bNode &node,
                                                            const bool allow_auto_structure_type)
{
  BLI_assert(node.is_type("NodeClosureOutput"));
  const bNodeTree &tree = node.owner_tree();
  const bNode *input_node =
      bke::zone_type_by_node_type(node.type_legacy)->get_corresponding_input(tree, node);
  const auto &storage = *static_cast<const NodeClosureOutput *>(node.storage);
  nodes::ClosureSignature signature;
  if (input_node) {
    for (const int i : IndexRange(storage.input_items.items_num)) {
      const NodeClosureInputItem &item = storage.input_items.items[i];
      const bNodeSocket &socket = input_node->output_socket(i);
      if (const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type))
      {
        const NodeSocketInterfaceStructureType structure_type =
            get_structure_type_for_bundle_signature(
                socket,
                NodeSocketInterfaceStructureType(item.structure_type),
                allow_auto_structure_type);
        signature.inputs.add({item.name, stype, structure_type});
      }
    }
  }
  for (const int i : IndexRange(storage.output_items.items_num)) {
    const NodeClosureOutputItem &item = storage.output_items.items[i];
    const bNodeSocket &socket = node.input_socket(i);
    if (const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type)) {
      const NodeSocketInterfaceStructureType structure_type =
          get_structure_type_for_bundle_signature(
              socket,
              NodeSocketInterfaceStructureType(item.structure_type),
              allow_auto_structure_type);
      signature.outputs.add({item.name, stype, structure_type});
    }
  }
  return signature;
}

ClosureSignature ClosureSignature::from_evaluate_closure_node(const bNode &node,
                                                              const bool allow_auto_structure_type)
{
  BLI_assert(node.is_type("NodeEvaluateClosure"));
  const auto &storage = *static_cast<const NodeEvaluateClosure *>(node.storage);
  nodes::ClosureSignature signature;
  for (const int i : IndexRange(storage.input_items.items_num)) {
    const NodeEvaluateClosureInputItem &item = storage.input_items.items[i];
    const bNodeSocket &socket = node.input_socket(i + 1);
    if (const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type)) {
      const NodeSocketInterfaceStructureType structure_type =
          get_structure_type_for_bundle_signature(
              socket,
              NodeSocketInterfaceStructureType(item.structure_type),
              allow_auto_structure_type);
      signature.inputs.add({item.name, stype, structure_type});
    }
  }
  for (const int i : IndexRange(storage.output_items.items_num)) {
    const NodeEvaluateClosureOutputItem &item = storage.output_items.items[i];
    const bNodeSocket &socket = node.output_socket(i);
    if (const bke::bNodeSocketType *stype = bke::node_socket_type_find_static(item.socket_type)) {
      const NodeSocketInterfaceStructureType structure_type =
          get_structure_type_for_bundle_signature(
              socket,
              NodeSocketInterfaceStructureType(item.structure_type),
              allow_auto_structure_type);
      signature.outputs.add({item.name, stype, structure_type});
    }
  }
  return signature;
}

bool LinkedClosureSignatures::has_type_definition() const
{
  for (const Item &item : this->items) {
    if (item.define_signature) {
      return true;
    }
  }
  return false;
}

std::optional<ClosureSignature> LinkedClosureSignatures::get_merged_signature() const
{
  ClosureSignature signature;
  for (const Item &src_signature : this->items) {
    for (const ClosureSignature::Item &item : src_signature.signature.inputs) {
      if (!signature.inputs.add(item)) {
        const ClosureSignature::Item &existing_item = *signature.inputs.lookup_key_ptr_as(
            item.key);
        if (existing_item.type->type != item.type->type) {
          return std::nullopt;
        }
        if (existing_item.structure_type != item.structure_type) {
          const_cast<ClosureSignature::Item &>(existing_item).structure_type =
              NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_DYNAMIC;
        }
      }
    }
    for (const ClosureSignature::Item &item : src_signature.signature.outputs) {
      if (!signature.outputs.add(item)) {
        const ClosureSignature::Item &existing_item = *signature.outputs.lookup_key_ptr_as(
            item.key);
        if (existing_item.type->type != item.type->type) {
          return std::nullopt;
        }
        if (existing_item.structure_type != item.structure_type) {
          const_cast<ClosureSignature::Item &>(existing_item).structure_type =
              NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_DYNAMIC;
        }
      }
    }
  }
  return signature;
}

}  // namespace blender::nodes
