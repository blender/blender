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

#include "COM_ColorSpillNode.h"
#include "BKE_node.h"
#include "COM_ColorSpillOperation.h"

ColorSpillNode::ColorSpillNode(bNode *editorNode): Node(editorNode)
{}

void ColorSpillNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *inputSocketImage = this->getInputSocket(0);
	InputSocket *inputSocketFac = this->getInputSocket(1);
	OutputSocket *outputSocketImage = this->getOutputSocket(0);

	bNode *editorsnode = getbNode();

	
	ColorSpillOperation *operation;
	if (editorsnode->custom2 == 0) {
		// Simple color spill
		operation = new ColorSpillOperation();
	}
	else {
		// Average color spill
		operation = new ColorSpillAverageOperation();
	}
	operation->setSettings((NodeColorspill*)editorsnode->storage);
	operation->setSpillChannel(editorsnode->custom1-1); // Channel for spilling
	

	inputSocketImage->relinkConnections(operation->getInputSocket(0), 0, graph);
	inputSocketFac->relinkConnections(operation->getInputSocket(1), 1, graph);
	
	outputSocketImage->relinkConnections(operation->getOutputSocket());
	graph->addOperation(operation);
}
