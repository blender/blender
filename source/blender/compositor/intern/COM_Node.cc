/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node.hh"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "COM_Node.h" /* own include */

namespace blender::compositor {

/**************
 **** Node ****
 **************/

Node::Node(bNode *editor_node, bool create_sockets)
    : editor_node_tree_(nullptr),
      editor_node_(editor_node),
      in_active_group_(false),
      instance_key_(NODE_INSTANCE_KEY_NONE)
{
  if (create_sockets) {
    bNodeSocket *input = (bNodeSocket *)editor_node->inputs.first;
    while (input != nullptr) {
      DataType dt = DataType::Value;
      if (input->type == SOCK_RGBA) {
        dt = DataType::Color;
      }
      if (input->type == SOCK_VECTOR) {
        dt = DataType::Vector;
      }

      this->add_input_socket(dt, input);
      input = input->next;
    }
    bNodeSocket *output = (bNodeSocket *)editor_node->outputs.first;
    while (output != nullptr) {
      DataType dt = DataType::Value;
      if (output->type == SOCK_RGBA) {
        dt = DataType::Color;
      }
      if (output->type == SOCK_VECTOR) {
        dt = DataType::Vector;
      }

      this->add_output_socket(dt, output);
      output = output->next;
    }
  }
}

Node::~Node()
{
  while (!outputs_.is_empty()) {
    delete outputs_.pop_last();
  }
  while (!inputs_.is_empty()) {
    delete inputs_.pop_last();
  }
}

void Node::add_input_socket(DataType datatype)
{
  this->add_input_socket(datatype, nullptr);
}

void Node::add_input_socket(DataType datatype, bNodeSocket *bSocket)
{
  NodeInput *socket = new NodeInput(this, bSocket, datatype);
  inputs_.append(socket);
}

void Node::add_output_socket(DataType datatype)
{
  this->add_output_socket(datatype, nullptr);
}
void Node::add_output_socket(DataType datatype, bNodeSocket *bSocket)
{
  NodeOutput *socket = new NodeOutput(this, bSocket, datatype);
  outputs_.append(socket);
}

NodeOutput *Node::get_output_socket(uint index) const
{
  return outputs_[index];
}

NodeInput *Node::get_input_socket(uint index) const
{
  return inputs_[index];
}

bNodeSocket *Node::get_editor_input_socket(int editor_node_input_socket_index)
{
  bNodeSocket *bSock = (bNodeSocket *)this->get_bnode()->inputs.first;
  int index = 0;
  while (bSock != nullptr) {
    if (index == editor_node_input_socket_index) {
      return bSock;
    }
    index++;
    bSock = bSock->next;
  }
  return nullptr;
}
bNodeSocket *Node::get_editor_output_socket(int editor_node_output_socket_index)
{
  bNodeSocket *bSock = (bNodeSocket *)this->get_bnode()->outputs.first;
  int index = 0;
  while (bSock != nullptr) {
    if (index == editor_node_output_socket_index) {
      return bSock;
    }
    index++;
    bSock = bSock->next;
  }
  return nullptr;
}

/*******************
 **** NodeInput ****
 *******************/

NodeInput::NodeInput(Node *node, bNodeSocket *b_socket, DataType datatype)
    : node_(node), editor_socket_(b_socket), datatype_(datatype), link_(nullptr)
{
}

void NodeInput::set_link(NodeOutput *link)
{
  link_ = link;
}

float NodeInput::get_editor_value_float() const
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)get_node()->get_bnodetree(), &RNA_NodeSocket, get_bnode_socket(), &ptr);
  return RNA_float_get(&ptr, "default_value");
}

void NodeInput::get_editor_value_color(float *value) const
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)get_node()->get_bnodetree(), &RNA_NodeSocket, get_bnode_socket(), &ptr);
  return RNA_float_get_array(&ptr, "default_value", value);
}

void NodeInput::get_editor_value_vector(float *value) const
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)get_node()->get_bnodetree(), &RNA_NodeSocket, get_bnode_socket(), &ptr);
  return RNA_float_get_array(&ptr, "default_value", value);
}

/********************
 **** NodeOutput ****
 ********************/

NodeOutput::NodeOutput(Node *node, bNodeSocket *b_socket, DataType datatype)
    : node_(node), editor_socket_(b_socket), datatype_(datatype)
{
}

float NodeOutput::get_editor_value_float()
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)get_node()->get_bnodetree(), &RNA_NodeSocket, get_bnode_socket(), &ptr);
  return RNA_float_get(&ptr, "default_value");
}

void NodeOutput::get_editor_value_color(float *value)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)get_node()->get_bnodetree(), &RNA_NodeSocket, get_bnode_socket(), &ptr);
  return RNA_float_get_array(&ptr, "default_value", value);
}

void NodeOutput::get_editor_value_vector(float *value)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)get_node()->get_bnodetree(), &RNA_NodeSocket, get_bnode_socket(), &ptr);
  return RNA_float_get_array(&ptr, "default_value", value);
}

}  // namespace blender::compositor
