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

#include "COM_LuminanceMatteNode.h"
#include "BKE_node.h"
#include "COM_LuminanceMatteOperation.h"
#include "COM_ConvertRGBToYUVOperation.h"
#include "COM_SetAlphaOperation.h"

LuminanceMatteNode::LuminanceMatteNode(bNode *editorNode): Node(editorNode)
{}

void LuminanceMatteNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context) {
	InputSocket *inputSocket = this->getInputSocket(0);
	OutputSocket *outputSocketImage = this->getOutputSocket(0);
	OutputSocket *outputSocketMatte = this->getOutputSocket(1);

	ConvertRGBToYUVOperation *rgbToYUV = new ConvertRGBToYUVOperation();
	LuminanceMatteOperation *operationSet = new LuminanceMatteOperation();
	bNode* editorsnode = getbNode();
	operationSet->setSettings((NodeChroma*)editorsnode->storage);

	inputSocket->relinkConnections(rgbToYUV->getInputSocket(0), true, 0, graph);
	addLink(graph, rgbToYUV->getOutputSocket(), operationSet->getInputSocket(0));

	if (outputSocketMatte->isConnected()) {
		outputSocketMatte->relinkConnections(operationSet->getOutputSocket(0));
	}

	graph->addOperation(rgbToYUV);
	graph->addOperation(operationSet);

	SetAlphaOperation *operation = new SetAlphaOperation();
	addLink(graph, rgbToYUV->getInputSocket(0)->getConnection()->getFromSocket(), operation->getInputSocket(0));
	addLink(graph, operationSet->getOutputSocket(), operation->getInputSocket(1));
	graph->addOperation(operation);
	addPreviewOperation(graph, operation->getOutputSocket(), 9);

	if (outputSocketImage->isConnected()) {
		outputSocketImage->relinkConnections(operation->getOutputSocket());
	}
}
