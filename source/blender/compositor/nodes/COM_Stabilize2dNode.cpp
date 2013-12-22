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

#include "COM_Stabilize2dNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_TranslateOperation.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"
#include "COM_MovieClipAttributeOperation.h"
#include "COM_SetSamplerOperation.h"

extern "C" {
#  include "DNA_movieclip_types.h"
#  include "BKE_tracking.h"
}

Stabilize2dNode::Stabilize2dNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void Stabilize2dNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *imageInput = this->getInputSocket(0);
	MovieClip *clip = (MovieClip *)getbNode()->id;
	
	ScaleOperation *scaleOperation = new ScaleOperation();
	RotateOperation *rotateOperation = new RotateOperation();
	TranslateOperation *translateOperation = new TranslateOperation();
	MovieClipAttributeOperation *scaleAttribute = new MovieClipAttributeOperation();
	MovieClipAttributeOperation *angleAttribute = new MovieClipAttributeOperation();
	MovieClipAttributeOperation *xAttribute = new MovieClipAttributeOperation();
	MovieClipAttributeOperation *yAttribute = new MovieClipAttributeOperation();
	SetSamplerOperation *psoperation = new SetSamplerOperation();

	scaleAttribute->setAttribute(MCA_SCALE);
	scaleAttribute->setFramenumber(context->getFramenumber());
	scaleAttribute->setMovieClip(clip);

	angleAttribute->setAttribute(MCA_ANGLE);
	angleAttribute->setFramenumber(context->getFramenumber());
	angleAttribute->setMovieClip(clip);

	xAttribute->setAttribute(MCA_X);
	xAttribute->setFramenumber(context->getFramenumber());
	xAttribute->setMovieClip(clip);
	
	yAttribute->setAttribute(MCA_Y);
	yAttribute->setFramenumber(context->getFramenumber());
	yAttribute->setMovieClip(clip);
	
	imageInput->relinkConnections(scaleOperation->getInputSocket(0), 0, graph);
	addLink(graph, scaleAttribute->getOutputSocket(), scaleOperation->getInputSocket(1));
	addLink(graph, scaleAttribute->getOutputSocket(), scaleOperation->getInputSocket(2));
	
	scaleOperation->setSampler((PixelSampler)this->getbNode()->custom1);
	
	addLink(graph, scaleOperation->getOutputSocket(), rotateOperation->getInputSocket(0));
	addLink(graph, angleAttribute->getOutputSocket(), rotateOperation->getInputSocket(1));
	rotateOperation->setDoDegree2RadConversion(false);

	addLink(graph, rotateOperation->getOutputSocket(), translateOperation->getInputSocket(0));
	addLink(graph, xAttribute->getOutputSocket(), translateOperation->getInputSocket(1));
	addLink(graph, yAttribute->getOutputSocket(), translateOperation->getInputSocket(2));
	
	psoperation->setSampler((PixelSampler)this->getbNode()->custom1);
	addLink(graph, translateOperation->getOutputSocket(), psoperation->getInputSocket(0));
	this->getOutputSocket()->relinkConnections(psoperation->getOutputSocket());
	
	graph->addOperation(scaleAttribute);
	graph->addOperation(angleAttribute);
	graph->addOperation(xAttribute);
	graph->addOperation(yAttribute);
	graph->addOperation(scaleOperation);
	graph->addOperation(translateOperation);
	graph->addOperation(rotateOperation);
	graph->addOperation(psoperation);
}
