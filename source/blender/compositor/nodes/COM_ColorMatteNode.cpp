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

#include "COM_ColorMatteNode.h"
#include "BKE_node.h"
#include "COM_ColorMatteOperation.h"
#include "COM_ConvertOperation.h"
#include "COM_SetAlphaOperation.h"

ColorMatteNode::ColorMatteNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void ColorMatteNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *inputSocketImage = this->getInputSocket(0);
	InputSocket *inputSocketKey = this->getInputSocket(1);
	OutputSocket *outputSocketImage = this->getOutputSocket(0);
	OutputSocket *outputSocketMatte = this->getOutputSocket(1);

	ConvertRGBToHSVOperation *operationRGBToHSV_Image = new ConvertRGBToHSVOperation();
	ConvertRGBToHSVOperation *operationRGBToHSV_Key = new ConvertRGBToHSVOperation();

	ColorMatteOperation *operation = new ColorMatteOperation();
	bNode *editorsnode = getbNode();
	operation->setSettings((NodeChroma *)editorsnode->storage);

	inputSocketImage->relinkConnections(operationRGBToHSV_Image->getInputSocket(0), 0, graph);
	inputSocketKey->relinkConnections(operationRGBToHSV_Key->getInputSocket(0), 1, graph);

	addLink(graph, operationRGBToHSV_Image->getOutputSocket(), operation->getInputSocket(0));
	addLink(graph, operationRGBToHSV_Key->getOutputSocket(), operation->getInputSocket(1));

	if (outputSocketMatte->isConnected()) {
		outputSocketMatte->relinkConnections(operation->getOutputSocket(0));
	}

	graph->addOperation(operationRGBToHSV_Image);
	graph->addOperation(operationRGBToHSV_Key);
	graph->addOperation(operation);

	SetAlphaOperation *operationAlpha = new SetAlphaOperation();
	addLink(graph, operationRGBToHSV_Image->getInputSocket(0)->getConnection()->getFromSocket(), operationAlpha->getInputSocket(0));
	addLink(graph, operation->getOutputSocket(), operationAlpha->getInputSocket(1));
	graph->addOperation(operationAlpha);
	addPreviewOperation(graph, context, operationAlpha->getOutputSocket());

	if (outputSocketImage->isConnected()) {
		outputSocketImage->relinkConnections(operationAlpha->getOutputSocket());
	}
}
