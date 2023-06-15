/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_utildefines.h"

#include "COM_Node.h"
#include "COM_NodeOperationBuilder.h"
#include "COM_SetColorOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SocketProxyOperation.h"

#include "COM_NodeConverter.h" /* own include */

namespace blender::compositor {

NodeConverter::NodeConverter(NodeOperationBuilder *builder) : builder_(builder) {}

void NodeConverter::add_operation(NodeOperation *operation)
{
  builder_->add_operation(operation);
}

void NodeConverter::map_input_socket(NodeInput *node_socket, NodeOperationInput *operation_socket)
{
  builder_->map_input_socket(node_socket, operation_socket);
}

void NodeConverter::map_output_socket(NodeOutput *node_socket,
                                      NodeOperationOutput *operation_socket)
{
  builder_->map_output_socket(node_socket, operation_socket);
}

void NodeConverter::add_link(NodeOperationOutput *from, NodeOperationInput *to)
{
  builder_->add_link(from, to);
}

void NodeConverter::add_preview(NodeOperationOutput *output)
{
  builder_->add_preview(output);
}

void NodeConverter::add_node_input_preview(NodeInput *input)
{
  builder_->add_node_input_preview(input);
}

NodeOperation *NodeConverter::set_invalid_output(NodeOutput *output)
{
  /* this is a really bad situation - bring on the pink! - so artists know this is bad */
  const float warning_color[4] = {1.0f, 0.0f, 1.0f, 1.0f};

  SetColorOperation *operation = new SetColorOperation();
  operation->set_channels(warning_color);

  builder_->add_operation(operation);
  builder_->map_output_socket(output, operation->get_output_socket());

  return operation;
}

NodeOperationOutput *NodeConverter::add_input_proxy(NodeInput *input, bool use_conversion)
{
  SocketProxyOperation *proxy = new SocketProxyOperation(input->get_data_type(), use_conversion);
  builder_->add_operation(proxy);

  builder_->map_input_socket(input, proxy->get_input_socket(0));

  return proxy->get_output_socket();
}

NodeOperationInput *NodeConverter::add_output_proxy(NodeOutput *output, bool use_conversion)
{
  SocketProxyOperation *proxy = new SocketProxyOperation(output->get_data_type(), use_conversion);
  builder_->add_operation(proxy);

  builder_->map_output_socket(output, proxy->get_output_socket());

  return proxy->get_input_socket(0);
}

void NodeConverter::add_input_value(NodeOperationInput *input, float value)
{
  SetValueOperation *operation = new SetValueOperation();
  operation->set_value(value);

  builder_->add_operation(operation);
  builder_->add_link(operation->get_output_socket(), input);
}

void NodeConverter::add_input_color(NodeOperationInput *input, const float value[4])
{
  SetColorOperation *operation = new SetColorOperation();
  operation->set_channels(value);

  builder_->add_operation(operation);
  builder_->add_link(operation->get_output_socket(), input);
}

void NodeConverter::add_input_vector(NodeOperationInput *input, const float value[3])
{
  SetVectorOperation *operation = new SetVectorOperation();
  operation->set_vector(value);

  builder_->add_operation(operation);
  builder_->add_link(operation->get_output_socket(), input);
}

void NodeConverter::add_output_value(NodeOutput *output, float value)
{
  SetValueOperation *operation = new SetValueOperation();
  operation->set_value(value);

  builder_->add_operation(operation);
  builder_->map_output_socket(output, operation->get_output_socket());
}

void NodeConverter::add_output_color(NodeOutput *output, const float value[4])
{
  SetColorOperation *operation = new SetColorOperation();
  operation->set_channels(value);

  builder_->add_operation(operation);
  builder_->map_output_socket(output, operation->get_output_socket());
}

void NodeConverter::add_output_vector(NodeOutput *output, const float value[3])
{
  SetVectorOperation *operation = new SetVectorOperation();
  operation->set_vector(value);

  builder_->add_operation(operation);
  builder_->map_output_socket(output, operation->get_output_socket());
}

void NodeConverter::register_viewer(ViewerOperation *viewer)
{
  builder_->register_viewer(viewer);
}

ViewerOperation *NodeConverter::active_viewer() const
{
  return builder_->active_viewer();
}

}  // namespace blender::compositor
