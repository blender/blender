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
 *		Dalai Felinto
 */

#include "COM_SeparateYCCANode.h"
#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"
#include "COM_ConvertOperation.h"

SeparateYCCANode::SeparateYCCANode(bNode *editorNode) : SeparateRGBANode(editorNode)
{
	/* pass */
}

void SeparateYCCANode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	ConvertRGBToYCCOperation *operation = new ConvertRGBToYCCOperation();
	InputSocket *inputSocket = this->getInputSocket(0);

	bNode *node = this->getbNode();
	operation->setMode(node->custom1);

	if (inputSocket->isConnected()) {
		inputSocket->relinkConnections(operation->getInputSocket(0));
		addLink(graph, operation->getOutputSocket(), inputSocket);
	}
	graph->addOperation(operation);
	SeparateRGBANode::convertToOperations(graph, context);
}
