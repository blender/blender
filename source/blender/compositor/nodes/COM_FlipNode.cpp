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

#include "COM_FlipNode.h"

#include "COM_FlipOperation.h"
#include "COM_ExecutionSystem.h"

FlipNode::FlipNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void FlipNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *inputSocket = this->getInputSocket(0);
	OutputSocket *outputSocket = this->getOutputSocket(0);
	FlipOperation *operation = new FlipOperation();
	switch (this->getbNode()->custom1) {
		case 0: /// @TODO: I didn't find any constants in the old implementation, should I introduce them.
			operation->setFlipX(true);
			operation->setFlipY(false);
			break;
		case 1:
			operation->setFlipX(false);
			operation->setFlipY(true);
			break;
		case 2:
			operation->setFlipX(true);
			operation->setFlipY(true);
			break;
	}
	
	inputSocket->relinkConnections(operation->getInputSocket(0), 0, graph);
	outputSocket->relinkConnections(operation->getOutputSocket(0));
	graph->addOperation(operation);
}
