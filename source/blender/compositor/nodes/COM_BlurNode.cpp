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

#include "COM_BlurNode.h"
#include "DNA_scene_types.h"
#include "DNA_node_types.h"
#include "COM_GaussianXBlurOperation.h"
#include "COM_GaussianYBlurOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_GaussianBokehBlurOperation.h"
#include "COM_FastGaussianBlurOperation.h"

BlurNode::BlurNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void BlurNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	bNode *editorNode = this->getbNode();
	NodeBlurData *data = (NodeBlurData *)editorNode->storage;
	InputSocket *inputSizeSocket = this->getInputSocket(1);
	bool connectedSizeSocket = inputSizeSocket->isConnected();

	const bNodeSocket *sock = this->getInputSocket(1)->getbNodeSocket();
	const float size = ((const bNodeSocketValueFloat *)sock->default_value)->value;
	
	CompositorQuality quality = context->getQuality();
	
	if (data->filtertype == R_FILTER_FAST_GAUSS) {
		FastGaussianBlurOperation *operationfgb = new FastGaussianBlurOperation();
		operationfgb->setData(data);
		operationfgb->setbNode(editorNode);
		this->getInputSocket(0)->relinkConnections(operationfgb->getInputSocket(0), 0, graph);
		this->getInputSocket(1)->relinkConnections(operationfgb->getInputSocket(1), 1, graph);
		this->getOutputSocket(0)->relinkConnections(operationfgb->getOutputSocket(0));
		graph->addOperation(operationfgb);
		addPreviewOperation(graph, operationfgb->getOutputSocket());
	}
	else if (!data->bokeh) {
		GaussianXBlurOperation *operationx = new GaussianXBlurOperation();
		operationx->setData(data);
		operationx->setbNode(editorNode);
		operationx->setQuality(quality);
		this->getInputSocket(0)->relinkConnections(operationx->getInputSocket(0), 0, graph);
		this->getInputSocket(1)->relinkConnections(operationx->getInputSocket(1), 1, graph);
		graph->addOperation(operationx);
		GaussianYBlurOperation *operationy = new GaussianYBlurOperation();
		operationy->setData(data);
		operationy->setbNode(editorNode);
		operationy->setQuality(quality);
		this->getOutputSocket(0)->relinkConnections(operationy->getOutputSocket());
		graph->addOperation(operationy);
		addLink(graph, operationx->getOutputSocket(), operationy->getInputSocket(0));
		addLink(graph, operationx->getInputSocket(1)->getConnection()->getFromSocket(), operationy->getInputSocket(1));
		addPreviewOperation(graph, operationy->getOutputSocket());

		if (!connectedSizeSocket) {
			operationx->setSize(size);
			operationy->setSize(size);
		}
	}
	else {
		GaussianBokehBlurOperation *operation = new GaussianBokehBlurOperation();
		operation->setData(data);
		operation->setbNode(editorNode);
		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, graph);
		this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), 1, graph);
		operation->setQuality(quality);
		graph->addOperation(operation);
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
		addPreviewOperation(graph, operation->getOutputSocket());

		if (!connectedSizeSocket) {
			operation->setSize(size);
		}
	}
}
