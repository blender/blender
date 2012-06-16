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

#include "COM_DilateErodeNode.h"
#include "DNA_scene_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_DilateErodeOperation.h"
#include "COM_AntiAliasOperation.h"
#include "COM_GaussianAlphaXBlurOperation.h"
#include "COM_GaussianAlphaYBlurOperation.h"
#include "BLI_math.h"

DilateErodeNode::DilateErodeNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void DilateErodeNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	
	bNode *editorNode = this->getbNode();
	if (editorNode->custom1 == CMP_NODE_DILATEERODE_DISTANCE_THRESH) {
		DilateErodeThresholdOperation *operation = new DilateErodeThresholdOperation();
		operation->setDistance(editorNode->custom2);
		operation->setInset(editorNode->custom3);
		
		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, graph);
	
		if (editorNode->custom3 < 2.0f) {
			AntiAliasOperation *antiAlias = new AntiAliasOperation();
			addLink(graph, operation->getOutputSocket(), antiAlias->getInputSocket(0));
			this->getOutputSocket(0)->relinkConnections(antiAlias->getOutputSocket(0));
			graph->addOperation(antiAlias);
		}
		else {
			this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
		}
		graph->addOperation(operation);
	}
	else if (editorNode->custom1 == CMP_NODE_DILATEERODE_DISTANCE) {
		if (editorNode->custom2 > 0) {
			DilateDistanceOperation *operation = new DilateDistanceOperation();
			operation->setDistance(editorNode->custom2);
			this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, graph);
			this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
			graph->addOperation(operation);
		}
		else {
			ErodeDistanceOperation *operation = new ErodeDistanceOperation();
			operation->setDistance(-editorNode->custom2);
			this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, graph);
			this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
			graph->addOperation(operation);
		}
	}
	else if (editorNode->custom1 == CMP_NODE_DILATEERODE_DISTANCE_FEATHER) {
		/* this uses a modified gaussian blur function otherwise its far too slow */
		if (editorNode->custom2 > 0) {

			CompositorQuality quality = context->getQuality();

			/* initialize node data */
			NodeBlurData *data = (NodeBlurData *)&this->alpha_blur;
			memset(data, 0, sizeof(*data));
			data->sizex = data->sizey = editorNode->custom2;
			data->filtertype = R_FILTER_GAUSS;

			GaussianAlphaXBlurOperation *operationx = new GaussianAlphaXBlurOperation();
			operationx->setData(data);
			operationx->setQuality(quality);
			this->getInputSocket(0)->relinkConnections(operationx->getInputSocket(0), 0, graph);
			// this->getInputSocket(1)->relinkConnections(operationx->getInputSocket(1), 1, graph); // no size input yet
			graph->addOperation(operationx);
			GaussianAlphaYBlurOperation *operationy = new GaussianAlphaYBlurOperation();
			operationy->setData(data);
			operationy->setQuality(quality);
			this->getOutputSocket(0)->relinkConnections(operationy->getOutputSocket());
			graph->addOperation(operationy);
			addLink(graph, operationx->getOutputSocket(), operationy->getInputSocket(0));
			// addLink(graph, operationx->getInputSocket(1)->getConnection()->getFromSocket(), operationy->getInputSocket(1)); // no size input yet
			addPreviewOperation(graph, operationy->getOutputSocket());

			/* TODO? */
			/* see gaussian blue node for original usage */
#if 0
			if (!connectedSizeSocket) {
				operationx->setSize(size);
				operationy->setSize(size);
			}
#else
			operationx->setSize(1.0f);
			operationy->setSize(1.0f);
#endif
		}
		else {
			ErodeDistanceOperation *operation = new ErodeDistanceOperation();
			operation->setDistance(-editorNode->custom2);
			this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, graph);
			this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
			graph->addOperation(operation);
		}
	}
	else {
		if (editorNode->custom2 > 0) {
			DilateStepOperation *operation = new DilateStepOperation();
			operation->setIterations(editorNode->custom2);
			this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, graph);
			this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
			graph->addOperation(operation);
		}
		else {
			ErodeStepOperation *operation = new ErodeStepOperation();
			operation->setIterations(-editorNode->custom2);
			this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, graph);
			this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
			graph->addOperation(operation);
		}
	}
}
