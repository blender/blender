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
#include "COM_RenderLayersImageProg.h"
#include "COM_RenderLayersAlphaProg.h"
#include "COM_RenderLayersDepthProg.h"
#include "COM_RenderLayersNormalOperation.h"
#include "COM_RenderLayersSpeedOperation.h"
#include "COM_RenderLayersColorOperation.h"
#include "COM_RenderLayersUVOperation.h"
#include "COM_RenderLayersMistOperation.h"
#include "COM_RenderLayersObjectIndexOperation.h"
#include "COM_RenderLayersDiffuseOperation.h"
#include "COM_RenderLayersSpecularOperation.h"
#include "COM_RenderLayersShadowOperation.h"
#include "COM_RenderLayersAOOperation.h"
#include "COM_RenderLayersEmitOperation.h"
#include "COM_RenderLayersReflectionOperation.h"
#include "COM_RenderLayersRefractionOperation.h"
#include "COM_RenderLayersEnvironmentOperation.h"
#include "COM_RenderLayersIndirectOperation.h"
#include "COM_RenderLayersMaterialIndexOperation.h"
#include "COM_RenderLayersCyclesOperation.h"
#include "COM_TranslateOperation.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"
#include "COM_SetValueOperation.h"

RenderLayersNode::RenderLayersNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void RenderLayersNode::testSocketConnection(ExecutionSystem *system, int outputSocketNumber, RenderLayersBaseProg *operation)
{
	OutputSocket *outputSocket = this->getOutputSocket(outputSocketNumber);
	Scene *scene = (Scene *)this->getbNode()->id;
	short layerId = this->getbNode()->custom1;

	if (outputSocket->isConnected()) {
		operation->setScene(scene);
		operation->setLayerId(layerId);
		outputSocket->relinkConnections(operation->getOutputSocket());
		system->addOperation(operation);
		if (outputSocketNumber == 0) { // only do for image socket if connected
			addPreviewOperation(system, operation->getOutputSocket());
		}
	}
	else {
		if (outputSocketNumber == 0) {
			system->addOperation(operation);
			operation->setScene(scene);
			operation->setLayerId(layerId);
			addPreviewOperation(system, operation->getOutputSocket());
		}
		else {
			delete operation;
		}
	}
}

void RenderLayersNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	testSocketConnection(graph, 0, new RenderLayersColorProg());
	testSocketConnection(graph, 1, new RenderLayersAlphaProg());
	testSocketConnection(graph, 2, new RenderLayersDepthProg());
	testSocketConnection(graph, 3, new RenderLayersNormalOperation());
	testSocketConnection(graph, 4, new RenderLayersUVOperation());
	testSocketConnection(graph, 5, new RenderLayersSpeedOperation());
	testSocketConnection(graph, 6, new RenderLayersColorOperation());
	testSocketConnection(graph, 7, new RenderLayersDiffuseOperation());
	testSocketConnection(graph, 8, new RenderLayersSpecularOperation());
	testSocketConnection(graph, 9, new RenderLayersShadowOperation());
	testSocketConnection(graph, 10, new RenderLayersAOOperation());
	testSocketConnection(graph, 11, new RenderLayersReflectionOperation());
	testSocketConnection(graph, 12, new RenderLayersRefractionOperation());
	testSocketConnection(graph, 13, new RenderLayersIndirectOperation());
	testSocketConnection(graph, 14, new RenderLayersObjectIndexOperation());
	testSocketConnection(graph, 15, new RenderLayersMaterialIndexOperation());
	testSocketConnection(graph, 16, new RenderLayersMistOperation());
	testSocketConnection(graph, 17, new RenderLayersEmitOperation());
	testSocketConnection(graph, 18, new RenderLayersEnvironmentOperation());
	
	// cycles passes
	testSocketConnection(graph, 19, new RenderLayersCyclesOperation(SCE_PASS_DIFFUSE_DIRECT));
	testSocketConnection(graph, 20, new RenderLayersCyclesOperation(SCE_PASS_DIFFUSE_INDIRECT));
	testSocketConnection(graph, 21, new RenderLayersCyclesOperation(SCE_PASS_DIFFUSE_COLOR));
	testSocketConnection(graph, 22, new RenderLayersCyclesOperation(SCE_PASS_GLOSSY_DIRECT));
	testSocketConnection(graph, 23, new RenderLayersCyclesOperation(SCE_PASS_GLOSSY_INDIRECT));
	testSocketConnection(graph, 24, new RenderLayersCyclesOperation(SCE_PASS_GLOSSY_COLOR));
	testSocketConnection(graph, 25, new RenderLayersCyclesOperation(SCE_PASS_TRANSM_DIRECT));
	testSocketConnection(graph, 26, new RenderLayersCyclesOperation(SCE_PASS_TRANSM_INDIRECT));
	testSocketConnection(graph, 27, new RenderLayersCyclesOperation(SCE_PASS_TRANSM_COLOR));
}
