/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_dot_export.hh"

#include "FN_lazy_function_graph.hh"

namespace blender::fn::lazy_function {

Graph::~Graph()
{
  for (Node *node : nodes_) {
    for (InputSocket *socket : node->inputs_) {
      std::destroy_at(socket);
    }
    for (OutputSocket *socket : node->outputs_) {
      std::destroy_at(socket);
    }
    std::destroy_at(node);
  }
}

FunctionNode &Graph::add_function(const LazyFunction &fn)
{
  const Span<Input> inputs = fn.inputs();
  const Span<Output> outputs = fn.outputs();

  FunctionNode &node = *allocator_.construct<FunctionNode>().release();
  node.fn_ = &fn;
  node.inputs_ = allocator_.construct_elements_and_pointer_array<InputSocket>(inputs.size());
  node.outputs_ = allocator_.construct_elements_and_pointer_array<OutputSocket>(outputs.size());

  for (const int i : inputs.index_range()) {
    InputSocket &socket = *node.inputs_[i];
    socket.index_in_node_ = i;
    socket.is_input_ = true;
    socket.node_ = &node;
    socket.type_ = inputs[i].type;
  }
  for (const int i : outputs.index_range()) {
    OutputSocket &socket = *node.outputs_[i];
    socket.index_in_node_ = i;
    socket.is_input_ = false;
    socket.node_ = &node;
    socket.type_ = outputs[i].type;
  }

  nodes_.append(&node);
  return node;
}

DummyNode &Graph::add_dummy(Span<const CPPType *> input_types,
                            Span<const CPPType *> output_types,
                            const DummyDebugInfo *debug_info)
{
  DummyNode &node = *allocator_.construct<DummyNode>().release();
  node.fn_ = nullptr;
  node.inputs_ = allocator_.construct_elements_and_pointer_array<InputSocket>(input_types.size());
  node.outputs_ = allocator_.construct_elements_and_pointer_array<OutputSocket>(
      output_types.size());
  node.debug_info_ = debug_info;

  for (const int i : input_types.index_range()) {
    InputSocket &socket = *node.inputs_[i];
    socket.index_in_node_ = i;
    socket.is_input_ = true;
    socket.node_ = &node;
    socket.type_ = input_types[i];
  }
  for (const int i : output_types.index_range()) {
    OutputSocket &socket = *node.outputs_[i];
    socket.index_in_node_ = i;
    socket.is_input_ = false;
    socket.node_ = &node;
    socket.type_ = output_types[i];
  }

  nodes_.append(&node);
  return node;
}

void Graph::add_link(OutputSocket &from, InputSocket &to)
{
  BLI_assert(to.origin_ == nullptr);
  BLI_assert(from.type_ == to.type_);
  to.origin_ = &from;
  from.targets_.append(&to);
}

void Graph::update_node_indices()
{
  for (const int i : nodes_.index_range()) {
    nodes_[i]->index_in_graph_ = i;
  }
}

bool Graph::node_indices_are_valid() const
{
  for (const int i : nodes_.index_range()) {
    if (nodes_[i]->index_in_graph_ != i) {
      return false;
    }
  }
  return true;
}

static const char *fallback_name = "No Name";

std::string Socket::name() const
{
  if (node_->is_function()) {
    const FunctionNode &fn_node = static_cast<const FunctionNode &>(*node_);
    const LazyFunction &fn = fn_node.function();
    if (is_input_) {
      return fn.input_name(index_in_node_);
    }
    return fn.output_name(index_in_node_);
  }
  const DummyNode &dummy_node = *static_cast<const DummyNode *>(node_);
  if (dummy_node.debug_info_) {
    if (is_input_) {
      return dummy_node.debug_info_->input_name(index_in_node_);
    }
    return dummy_node.debug_info_->output_name(index_in_node_);
  }
  return fallback_name;
}

std::string Node::name() const
{
  if (this->is_function()) {
    return fn_->name();
  }
  const DummyNode &dummy_node = *static_cast<const DummyNode *>(this);
  if (dummy_node.debug_info_) {
    return dummy_node.debug_info_->node_name();
  }
  return fallback_name;
}

std::string DummyDebugInfo::node_name() const
{
  return fallback_name;
}

std::string DummyDebugInfo::input_name(const int /*i*/) const
{
  return fallback_name;
}

std::string DummyDebugInfo::output_name(const int /*i*/) const
{
  return fallback_name;
}

std::string Graph::to_dot() const
{
  dot::DirectedGraph digraph;
  digraph.set_rankdir(dot::Attr_rankdir::LeftToRight);

  Map<const Node *, dot::NodeWithSocketsRef> dot_nodes;

  for (const Node *node : nodes_) {
    dot::Node &dot_node = digraph.new_node("");
    if (node->is_dummy()) {
      dot_node.set_background_color("lightblue");
    }
    else {
      dot_node.set_background_color("white");
    }

    Vector<std::string> input_names;
    Vector<std::string> output_names;
    for (const InputSocket *socket : node->inputs()) {
      input_names.append(socket->name());
    }
    for (const OutputSocket *socket : node->outputs()) {
      output_names.append(socket->name());
    }

    dot_nodes.add_new(node,
                      dot::NodeWithSocketsRef(dot_node, node->name(), input_names, output_names));
  }

  for (const Node *node : nodes_) {
    for (const InputSocket *socket : node->inputs()) {
      const dot::NodeWithSocketsRef &to_dot_node = dot_nodes.lookup(&socket->node());
      const dot::NodePort to_dot_port = to_dot_node.input(socket->index());

      if (const OutputSocket *origin = socket->origin()) {
        dot::NodeWithSocketsRef &from_dot_node = dot_nodes.lookup(&origin->node());
        digraph.new_edge(from_dot_node.output(origin->index()), to_dot_port);
      }
      else if (const void *default_value = socket->default_value()) {
        const CPPType &type = socket->type();
        std::string value_string;
        if (type.is_printable()) {
          value_string = type.to_string(default_value);
        }
        else {
          value_string = type.name();
        }
        dot::Node &default_value_dot_node = digraph.new_node(value_string);
        default_value_dot_node.set_shape(dot::Attr_shape::Ellipse);
        default_value_dot_node.attributes.set("color", "#00000055");
        digraph.new_edge(default_value_dot_node, to_dot_port);
      }
    }
  }

  return digraph.to_dot_string();
}

}  // namespace blender::fn::lazy_function
