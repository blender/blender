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

#include "COM_TransformNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_TranslateOperation.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_SetSamplerOperation.h"

TransformNode::TransformNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void TransformNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *imageInput = this->getInputSocket(0);
	InputSocket *xInput = this->getInputSocket(1);
	InputSocket *yInput = this->getInputSocket(2);
	InputSocket *angleInput = this->getInputSocket(3);
	InputSocket *scaleInput = this->getInputSocket(4);
	
	ScaleOperation *scaleOperation = new ScaleOperation();
	RotateOperation *rotateOperation = new RotateOperation();
	TranslateOperation *translateOperation = new TranslateOperation();
	SetSamplerOperation *sampler = new SetSamplerOperation();

	switch (this->getbNode()->custom1) {
		case 0:
			sampler->setSampler(COM_PS_NEAREST);
			break;
		case 1:
			sampler->setSampler(COM_PS_BILINEAR);
			break;
		case 2:
			sampler->setSampler(COM_PS_BICUBIC);
			break;
	}
	
	imageInput->relinkConnections(sampler->getInputSocket(0), 0, graph);
	addLink(graph, sampler->getOutputSocket(), scaleOperation->getInputSocket(0));
	scaleInput->relinkConnections(scaleOperation->getInputSocket(1), 4, graph);
	addLink(graph, scaleOperation->getInputSocket(1)->getConnection()->getFromSocket(), scaleOperation->getInputSocket(2)); // xscale = yscale
	
	addLink(graph, scaleOperation->getOutputSocket(), rotateOperation->getInputSocket(0));
	rotateOperation->setDoDegree2RadConversion(false);
	angleInput->relinkConnections(rotateOperation->getInputSocket(1), 3, graph);

	addLink(graph, rotateOperation->getOutputSocket(), translateOperation->getInputSocket(0));
	xInput->relinkConnections(translateOperation->getInputSocket(1), 1, graph);
	yInput->relinkConnections(translateOperation->getInputSocket(2), 2, graph);
	
	this->getOutputSocket()->relinkConnections(translateOperation->getOutputSocket());
	
	graph->addOperation(sampler);
	graph->addOperation(scaleOperation);
	graph->addOperation(rotateOperation);
	graph->addOperation(translateOperation);
}
