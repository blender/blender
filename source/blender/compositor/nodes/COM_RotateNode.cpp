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

#include "COM_RotateNode.h"

#include "COM_RotateOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_SetSamplerOperation.h"

RotateNode::RotateNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void RotateNode::convertToOperations(ExecutionSystem *system, CompositorContext *context)
{
	InputSocket *inputSocket = this->getInputSocket(0);
	InputSocket *inputDegreeSocket = this->getInputSocket(1);
	OutputSocket *outputSocket = this->getOutputSocket(0);
	RotateOperation *operation = new RotateOperation();
	SetSamplerOperation *sampler = new SetSamplerOperation();

	switch (this->getbNode()->custom1) {
		case 0:
			sampler->setSampler(COM_PS_NEAREST);
			break;
		case 1:
			sampler->setSampler(COM_PS_BILINEAR);
			break;
		case 2:
			sampler->setSampler(COM_PS_BICUBIC);
			break;
	
	}

	addLink(system, sampler->getOutputSocket(), operation->getInputSocket(0));
	
	inputSocket->relinkConnections(sampler->getInputSocket(0), 0, system);
	inputDegreeSocket->relinkConnections(operation->getInputSocket(1), 1, system);
	outputSocket->relinkConnections(operation->getOutputSocket(0));
	system->addOperation(sampler);
	system->addOperation(operation);
	
}
