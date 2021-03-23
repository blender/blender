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

#include <cstdio>
#include <typeinfo>

#include "COM_ExecutionSystem.h"
#include "COM_defines.h"

#include "COM_NodeOperation.h" /* own include */

namespace blender::compositor {

/*******************
 **** NodeOperation ****
 *******************/

NodeOperation::NodeOperation()
{
  this->m_resolutionInputSocketIndex = 0;
  this->m_complex = false;
  this->m_width = 0;
  this->m_height = 0;
  this->m_isResolutionSet = false;
  this->m_openCL = false;
  this->m_btree = nullptr;
}

NodeOperationOutput *NodeOperation::getOutputSocket(unsigned int index)
{
  return &m_outputs[index];
}

NodeOperationInput *NodeOperation::getInputSocket(unsigned int index)
{
  return &m_inputs[index];
}

void NodeOperation::addInputSocket(DataType datatype, ResizeMode resize_mode)
{
  m_inputs.append(NodeOperationInput(this, datatype, resize_mode));
}

void NodeOperation::addOutputSocket(DataType datatype)
{
  m_outputs.append(NodeOperationOutput(this, datatype));
}

void NodeOperation::determineResolution(unsigned int resolution[2],
                                        unsigned int preferredResolution[2])
{
  if (m_resolutionInputSocketIndex < m_inputs.size()) {
    NodeOperationInput &input = m_inputs[m_resolutionInputSocketIndex];
    input.determineResolution(resolution, preferredResolution);
  }
  unsigned int temp2[2] = {resolution[0], resolution[1]};

  unsigned int temp[2];
  for (unsigned int index = 0; index < m_inputs.size(); index++) {
    if (index == this->m_resolutionInputSocketIndex) {
      continue;
    }
    NodeOperationInput &input = m_inputs[index];
    if (input.isConnected()) {
      input.determineResolution(temp, temp2);
    }
  }
}

void NodeOperation::setResolutionInputSocketIndex(unsigned int index)
{
  this->m_resolutionInputSocketIndex = index;
}
void NodeOperation::initExecution()
{
  /* pass */
}

void NodeOperation::initMutex()
{
  BLI_mutex_init(&this->m_mutex);
}

void NodeOperation::lockMutex()
{
  BLI_mutex_lock(&this->m_mutex);
}

void NodeOperation::unlockMutex()
{
  BLI_mutex_unlock(&this->m_mutex);
}

void NodeOperation::deinitMutex()
{
  BLI_mutex_end(&this->m_mutex);
}

void NodeOperation::deinitExecution()
{
  /* pass */
}
SocketReader *NodeOperation::getInputSocketReader(unsigned int inputSocketIndex)
{
  return this->getInputSocket(inputSocketIndex)->getReader();
}

NodeOperation *NodeOperation::getInputOperation(unsigned int inputSocketIndex)
{
  NodeOperationInput *input = getInputSocket(inputSocketIndex);
  if (input && input->isConnected()) {
    return &input->getLink()->getOperation();
  }

  return nullptr;
}

bool NodeOperation::determineDependingAreaOfInterest(rcti *input,
                                                     ReadBufferOperation *readOperation,
                                                     rcti *output)
{
  if (isInputOperation()) {
    BLI_rcti_init(output, input->xmin, input->xmax, input->ymin, input->ymax);
    return false;
  }

  rcti tempOutput;
  bool first = true;
  for (int i = 0; i < getNumberOfInputSockets(); i++) {
    NodeOperation *inputOperation = this->getInputOperation(i);
    if (inputOperation &&
        inputOperation->determineDependingAreaOfInterest(input, readOperation, &tempOutput)) {
      if (first) {
        output->xmin = tempOutput.xmin;
        output->ymin = tempOutput.ymin;
        output->xmax = tempOutput.xmax;
        output->ymax = tempOutput.ymax;
        first = false;
      }
      else {
        output->xmin = MIN2(output->xmin, tempOutput.xmin);
        output->ymin = MIN2(output->ymin, tempOutput.ymin);
        output->xmax = MAX2(output->xmax, tempOutput.xmax);
        output->ymax = MAX2(output->ymax, tempOutput.ymax);
      }
    }
  }
  return !first;
}

/*****************
 **** OpInput ****
 *****************/

NodeOperationInput::NodeOperationInput(NodeOperation *op, DataType datatype, ResizeMode resizeMode)
    : m_operation(op), m_datatype(datatype), m_resizeMode(resizeMode), m_link(nullptr)
{
}

SocketReader *NodeOperationInput::getReader()
{
  if (isConnected()) {
    return &m_link->getOperation();
  }

  return nullptr;
}

void NodeOperationInput::determineResolution(unsigned int resolution[2],
                                             unsigned int preferredResolution[2])
{
  if (m_link) {
    m_link->determineResolution(resolution, preferredResolution);
  }
}

/******************
 **** OpOutput ****
 ******************/

NodeOperationOutput::NodeOperationOutput(NodeOperation *op, DataType datatype)
    : m_operation(op), m_datatype(datatype)
{
}

void NodeOperationOutput::determineResolution(unsigned int resolution[2],
                                              unsigned int preferredResolution[2])
{
  NodeOperation &operation = getOperation();
  if (operation.isResolutionSet()) {
    resolution[0] = operation.getWidth();
    resolution[1] = operation.getHeight();
  }
  else {
    operation.determineResolution(resolution, preferredResolution);
    operation.setResolution(resolution);
  }
}

}  // namespace blender::compositor
