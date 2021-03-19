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
#include "BLI_stack.hh"

#include "FN_multi_function_network.hh"

namespace blender::fn {

MFNetwork::~MFNetwork()
{
  for (MFFunctionNode *node : function_nodes_) {
    node->destruct_sockets();
    node->~MFFunctionNode();
  }
  for (MFDummyNode *node : dummy_nodes_) {
    node->destruct_sockets();
    node->~MFDummyNode();
  }
}

void MFNode::destruct_sockets()
{
  for (MFInputSocket *socket : inputs_) {
    socket->~MFInputSocket();
  }
  for (MFOutputSocket *socket : outputs_) {
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
  Vector<int, 16> input_param_indices, output_param_indices;

  for (int param_index : function.param_indices()) {
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

  MFFunctionNode &node = *allocator_.construct<MFFunctionNode>().release();
  function_nodes_.add_new(&node);

  node.network_ = this;
  node.is_dummy_ = false;
  node.id_ = node_or_null_by_id_.append_and_get_index(&node);
  node.function_ = &function;
  node.input_param_indices_ = allocator_.construct_array_copy<int>(input_param_indices);
  node.output_param_indices_ = allocator_.construct_array_copy<int>(output_param_indices);

  node.inputs_ = allocator_.construct_elements_and_pointer_array<MFInputSocket>(
      input_param_indices.size());
  node.outputs_ = allocator_.construct_elements_and_pointer_array<MFOutputSocket>(
      output_param_indices.size());

  for (int i : input_param_indices.index_range()) {
    int param_index = input_param_indices[i];
    MFParamType param = function.param_type(param_index);
    BLI_assert(param.is_input_or_mutable());

    MFInputSocket &socket = *node.inputs_[i];
    socket.data_type_ = param.data_type();
    socket.node_ = &node;
    socket.index_ = i;
    socket.is_output_ = false;
    socket.name_ = function.param_name(param_index);
    socket.origin_ = nullptr;
    socket.id_ = socket_or_null_by_id_.append_and_get_index(&socket);
  }

  for (int i : output_param_indices.index_range()) {
    int param_index = output_param_indices[i];
    MFParamType param = function.param_type(param_index);
    BLI_assert(param.is_output_or_mutable());

    MFOutputSocket &socket = *node.outputs_[i];
    socket.data_type_ = param.data_type();
    socket.node_ = &node;
    socket.index_ = i;
    socket.is_output_ = true;
    socket.name_ = function.param_name(param_index);
    socket.id_ = socket_or_null_by_id_.append_and_get_index(&socket);
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

  MFDummyNode &node = *allocator_.construct<MFDummyNode>().release();
  dummy_nodes_.add_new(&node);

  node.network_ = this;
  node.is_dummy_ = true;
  node.name_ = allocator_.copy_string(name);
  node.id_ = node_or_null_by_id_.append_and_get_index(&node);

  node.inputs_ = allocator_.construct_elements_and_pointer_array<MFInputSocket>(
      input_types.size());
  node.outputs_ = allocator_.construct_elements_and_pointer_array<MFOutputSocket>(
      output_types.size());

  node.input_names_ = allocator_.allocate_array<StringRefNull>(input_types.size());
  node.output_names_ = allocator_.allocate_array<StringRefNull>(output_types.size());

  for (int i : input_types.index_range()) {
    MFInputSocket &socket = *node.inputs_[i];
    socket.data_type_ = input_types[i];
    socket.node_ = &node;
    socket.index_ = i;
    socket.is_output_ = false;
    socket.name_ = allocator_.copy_string(input_names[i]);
    socket.id_ = socket_or_null_by_id_.append_and_get_index(&socket);
    node.input_names_[i] = socket.name_;
  }

  for (int i : output_types.index_range()) {
    MFOutputSocket &socket = *node.outputs_[i];
    socket.data_type_ = output_types[i];
    socket.node_ = &node;
    socket.index_ = i;
    socket.is_output_ = true;
    socket.name_ = allocator_.copy_string(output_names[i]);
    socket.id_ = socket_or_null_by_id_.append_and_get_index(&socket);
    node.output_names_[i] = socket.name_;
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
  BLI_assert(to.origin_ == nullptr);
  BLI_assert(from.node_->network_ == to.node_->network_);
  BLI_assert(from.data_type_ == to.data_type_);
  from.targets_.append(&to);
  to.origin_ = &from;
}

MFOutputSocket &MFNetwork::add_input(StringRef name, MFDataType data_type)
{
  return this->add_dummy(name, {}, {data_type}, {}, {"Value"}).output(0);
}

MFInputSocket &MFNetwork::add_output(StringRef name, MFDataType data_type)
{
  return this->add_dummy(name, {data_type}, {}, {"Value"}, {}).input(0);
}

void MFNetwork::relink(MFOutputSocket &old_output, MFOutputSocket &new_output)
{
  BLI_assert(&old_output != &new_output);
  BLI_assert(old_output.data_type_ == new_output.data_type_);
  for (MFInputSocket *input : old_output.targets()) {
    input->origin_ = &new_output;
  }
  new_output.targets_.extend(old_output.targets_);
  old_output.targets_.clear();
}

void MFNetwork::remove(MFNode &node)
{
  for (MFInputSocket *socket : node.inputs_) {
    if (socket->origin_ != nullptr) {
      socket->origin_->targets_.remove_first_occurrence_and_reorder(socket);
    }
    socket_or_null_by_id_[socket->id_] = nullptr;
  }
  for (MFOutputSocket *socket : node.outputs_) {
    for (MFInputSocket *other : socket->targets_) {
      other->origin_ = nullptr;
    }
    socket_or_null_by_id_[socket->id_] = nullptr;
  }
  node.destruct_sockets();
  if (node.is_dummy()) {
    MFDummyNode &dummy_node = node.as_dummy();
    dummy_node.~MFDummyNode();
    dummy_nodes_.remove_contained(&dummy_node);
  }
  else {
    MFFunctionNode &function_node = node.as_function();
    function_node.~MFFunctionNode();
    function_nodes_.remove_contained(&function_node);
  }
  node_or_null_by_id_[node.id_] = nullptr;
}

void MFNetwork::remove(Span<MFNode *> nodes)
{
  for (MFNode *node : nodes) {
    this->remove(*node);
  }
}

void MFNetwork::find_dependencies(Span<const MFInputSocket *> sockets,
                                  VectorSet<const MFOutputSocket *> &r_dummy_sockets,
                                  VectorSet<const MFInputSocket *> &r_unlinked_inputs) const
{
  Set<const MFNode *> visited_nodes;
  Stack<const MFInputSocket *> sockets_to_check;
  sockets_to_check.push_multiple(sockets);

  while (!sockets_to_check.is_empty()) {
    const MFInputSocket &socket = *sockets_to_check.pop();
    const MFOutputSocket *origin_socket = socket.origin();
    if (origin_socket == nullptr) {
      r_unlinked_inputs.add(&socket);
      continue;
    }

    const MFNode &origin_node = origin_socket->node();

    if (origin_node.is_dummy()) {
      r_dummy_sockets.add(origin_socket);
      continue;
    }

    if (visited_nodes.add(&origin_node)) {
      sockets_to_check.push_multiple(origin_node.inputs());
    }
  }
}

bool MFNetwork::have_dummy_or_unlinked_dependencies(Span<const MFInputSocket *> sockets) const
{
  VectorSet<const MFOutputSocket *> dummy_sockets;
  VectorSet<const MFInputSocket *> unlinked_inputs;
  this->find_dependencies(sockets, dummy_sockets, unlinked_inputs);
  return dummy_sockets.size() + unlinked_inputs.size() > 0;
}

std::string MFNetwork::to_dot(Span<const MFNode *> marked_nodes) const
{
  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);

  Map<const MFNode *, dot::NodeWithSocketsRef> dot_nodes;

  Vector<const MFNode *> all_nodes;
  all_nodes.extend(function_nodes_.as_span().cast<const MFNode *>());
  all_nodes.extend(dummy_nodes_.as_span().cast<const MFNode *>());

  for (const MFNode *node : all_nodes) {
    dot::Node &dot_node = digraph.new_node("");

    Vector<std::string> input_names, output_names;
    for (const MFInputSocket *socket : node->inputs_) {
      input_names.append(socket->name() + "(" + socket->data_type().to_string() + ")");
    }
    for (const MFOutputSocket *socket : node->outputs_) {
      output_names.append(socket->name() + " (" + socket->data_type().to_string() + ")");
    }

    dot::NodeWithSocketsRef dot_node_ref{dot_node, node->name(), input_names, output_names};
    dot_nodes.add_new(node, dot_node_ref);
  }

  for (const MFDummyNode *node : dummy_nodes_) {
    dot_nodes.lookup(node).node().set_background_color("#77EE77");
  }
  for (const MFNode *node : marked_nodes) {
    dot_nodes.lookup(node).node().set_background_color("#7777EE");
  }

  for (const MFNode *to_node : all_nodes) {
    dot::NodeWithSocketsRef to_dot_node = dot_nodes.lookup(to_node);

    for (const MFInputSocket *to_socket : to_node->inputs_) {
      const MFOutputSocket *from_socket = to_socket->origin_;
      if (from_socket != nullptr) {
        const MFNode *from_node = from_socket->node_;
        dot::NodeWithSocketsRef from_dot_node = dot_nodes.lookup(from_node);
        digraph.new_edge(from_dot_node.output(from_socket->index_),
                         to_dot_node.input(to_socket->index_));
      }
    }
  }

  return digraph.to_dot_string();
}

}  // namespace blender::fn
