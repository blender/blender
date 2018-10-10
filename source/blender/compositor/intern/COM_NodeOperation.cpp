/*
 * Copyright 2011, Blender Foundation.
 *
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
 * Contributor:
 *		Jeroen Bakker
 *		Monique Dewanchand
 */

#include <typeinfo>
#include <stdio.h>

#include "COM_defines.h"
#include "COM_ExecutionSystem.h"

#include "COM_NodeOperation.h" /* own include */

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
	this->m_btree = NULL;
}

NodeOperation::~NodeOperation()
{
	while (!this->m_outputs.empty()) {
		delete (this->m_outputs.back());
		this->m_outputs.pop_back();
	}
	while (!this->m_inputs.empty()) {
		delete (this->m_inputs.back());
		this->m_inputs.pop_back();
	}
}

NodeOperationOutput *NodeOperation::getOutputSocket(unsigned int index) const
{
	BLI_assert(index < m_outputs.size());
	return m_outputs[index];
}

NodeOperationInput *NodeOperation::getInputSocket(unsigned int index) const
{
	BLI_assert(index < m_inputs.size());
	return m_inputs[index];
}

void NodeOperation::addInputSocket(DataType datatype, InputResizeMode resize_mode)
{
	NodeOperationInput *socket = new NodeOperationInput(this, datatype, resize_mode);
	m_inputs.push_back(socket);
}

void NodeOperation::addOutputSocket(DataType datatype)
{
	NodeOperationOutput *socket = new NodeOperationOutput(this, datatype);
	m_outputs.push_back(socket);
}

void NodeOperation::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	unsigned int temp[2];
	unsigned int temp2[2];

	for (unsigned int index = 0; index < m_inputs.size(); index++) {
		NodeOperationInput *input = m_inputs[index];
		if (input->isConnected()) {
			if (index == this->m_resolutionInputSocketIndex) {
				input->determineResolution(resolution, preferredResolution);
				temp2[0] = resolution[0];
				temp2[1] = resolution[1];
				break;
			}
		}
	}
	for (unsigned int index = 0; index < m_inputs.size(); index++) {
		NodeOperationInput *input = m_inputs[index];
		if (input->isConnected()) {
			if (index != this->m_resolutionInputSocketIndex) {
				input->determineResolution(temp, temp2);
			}
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
	if (input && input->isConnected())
		return &input->getLink()->getOperation();
	else
		return NULL;
}

void NodeOperation::getConnectedInputSockets(Inputs *sockets)
{
	for (Inputs::const_iterator it = m_inputs.begin(); it != m_inputs.end(); ++it) {
		NodeOperationInput *input = *it;
		if (input->isConnected()) {
			sockets->push_back(input);
		}
	}
}

bool NodeOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	if (isInputOperation()) {
		BLI_rcti_init(output, input->xmin, input->xmax, input->ymin, input->ymax);
		return false;
	}
	else {
		rcti tempOutput;
		bool first = true;
		for (int i = 0; i < getNumberOfInputSockets(); i ++) {
			NodeOperation *inputOperation = this->getInputOperation(i);
			if (inputOperation && inputOperation->determineDependingAreaOfInterest(input, readOperation, &tempOutput)) {
				if (first) {
					output->xmin = tempOutput.xmin;
					output->ymin = tempOutput.ymin;
					output->xmax = tempOutput.xmax;
					output->ymax = tempOutput.ymax;
					first = false;
				}
				else {
					output->xmin = min(output->xmin, tempOutput.xmin);
					output->ymin = min(output->ymin, tempOutput.ymin);
					output->xmax = max(output->xmax, tempOutput.xmax);
					output->ymax = max(output->ymax, tempOutput.ymax);
				}
			}
		}
		return !first;
	}
}


/*****************
 **** OpInput ****
 *****************/

NodeOperationInput::NodeOperationInput(NodeOperation *op, DataType datatype, InputResizeMode resizeMode) :
    m_operation(op),
    m_datatype(datatype),
    m_resizeMode(resizeMode),
    m_link(NULL)
{
}

SocketReader *NodeOperationInput::getReader()
{
	if (isConnected()) {
		return &m_link->getOperation();
	}
	else {
		return NULL;
	}
}

void NodeOperationInput::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	if (m_link)
		m_link->determineResolution(resolution, preferredResolution);
}


/******************
 **** OpOutput ****
 ******************/

NodeOperationOutput::NodeOperationOutput(NodeOperation *op, DataType datatype) :
    m_operation(op),
    m_datatype(datatype)
{
}

void NodeOperationOutput::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
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
