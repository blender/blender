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

#include "COM_DistanceMatteNode.h"
#include "BKE_node.h"
#include "COM_DistanceRGBMatteOperation.h"
#include "COM_DistanceYCCMatteOperation.h"
#include "COM_SetAlphaOperation.h"
#include "COM_ConvertOperation.h"

DistanceMatteNode::DistanceMatteNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void DistanceMatteNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *inputSocketImage = this->getInputSocket(0);
	InputSocket *inputSocketKey = this->getInputSocket(1);
	OutputSocket *outputSocketImage = this->getOutputSocket(0);
	OutputSocket *outputSocketMatte = this->getOutputSocket(1);

	NodeOperation *operation;
	bNode *editorsnode = getbNode();
	NodeChroma *storage = (NodeChroma *)editorsnode->storage;

	/* work in RGB color space */
	if (storage->channel == 1) {
		operation = new DistanceRGBMatteOperation();
		((DistanceRGBMatteOperation *) operation)->setSettings(storage);

		inputSocketImage->relinkConnections(operation->getInputSocket(0), 0, graph);
		inputSocketKey->relinkConnections(operation->getInputSocket(1), 1, graph);
	}
	/* work in YCbCr color space */
	else {
		operation = new DistanceYCCMatteOperation();
		((DistanceYCCMatteOperation *) operation)->setSettings(storage);

		ConvertRGBToYCCOperation *operationYCCImage = new ConvertRGBToYCCOperation();
		inputSocketImage->relinkConnections(operationYCCImage->getInputSocket(0), 0, graph);
		addLink(graph, operationYCCImage->getOutputSocket(), operation->getInputSocket(0));
		graph->addOperation(operationYCCImage);

		ConvertRGBToYCCOperation *operationYCCMatte = new ConvertRGBToYCCOperation();
		inputSocketKey->relinkConnections(operationYCCMatte->getInputSocket(0), 1, graph);
		addLink(graph, operationYCCMatte->getOutputSocket(), operation->getInputSocket(1));
		graph->addOperation(operationYCCMatte);
	}

	if (outputSocketMatte->isConnected()) {
		outputSocketMatte->relinkConnections(operation->getOutputSocket());
	}

	graph->addOperation(operation);

	SetAlphaOperation *operationAlpha = new SetAlphaOperation();
	addLink(graph, operation->getInputSocket(0)->getConnection()->getFromSocket(), operationAlpha->getInputSocket(0));
	addLink(graph, operation->getOutputSocket(), operationAlpha->getInputSocket(1));

	graph->addOperation(operationAlpha);
	addPreviewOperation(graph, context, operationAlpha->getOutputSocket());

	if (outputSocketImage->isConnected()) {
		outputSocketImage->relinkConnections(operationAlpha->getOutputSocket());
	}
}
