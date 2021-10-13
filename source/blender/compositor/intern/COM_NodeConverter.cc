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
 * Copyright 2013, Blender Foundation.
 */

#include "BLI_utildefines.h"

#include "COM_Node.h"
#include "COM_NodeOperationBuilder.h"
#include "COM_SetColorOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SocketProxyOperation.h"

#include "COM_NodeConverter.h" /* own include */

namespace blender::compositor {

NodeConverter::NodeConverter(NodeOperationBuilder *builder) : builder_(builder)
{
}

void NodeConverter::addOperation(NodeOperation *operation)
{
  builder_->addOperation(operation);
}

void NodeConverter::mapInputSocket(NodeInput *node_socket, NodeOperationInput *operation_socket)
{
  builder_->mapInputSocket(node_socket, operation_socket);
}

void NodeConverter::mapOutputSocket(NodeOutput *node_socket, NodeOperationOutput *operation_socket)
{
  builder_->mapOutputSocket(node_socket, operation_socket);
}

void NodeConverter::addLink(NodeOperationOutput *from, NodeOperationInput *to)
{
  builder_->addLink(from, to);
}

void NodeConverter::addPreview(NodeOperationOutput *output)
{
  builder_->addPreview(output);
}

void NodeConverter::addNodeInputPreview(NodeInput *input)
{
  builder_->addNodeInputPreview(input);
}

NodeOperation *NodeConverter::setInvalidOutput(NodeOutput *output)
{
  /* this is a really bad situation - bring on the pink! - so artists know this is bad */
  const float warning_color[4] = {1.0f, 0.0f, 1.0f, 1.0f};

  SetColorOperation *operation = new SetColorOperation();
  operation->setChannels(warning_color);

  builder_->addOperation(operation);
  builder_->mapOutputSocket(output, operation->getOutputSocket());

  return operation;
}

NodeOperationOutput *NodeConverter::addInputProxy(NodeInput *input, bool use_conversion)
{
  SocketProxyOperation *proxy = new SocketProxyOperation(input->getDataType(), use_conversion);
  builder_->addOperation(proxy);

  builder_->mapInputSocket(input, proxy->getInputSocket(0));

  return proxy->getOutputSocket();
}

NodeOperationInput *NodeConverter::addOutputProxy(NodeOutput *output, bool use_conversion)
{
  SocketProxyOperation *proxy = new SocketProxyOperation(output->getDataType(), use_conversion);
  builder_->addOperation(proxy);

  builder_->mapOutputSocket(output, proxy->getOutputSocket());

  return proxy->getInputSocket(0);
}

void NodeConverter::addInputValue(NodeOperationInput *input, float value)
{
  SetValueOperation *operation = new SetValueOperation();
  operation->setValue(value);

  builder_->addOperation(operation);
  builder_->addLink(operation->getOutputSocket(), input);
}

void NodeConverter::addInputColor(NodeOperationInput *input, const float value[4])
{
  SetColorOperation *operation = new SetColorOperation();
  operation->setChannels(value);

  builder_->addOperation(operation);
  builder_->addLink(operation->getOutputSocket(), input);
}

void NodeConverter::addInputVector(NodeOperationInput *input, const float value[3])
{
  SetVectorOperation *operation = new SetVectorOperation();
  operation->setVector(value);

  builder_->addOperation(operation);
  builder_->addLink(operation->getOutputSocket(), input);
}

void NodeConverter::addOutputValue(NodeOutput *output, float value)
{
  SetValueOperation *operation = new SetValueOperation();
  operation->setValue(value);

  builder_->addOperation(operation);
  builder_->mapOutputSocket(output, operation->getOutputSocket());
}

void NodeConverter::addOutputColor(NodeOutput *output, const float value[4])
{
  SetColorOperation *operation = new SetColorOperation();
  operation->setChannels(value);

  builder_->addOperation(operation);
  builder_->mapOutputSocket(output, operation->getOutputSocket());
}

void NodeConverter::addOutputVector(NodeOutput *output, const float value[3])
{
  SetVectorOperation *operation = new SetVectorOperation();
  operation->setVector(value);

  builder_->addOperation(operation);
  builder_->mapOutputSocket(output, operation->getOutputSocket());
}

void NodeConverter::registerViewer(ViewerOperation *viewer)
{
  builder_->registerViewer(viewer);
}

ViewerOperation *NodeConverter::active_viewer() const
{
  return builder_->active_viewer();
}

}  // namespace blender::compositor
