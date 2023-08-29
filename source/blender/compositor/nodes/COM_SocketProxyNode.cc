/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SocketProxyNode.h"
#include "COM_ReadBufferOperation.h"
#include "COM_WriteBufferOperation.h"

namespace blender::compositor {

SocketProxyNode::SocketProxyNode(bNode *editor_node,
                                 bNodeSocket *editor_input,
                                 bNodeSocket *editor_output,
                                 bool use_conversion)
    : Node(editor_node, false), use_conversion_(use_conversion)
{
  DataType dt;

  dt = DataType::Value;
  if (editor_input->type == SOCK_RGBA) {
    dt = DataType::Color;
  }
  if (editor_input->type == SOCK_VECTOR) {
    dt = DataType::Vector;
  }
  this->add_input_socket(dt, editor_input);

  dt = DataType::Value;
  if (editor_output->type == SOCK_RGBA) {
    dt = DataType::Color;
  }
  if (editor_output->type == SOCK_VECTOR) {
    dt = DataType::Vector;
  }
  this->add_output_socket(dt, editor_output);
}

void SocketProxyNode::convert_to_operations(NodeConverter &converter,
                                            const CompositorContext & /*context*/) const
{
  NodeOperationOutput *proxy_output = converter.add_input_proxy(get_input_socket(0),
                                                                use_conversion_);
  converter.map_output_socket(get_output_socket(), proxy_output);
}

SocketBufferNode::SocketBufferNode(bNode *editor_node,
                                   bNodeSocket *editor_input,
                                   bNodeSocket *editor_output)
    : Node(editor_node, false)
{
  DataType dt;

  dt = DataType::Value;
  if (editor_input->type == SOCK_RGBA) {
    dt = DataType::Color;
  }
  if (editor_input->type == SOCK_VECTOR) {
    dt = DataType::Vector;
  }
  this->add_input_socket(dt, editor_input);

  dt = DataType::Value;
  if (editor_output->type == SOCK_RGBA) {
    dt = DataType::Color;
  }
  if (editor_output->type == SOCK_VECTOR) {
    dt = DataType::Vector;
  }
  this->add_output_socket(dt, editor_output);
}

void SocketBufferNode::convert_to_operations(NodeConverter &converter,
                                             const CompositorContext & /*context*/) const
{
  NodeOutput *output = this->get_output_socket(0);
  NodeInput *input = this->get_input_socket(0);

  DataType datatype = output->get_data_type();
  WriteBufferOperation *write_operation = new WriteBufferOperation(datatype);
  ReadBufferOperation *read_operation = new ReadBufferOperation(datatype);
  read_operation->set_memory_proxy(write_operation->get_memory_proxy());
  converter.add_operation(write_operation);
  converter.add_operation(read_operation);

  converter.map_input_socket(input, write_operation->get_input_socket(0));
  converter.map_output_socket(output, read_operation->get_output_socket());
}

}  // namespace blender::compositor
