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

#include "COM_RenderLayersNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_RenderLayersProg.h"
#include "COM_TranslateOperation.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"
#include "COM_SetValueOperation.h"

RenderLayersNode::RenderLayersNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void RenderLayersNode::testSocketConnection(ExecutionSystem *system, CompositorContext *context, int outputSocketNumber, RenderLayersBaseProg *operation)
{
	OutputSocket *outputSocket = this->getOutputSocket(outputSocketNumber);
	Scene *scene = (Scene *)this->getbNode()->id;
	short layerId = this->getbNode()->custom1;

	if (outputSocket->isConnected()) {
		operation->setScene(scene);
		operation->setLayerId(layerId);
		operation->setRenderData(context->getRenderData());
		outputSocket->relinkConnections(operation->getOutputSocket());
		system->addOperation(operation);
		if (outputSocketNumber == 0) { // only do for image socket if connected
			addPreviewOperation(system, context, operation->getOutputSocket());
		}
	}
	else {
		if (outputSocketNumber == 0) {
			system->addOperation(operation);
			operation->setScene(scene);
			operation->setLayerId(layerId);
			operation->setRenderData(context->getRenderData());
			addPreviewOperation(system, context, operation->getOutputSocket());
		}
		else {
			delete operation;
		}
	}
}

void RenderLayersNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	testSocketConnection(graph, context, 0, new RenderLayersColorProg());
	testSocketConnection(graph, context, 1, new RenderLayersAlphaProg());
	testSocketConnection(graph, context, 2, new RenderLayersDepthProg());
	testSocketConnection(graph, context, 3, new RenderLayersNormalOperation());
	testSocketConnection(graph, context, 4, new RenderLayersUVOperation());
	testSocketConnection(graph, context, 5, new RenderLayersSpeedOperation());
	testSocketConnection(graph, context, 6, new RenderLayersColorOperation());
	testSocketConnection(graph, context, 7, new RenderLayersDiffuseOperation());
	testSocketConnection(graph, context, 8, new RenderLayersSpecularOperation());
	testSocketConnection(graph, context, 9, new RenderLayersShadowOperation());
	testSocketConnection(graph, context, 10, new RenderLayersAOOperation());
	testSocketConnection(graph, context, 11, new RenderLayersReflectionOperation());
	testSocketConnection(graph, context, 12, new RenderLayersRefractionOperation());
	testSocketConnection(graph, context, 13, new RenderLayersIndirectOperation());
	testSocketConnection(graph, context, 14, new RenderLayersObjectIndexOperation());
	testSocketConnection(graph, context, 15, new RenderLayersMaterialIndexOperation());
	testSocketConnection(graph, context, 16, new RenderLayersMistOperation());
	testSocketConnection(graph, context, 17, new RenderLayersEmitOperation());
	testSocketConnection(graph, context, 18, new RenderLayersEnvironmentOperation());
	
	// cycles passes
	testSocketConnection(graph, context, 19, new RenderLayersCyclesOperation(SCE_PASS_DIFFUSE_DIRECT));
	testSocketConnection(graph, context, 20, new RenderLayersCyclesOperation(SCE_PASS_DIFFUSE_INDIRECT));
	testSocketConnection(graph, context, 21, new RenderLayersCyclesOperation(SCE_PASS_DIFFUSE_COLOR));
	testSocketConnection(graph, context, 22, new RenderLayersCyclesOperation(SCE_PASS_GLOSSY_DIRECT));
	testSocketConnection(graph, context, 23, new RenderLayersCyclesOperation(SCE_PASS_GLOSSY_INDIRECT));
	testSocketConnection(graph, context, 24, new RenderLayersCyclesOperation(SCE_PASS_GLOSSY_COLOR));
	testSocketConnection(graph, context, 25, new RenderLayersCyclesOperation(SCE_PASS_TRANSM_DIRECT));
	testSocketConnection(graph, context, 26, new RenderLayersCyclesOperation(SCE_PASS_TRANSM_INDIRECT));
	testSocketConnection(graph, context, 27, new RenderLayersCyclesOperation(SCE_PASS_TRANSM_COLOR));
	testSocketConnection(graph, context, 28, new RenderLayersCyclesOperation(SCE_PASS_SUBSURFACE_DIRECT));
	testSocketConnection(graph, context, 29, new RenderLayersCyclesOperation(SCE_PASS_SUBSURFACE_INDIRECT));
	testSocketConnection(graph, context, 30, new RenderLayersCyclesOperation(SCE_PASS_SUBSURFACE_COLOR));
}
