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
 * Contributor: Campbell Barton
 */

#include "COM_DespeckleNode.h"
#include "DNA_scene_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_DespeckleOperation.h"
#include "BLI_math.h"

DespeckleNode::DespeckleNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void DespeckleNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	bNode *editorNode = this->getbNode();
	InputSocket *inputSocket = this->getInputSocket(0);
	InputSocket *inputImageSocket = this->getInputSocket(1);
	OutputSocket *outputSocket = this->getOutputSocket(0);
	DespeckleOperation *operation = new DespeckleOperation();

	operation->setbNode(editorNode);
	operation->setThreshold(editorNode->custom3);
	operation->setThresholdNeighbor(editorNode->custom4);

	inputImageSocket->relinkConnections(operation->getInputSocket(0), 1, graph);
	inputSocket->relinkConnections(operation->getInputSocket(1), 0, graph);
	outputSocket->relinkConnections(operation->getOutputSocket());
	addPreviewOperation(graph, context, operation->getOutputSocket(0));

	graph->addOperation(operation);
}
