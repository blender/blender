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

#include "COM_BokehBlurNode.h"
#include "DNA_scene_types.h"
#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_BokehBlurOperation.h"
#include "COM_VariableSizeBokehBlurOperation.h"
#include "COM_ConvertDepthToRadiusOperation.h"

BokehBlurNode::BokehBlurNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void BokehBlurNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	BokehBlurOperation *operation = new BokehBlurOperation();
	InputSocket *inputSizeSocket = this->getInputSocket(2);
	bool connectedSizeSocket = inputSizeSocket->isConnected();

	const bNodeSocket *sock = this->getInputSocket(2)->getbNodeSocket();
	const float size = ((const bNodeSocketValueFloat *)sock->default_value)->value;

	this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, graph);
	this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), 1, graph);
	this->getInputSocket(2)->relinkConnections(operation->getInputSocket(3), 2, graph);
	this->getInputSocket(3)->relinkConnections(operation->getInputSocket(2), 3, graph);
	//operation->setSize(((bNodeSocketValueFloat *)this->getInputSocket(2)->getbNodeSocket()->default_value)->value);
	operation->setQuality(context->getQuality());
	operation->setbNode(this->getbNode());
	graph->addOperation(operation);
	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());

	if (!connectedSizeSocket) {
		operation->setSize(size);
	}
}
