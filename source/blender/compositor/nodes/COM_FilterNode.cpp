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

#include "COM_FilterNode.h"
#include "COM_ConvolutionFilterOperation.h"
#include "COM_ConvolutionEdgeFilterOperation.h"
#include "COM_ExecutionSystem.h"
#include "BKE_node.h"
#include "COM_MixBlendOperation.h"

FilterNode::FilterNode(bNode *editorNode): Node(editorNode)
{
}

void FilterNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context)
{
	InputSocket *inputSocket = this->getInputSocket(0);
	InputSocket *inputImageSocket = this->getInputSocket(1);
	OutputSocket *outputSocket = this->getOutputSocket(0);
	ConvolutionFilterOperation *operation = NULL;
	
	switch (this->getbNode()->custom1) {
	case CMP_FILT_SOFT:
		operation = new ConvolutionFilterOperation();
		operation->set3x3Filter(1/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 4/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 1/16.0f);
		break;
	case CMP_FILT_SHARP:
		operation = new ConvolutionFilterOperation();
		operation->set3x3Filter(-1,-1,-1,-1,9,-1,-1,-1,-1);
		break;
	case CMP_FILT_LAPLACE:
		operation = new ConvolutionFilterOperation();
		operation->set3x3Filter(-1/8.0f, -1/8.0f, -1/8.0f, -1/8.0f, 1.0f, -1/8.0f, -1/8.0f, -1/8.0f, -1/8.0f);
		break;
	case CMP_FILT_SOBEL:
		operation = new ConvolutionEdgeFilterOperation();
		operation->set3x3Filter(1,2,1,0,0,0,-1,-2,-1);
		break;
	case CMP_FILT_PREWITT:
		operation = new ConvolutionEdgeFilterOperation();
		operation->set3x3Filter(1,1,1,0,0,0,-1,-1,-1);
		break;
	case CMP_FILT_KIRSCH:
		operation = new ConvolutionEdgeFilterOperation();
		operation->set3x3Filter(5,5,5,-3,-3,-3,-2,-2,-2);
		break;
	case CMP_FILT_SHADOW:
		operation = new ConvolutionFilterOperation();
		operation->set3x3Filter(1,2,1,0,1,0,-1,-2,-1);
		break;
	default:
		operation = new ConvolutionFilterOperation();
		operation->set3x3Filter(0,0,0,0,1,0,0,0,0);
		break;
	}
	
	inputImageSocket->relinkConnections(operation->getInputSocket(0), 1, graph);
	inputSocket->relinkConnections(operation->getInputSocket(1), 0, graph);
	outputSocket->relinkConnections(operation->getOutputSocket());
	addPreviewOperation(graph, operation->getOutputSocket(0));
	
	graph->addOperation(operation);
}
