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

#include "COM_Debug.h"

#include "COM_NodeOperation.h"
#include "COM_NodeOperationBuilder.h"
#include "COM_SetColorOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SocketProxyOperation.h"

#include "COM_NodeConverter.h" /* own include */

NodeConverter::NodeConverter(NodeOperationBuilder *builder) : m_builder(builder)
{
}

void NodeConverter::addOperation(NodeOperation *operation)
{
  m_builder->addOperation(operation);
}

void NodeConverter::mapInputSocket(NodeInput *node_socket, NodeOperationInput *operation_socket)
{
  m_builder->mapInputSocket(node_socket, operation_socket);
}

void NodeConverter::mapOutputSocket(NodeOutput *node_socket, NodeOperationOutput *operation_socket)
{
  m_builder->mapOutputSocket(node_socket, operation_socket);
}

void NodeConverter::addLink(NodeOperationOutput *from, NodeOperationInput *to)
{
  m_builder->addLink(from, to);
}

void NodeConverter::addPreview(NodeOperationOutput *output)
{
  m_builder->addPreview(output);
}

void NodeConverter::addNodeInputPreview(NodeInput *input)
{
  m_builder->addNodeInputPreview(input);
}

NodeOperation *NodeConverter::setInvalidOutput(NodeOutput *output)
{
  /* this is a really bad situation - bring on the pink! - so artists know this is bad */
  const float warning_color[4] = {1.0f, 0.0f, 1.0f, 1.0f};

  SetColorOperation *operation = new SetColorOperation();
  operation->setChannels(warning_color);

  m_builder->addOperation(operation);
  m_builder->mapOutputSocket(output, operation->getOutputSocket());

  return operation;
}

NodeOperationOutput *NodeConverter::addInputProxy(NodeInput *input, bool use_conversion)
{
  SocketProxyOperation *proxy = new SocketProxyOperation(input->getDataType(), use_conversion);
  m_builder->addOperation(proxy);

  m_builder->mapInputSocket(input, proxy->getInputSocket(0));

  return proxy->getOutputSocket();
}

NodeOperationInput *NodeConverter::addOutputProxy(NodeOutput *output, bool use_conversion)
{
  SocketProxyOperation *proxy = new SocketProxyOperation(output->getDataType(), use_conversion);
  m_builder->addOperation(proxy);

  m_builder->mapOutputSocket(output, proxy->getOutputSocket());

  return proxy->getInputSocket(0);
}

void NodeConverter::addInputValue(NodeOperationInput *input, float value)
{
  SetValueOperation *operation = new SetValueOperation();
  operation->setValue(value);

  m_builder->addOperation(operation);
  m_builder->addLink(operation->getOutputSocket(), input);
}

void NodeConverter::addInputColor(NodeOperationInput *input, const float value[4])
{
  SetColorOperation *operation = new SetColorOperation();
  operation->setChannels(value);

  m_builder->addOperation(operation);
  m_builder->addLink(operation->getOutputSocket(), input);
}

void NodeConverter::addInputVector(NodeOperationInput *input, const float value[3])
{
  SetVectorOperation *operation = new SetVectorOperation();
  operation->setVector(value);

  m_builder->addOperation(operation);
  m_builder->addLink(operation->getOutputSocket(), input);
}

void NodeConverter::addOutputValue(NodeOutput *output, float value)
{
  SetValueOperation *operation = new SetValueOperation();
  operation->setValue(value);

  m_builder->addOperation(operation);
  m_builder->mapOutputSocket(output, operation->getOutputSocket());
}

void NodeConverter::addOutputColor(NodeOutput *output, const float value[4])
{
  SetColorOperation *operation = new SetColorOperation();
  operation->setChannels(value);

  m_builder->addOperation(operation);
  m_builder->mapOutputSocket(output, operation->getOutputSocket());
}

void NodeConverter::addOutputVector(NodeOutput *output, const float value[3])
{
  SetVectorOperation *operation = new SetVectorOperation();
  operation->setVector(value);

  m_builder->addOperation(operation);
  m_builder->mapOutputSocket(output, operation->getOutputSocket());
}

void NodeConverter::registerViewer(ViewerOperation *viewer)
{
  m_builder->registerViewer(viewer);
}

ViewerOperation *NodeConverter::active_viewer() const
{
  return m_builder->active_viewer();
}
