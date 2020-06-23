/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BLI_dot_export.hh"
#include "FN_multi_function_network.hh"

namespace blender {
namespace fn {

MFNetwork::~MFNetwork()
{
  for (MFFunctionNode *node : m_function_nodes) {
    node->destruct_sockets();
    node->~MFFunctionNode();
  }
  for (MFDummyNode *node : m_dummy_nodes) {
    node->destruct_sockets();
    node->~MFDummyNode();
  }
}

void MFNode::destruct_sockets()
{
  for (MFInputSocket *socket : m_inputs) {
    socket->~MFInputSocket();
  }
  for (MFOutputSocket *socket : m_outputs) {
    socket->~MFOutputSocket();
  }
}

/**
 * Add a new function node to the network. The caller keeps the ownership of the function. The
 * function should not be freed before the network. A reference to the new node is returned. The
 * node is owned by the network.
 */
MFFunctionNode &MFNetwork::add_function(const MultiFunction &function)
{
  Vector<uint, 16> input_param_indices, output_param_indices;

  for (uint param_index : function.param_indices()) {
    switch (function.param_type(param_index).interface_type()) {
      case MFParamType::Input: {
        input_param_indices.append(param_index);
        break;
      }
      case MFParamType::Output: {
        output_param_indices.append(param_index);
        break;
      }
      case MFParamType::Mutable: {
        input_param_indices.append(param_index);
        output_param_indices.append(param_index);
        break;
      }
    }
  }

  MFFunctionNode &node = *m_allocator.construct<MFFunctionNode>();
  m_function_nodes.add_new(&node);

  node.m_network = this;
  node.m_is_dummy = false;
  node.m_id = m_node_or_null_by_id.append_and_get_index(&node);
  node.m_function = &function;
  node.m_input_param_indices = m_allocator.construct_array_copy<uint>(input_param_indices);
  node.m_output_param_indices = m_allocator.construct_array_copy<uint>(output_param_indices);

  node.m_inputs = m_allocator.construct_elements_and_pointer_array<MFInputSocket>(
      input_param_indices.size());
  node.m_outputs = m_allocator.construct_elements_and_pointer_array<MFOutputSocket>(
      output_param_indices.size());

  for (uint i : input_param_indices.index_range()) {
    uint param_index = input_param_indices[i];
    MFParamType param = function.param_type(param_index);
    BLI_assert(param.is_input_or_mutable());

    MFInputSocket &socket = *node.m_inputs[i];
    socket.m_data_type = param.data_type();
    socket.m_node = &node;
    socket.m_index = i;
    socket.m_is_output = false;
    socket.m_name = function.param_name(param_index);
    socket.m_origin = nullptr;
    socket.m_id = m_socket_or_null_by_id.append_and_get_index(&socket);
  }

  for (uint i : output_param_indices.index_range()) {
    uint param_index = output_param_indices[i];
    MFParamType param = function.param_type(param_index);
    BLI_assert(param.is_output_or_mutable());

    MFOutputSocket &socket = *node.m_outputs[i];
    socket.m_data_type = param.data_type();
    socket.m_node = &node;
    socket.m_index = i;
    socket.m_is_output = true;
    socket.m_name = function.param_name(param_index);
    socket.m_id = m_socket_or_null_by_id.append_and_get_index(&socket);
  }

  return node;
}

/**
 * Add a dummy node with the given input and output sockets.
 */
MFDummyNode &MFNetwork::add_dummy(StringRef name,
                                  Span<MFDataType> input_types,
                                  Span<MFDataType> output_types,
                                  Span<StringRef> input_names,
                                  Span<StringRef> output_names)
{
  assert_same_size(input_types, input_names);
  assert_same_size(output_types, output_names);

  MFDummyNode &node = *m_allocator.construct<MFDummyNode>();
  m_dummy_nodes.add_new(&node);

  node.m_network = this;
  node.m_is_dummy = true;
  node.m_name = m_allocator.copy_string(name);
  node.m_id = m_node_or_null_by_id.append_and_get_index(&node);

  node.m_inputs = m_allocator.construct_elements_and_pointer_array<MFInputSocket>(
      input_types.size());
  node.m_outputs = m_allocator.construct_elements_and_pointer_array<MFOutputSocket>(
      output_types.size());

  node.m_input_names = m_allocator.allocate_array<StringRefNull>(input_types.size());
  node.m_output_names = m_allocator.allocate_array<StringRefNull>(output_types.size());

  for (uint i : input_types.index_range()) {
    MFInputSocket &socket = *node.m_inputs[i];
    socket.m_data_type = input_types[i];
    socket.m_node = &node;
    socket.m_index = i;
    socket.m_is_output = false;
    socket.m_name = m_allocator.copy_string(input_names[i]);
    socket.m_id = m_socket_or_null_by_id.append_and_get_index(&socket);
    node.m_input_names[i] = socket.m_name;
  }

  for (uint i : output_types.index_range()) {
    MFOutputSocket &socket = *node.m_outputs[i];
    socket.m_data_type = output_types[i];
    socket.m_node = &node;
    socket.m_index = i;
    socket.m_is_output = true;
    socket.m_name = m_allocator.copy_string(output_names[i]);
    socket.m_id = m_socket_or_null_by_id.append_and_get_index(&socket);
    node.m_output_names[i] = socket.m_name;
  }

  return node;
}

/**
 * Connect two sockets. This invokes undefined behavior if the sockets belong to different
 * networks, the sockets have a different data type, or the `to` socket is connected to something
 * else already.
 */
void MFNetwork::add_link(MFOutputSocket &from, MFInputSocket &to)
{
  BLI_assert(to.m_origin == nullptr);
  BLI_assert(from.m_node->m_network == to.m_node->m_network);
  BLI_assert(from.m_data_type == to.m_data_type);
  from.m_targets.append(&to);
  to.m_origin = &from;
}

MFOutputSocket &MFNetwork::add_input(StringRef name, MFDataType data_type)
{
  return this->add_dummy(name, {}, {data_type}, {}, {name}).output(0);
}

MFInputSocket &MFNetwork::add_output(StringRef name, MFDataType data_type)
{
  return this->add_dummy(name, {data_type}, {}, {name}, {}).input(0);
}

std::string MFNetwork::to_dot() const
{
  namespace Dot = blender::DotExport;

  Dot::DirectedGraph digraph;
  digraph.set_rankdir(Dot::Attr_rankdir::LeftToRight);

  Map<const MFNode *, Dot::NodeWithSocketsRef> dot_nodes;

  Vector<const MFNode *> all_nodes;
  all_nodes.extend(m_function_nodes.as_span());
  all_nodes.extend(m_dummy_nodes.as_span());

  for (const MFNode *node : all_nodes) {
    Dot::Node &dot_node = digraph.new_node("");

    Vector<std::string> input_names, output_names;
    for (const MFInputSocket *socket : node->m_inputs) {
      input_names.append(socket->name() + "(" + socket->data_type().to_string() + ")");
    }
    for (const MFOutputSocket *socket : node->m_outputs) {
      output_names.append(socket->name() + " (" + socket->data_type().to_string() + ")");
    }

    Dot::NodeWithSocketsRef dot_node_ref{dot_node, node->name(), input_names, output_names};
    dot_nodes.add_new(node, dot_node_ref);
  }

  for (const MFNode *to_node : all_nodes) {
    Dot::NodeWithSocketsRef to_dot_node = dot_nodes.lookup(to_node);

    for (const MFInputSocket *to_socket : to_node->m_inputs) {
      const MFOutputSocket *from_socket = to_socket->m_origin;
      if (from_socket != nullptr) {
        const MFNode *from_node = from_socket->m_node;
        Dot::NodeWithSocketsRef from_dot_node = dot_nodes.lookup(from_node);
        digraph.new_edge(from_dot_node.output(from_socket->m_index),
                         to_dot_node.input(to_socket->m_index));
      }
    }
  }

  return digraph.to_dot_string();
}

}  // namespace fn
}  // namespace blender
