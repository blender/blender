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

#include "COM_DefocusNode.h"
#include "DNA_scene_types.h"
#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_ConvertDepthToRadiusOperation.h"
#include "COM_VariableSizeBokehBlurOperation.h"
#include "COM_BokehImageOperation.h"
#include "COM_MathBaseOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_GammaCorrectOperation.h"

DefocusNode::DefocusNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void DefocusNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	bNode *node = this->getbNode();
	Scene *scene = (Scene *)node->id;
	Object *camob = (scene) ? scene->camera : NULL;
	NodeDefocus *data = (NodeDefocus *)node->storage;

	NodeOperation *radiusOperation;
	if (data->no_zbuf) {
		MathMultiplyOperation *multiply = new MathMultiplyOperation();
		SetValueOperation *multiplier = new SetValueOperation();
		multiplier->setValue(data->scale);
		SetValueOperation *maxRadius = new SetValueOperation();
		maxRadius->setValue(data->maxblur);
		MathMinimumOperation *minimize = new MathMinimumOperation();
		this->getInputSocket(1)->relinkConnections(multiply->getInputSocket(0), 1, graph);
		addLink(graph, multiplier->getOutputSocket(), multiply->getInputSocket(1));
		addLink(graph, maxRadius->getOutputSocket(), minimize->getInputSocket(1));
		addLink(graph, multiply->getOutputSocket(), minimize->getInputSocket(0));
		
		graph->addOperation(multiply);
		graph->addOperation(multiplier);
		graph->addOperation(maxRadius);
		graph->addOperation(minimize);
		radiusOperation = minimize;
	}
	else {
		ConvertDepthToRadiusOperation *converter = new ConvertDepthToRadiusOperation();
		converter->setCameraObject(camob);
		converter->setfStop(data->fstop);
		converter->setMaxRadius(data->maxblur);
		this->getInputSocket(1)->relinkConnections(converter->getInputSocket(0), 1, graph);
		graph->addOperation(converter);
		radiusOperation = converter;
	}
	
	BokehImageOperation *bokeh = new BokehImageOperation();
	NodeBokehImage *bokehdata = new NodeBokehImage();
	bokehdata->angle = data->rotation;
	bokehdata->rounding = 0.0f;
	bokehdata->flaps = data->bktype;
	if (data->bktype < 3) {
		bokehdata->flaps = 5;
		bokehdata->rounding = 1.0f;
	}
	bokehdata->catadioptric = 0.0f;
	bokehdata->lensshift = 0.0f;
	
	bokeh->setData(bokehdata);
	bokeh->deleteDataOnFinish();
	graph->addOperation(bokeh);
	
	VariableSizeBokehBlurOperation *operation = new VariableSizeBokehBlurOperation();
	if (data->preview) {
		operation->setQuality(COM_QUALITY_LOW);
	} else {
		operation->setQuality(context->getQuality());
	}
	operation->setMaxBlur(data->maxblur);
	operation->setThreshold(data->bthresh);
	addLink(graph, bokeh->getOutputSocket(), operation->getInputSocket(1));
	addLink(graph, radiusOperation->getOutputSocket(), operation->getInputSocket(2));
	addLink(graph, radiusOperation->getInputSocket(0)->getConnection()->getFromSocket(), operation->getInputSocket(3));
	if (data->gamco) {
		GammaCorrectOperation *correct = new GammaCorrectOperation();
		GammaUncorrectOperation *inverse = new GammaUncorrectOperation();
		this->getInputSocket(0)->relinkConnections(correct->getInputSocket(0), 0, graph);
		addLink(graph, correct->getOutputSocket(), operation->getInputSocket(0));
		addLink(graph, operation->getOutputSocket(), inverse->getInputSocket(0));
		this->getOutputSocket()->relinkConnections(inverse->getOutputSocket());
		graph->addOperation(correct);
		graph->addOperation(inverse);
	}
	else {
		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, graph);
		this->getOutputSocket()->relinkConnections(operation->getOutputSocket());
	}
	
	graph->addOperation(operation);
}
