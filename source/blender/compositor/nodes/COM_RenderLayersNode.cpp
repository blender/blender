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
#include "COM_RenderLayersProg.h"
#include "COM_TranslateOperation.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"
#include "COM_SetValueOperation.h"

RenderLayersNode::RenderLayersNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void RenderLayersNode::testSocketLink(NodeConverter &converter, const CompositorContext &context,
                                            int outputSocketNumber, RenderLayersBaseProg *operation) const
{
	NodeOutput *outputSocket = this->getOutputSocket(outputSocketNumber);
	Scene *scene = (Scene *)this->getbNode()->id;
	short layerId = this->getbNode()->custom1;

	operation->setScene(scene);
	operation->setLayerId(layerId);
	operation->setRenderData(context.getRenderData());
	
	converter.mapOutputSocket(outputSocket, operation->getOutputSocket());
	converter.addOperation(operation);
	
	if (outputSocketNumber == 0) /* only for image socket */
		converter.addPreview(operation->getOutputSocket());
}

void RenderLayersNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	testSocketLink(converter, context, 0, new RenderLayersColorProg());
	testSocketLink(converter, context, 1, new RenderLayersAlphaProg());
	testSocketLink(converter, context, 2, new RenderLayersDepthProg());
	testSocketLink(converter, context, 3, new RenderLayersNormalOperation());
	testSocketLink(converter, context, 4, new RenderLayersUVOperation());
	testSocketLink(converter, context, 5, new RenderLayersSpeedOperation());
	testSocketLink(converter, context, 6, new RenderLayersColorOperation());
	testSocketLink(converter, context, 7, new RenderLayersDiffuseOperation());
	testSocketLink(converter, context, 8, new RenderLayersSpecularOperation());
	testSocketLink(converter, context, 9, new RenderLayersShadowOperation());
	testSocketLink(converter, context, 10, new RenderLayersAOOperation());
	testSocketLink(converter, context, 11, new RenderLayersReflectionOperation());
	testSocketLink(converter, context, 12, new RenderLayersRefractionOperation());
	testSocketLink(converter, context, 13, new RenderLayersIndirectOperation());
	testSocketLink(converter, context, 14, new RenderLayersObjectIndexOperation());
	testSocketLink(converter, context, 15, new RenderLayersMaterialIndexOperation());
	testSocketLink(converter, context, 16, new RenderLayersMistOperation());
	testSocketLink(converter, context, 17, new RenderLayersEmitOperation());
	testSocketLink(converter, context, 18, new RenderLayersEnvironmentOperation());
	
	// cycles passes
	testSocketLink(converter, context, 19, new RenderLayersCyclesOperation(SCE_PASS_DIFFUSE_DIRECT));
	testSocketLink(converter, context, 20, new RenderLayersCyclesOperation(SCE_PASS_DIFFUSE_INDIRECT));
	testSocketLink(converter, context, 21, new RenderLayersCyclesOperation(SCE_PASS_DIFFUSE_COLOR));
	testSocketLink(converter, context, 22, new RenderLayersCyclesOperation(SCE_PASS_GLOSSY_DIRECT));
	testSocketLink(converter, context, 23, new RenderLayersCyclesOperation(SCE_PASS_GLOSSY_INDIRECT));
	testSocketLink(converter, context, 24, new RenderLayersCyclesOperation(SCE_PASS_GLOSSY_COLOR));
	testSocketLink(converter, context, 25, new RenderLayersCyclesOperation(SCE_PASS_TRANSM_DIRECT));
	testSocketLink(converter, context, 26, new RenderLayersCyclesOperation(SCE_PASS_TRANSM_INDIRECT));
	testSocketLink(converter, context, 27, new RenderLayersCyclesOperation(SCE_PASS_TRANSM_COLOR));
	testSocketLink(converter, context, 28, new RenderLayersCyclesOperation(SCE_PASS_SUBSURFACE_DIRECT));
	testSocketLink(converter, context, 29, new RenderLayersCyclesOperation(SCE_PASS_SUBSURFACE_INDIRECT));
	testSocketLink(converter, context, 30, new RenderLayersCyclesOperation(SCE_PASS_SUBSURFACE_COLOR));
}
