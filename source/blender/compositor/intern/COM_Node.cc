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
 *
 * Copyright 2011, Blender Foundation.
 */

#include <cstring>

#include "BKE_node.h"

#include "RNA_access.h"

#include "COM_ExecutionSystem.h"
#include "COM_NodeOperation.h"
#include "COM_TranslateOperation.h"

#include "COM_SocketProxyNode.h"

#include "COM_defines.h"

#include "COM_Node.h" /* own include */

namespace blender::compositor {

/**************
 **** Node ****
 **************/

Node::Node(bNode *editorNode, bool create_sockets)
    : m_editorNodeTree(nullptr),
      m_editorNode(editorNode),
      m_inActiveGroup(false),
      m_instanceKey(NODE_INSTANCE_KEY_NONE)
{
  if (create_sockets) {
    bNodeSocket *input = (bNodeSocket *)editorNode->inputs.first;
    while (input != nullptr) {
      DataType dt = DataType::Value;
      if (input->type == SOCK_RGBA) {
        dt = DataType::Color;
      }
      if (input->type == SOCK_VECTOR) {
        dt = DataType::Vector;
      }

      this->addInputSocket(dt, input);
      input = input->next;
    }
    bNodeSocket *output = (bNodeSocket *)editorNode->outputs.first;
    while (output != nullptr) {
      DataType dt = DataType::Value;
      if (output->type == SOCK_RGBA) {
        dt = DataType::Color;
      }
      if (output->type == SOCK_VECTOR) {
        dt = DataType::Vector;
      }

      this->addOutputSocket(dt, output);
      output = output->next;
    }
  }
}

Node::~Node()
{
  while (!this->outputs.is_empty()) {
    delete (this->outputs.pop_last());
  }
  while (!this->inputs.is_empty()) {
    delete (this->inputs.pop_last());
  }
}

void Node::addInputSocket(DataType datatype)
{
  this->addInputSocket(datatype, nullptr);
}

void Node::addInputSocket(DataType datatype, bNodeSocket *bSocket)
{
  NodeInput *socket = new NodeInput(this, bSocket, datatype);
  this->inputs.append(socket);
}

void Node::addOutputSocket(DataType datatype)
{
  this->addOutputSocket(datatype, nullptr);
}
void Node::addOutputSocket(DataType datatype, bNodeSocket *bSocket)
{
  NodeOutput *socket = new NodeOutput(this, bSocket, datatype);
  outputs.append(socket);
}

NodeOutput *Node::getOutputSocket(unsigned int index) const
{
  return outputs[index];
}

NodeInput *Node::getInputSocket(unsigned int index) const
{
  return inputs[index];
}

bNodeSocket *Node::getEditorInputSocket(int editorNodeInputSocketIndex)
{
  bNodeSocket *bSock = (bNodeSocket *)this->getbNode()->inputs.first;
  int index = 0;
  while (bSock != nullptr) {
    if (index == editorNodeInputSocketIndex) {
      return bSock;
    }
    index++;
    bSock = bSock->next;
  }
  return nullptr;
}
bNodeSocket *Node::getEditorOutputSocket(int editorNodeOutputSocketIndex)
{
  bNodeSocket *bSock = (bNodeSocket *)this->getbNode()->outputs.first;
  int index = 0;
  while (bSock != nullptr) {
    if (index == editorNodeOutputSocketIndex) {
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
    : m_node(node), m_editorSocket(b_socket), m_datatype(datatype), m_link(nullptr)
{
}

void NodeInput::setLink(NodeOutput *link)
{
  m_link = link;
}

float NodeInput::getEditorValueFloat() const
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)getNode()->getbNodeTree(), &RNA_NodeSocket, getbNodeSocket(), &ptr);
  return RNA_float_get(&ptr, "default_value");
}

void NodeInput::getEditorValueColor(float *value) const
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)getNode()->getbNodeTree(), &RNA_NodeSocket, getbNodeSocket(), &ptr);
  return RNA_float_get_array(&ptr, "default_value", value);
}

void NodeInput::getEditorValueVector(float *value) const
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)getNode()->getbNodeTree(), &RNA_NodeSocket, getbNodeSocket(), &ptr);
  return RNA_float_get_array(&ptr, "default_value", value);
}

/********************
 **** NodeOutput ****
 ********************/

NodeOutput::NodeOutput(Node *node, bNodeSocket *b_socket, DataType datatype)
    : m_node(node), m_editorSocket(b_socket), m_datatype(datatype)
{
}

float NodeOutput::getEditorValueFloat()
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)getNode()->getbNodeTree(), &RNA_NodeSocket, getbNodeSocket(), &ptr);
  return RNA_float_get(&ptr, "default_value");
}

void NodeOutput::getEditorValueColor(float *value)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)getNode()->getbNodeTree(), &RNA_NodeSocket, getbNodeSocket(), &ptr);
  return RNA_float_get_array(&ptr, "default_value", value);
}

void NodeOutput::getEditorValueVector(float *value)
{
  PointerRNA ptr;
  RNA_pointer_create((ID *)getNode()->getbNodeTree(), &RNA_NodeSocket, getbNodeSocket(), &ptr);
  return RNA_float_get_array(&ptr, "default_value", value);
}

}  // namespace blender::compositor
