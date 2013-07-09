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

#include "COM_ColorCurveNode.h"
#include "COM_ColorCurveOperation.h"
#include "COM_ExecutionSystem.h"

ColorCurveNode::ColorCurveNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void ColorCurveNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	if (this->getInputSocket(2)->isConnected() || this->getInputSocket(3)->isConnected()) {
		ColorCurveOperation *operation = new ColorCurveOperation();
	
		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, graph);
		this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), 1, graph);
		this->getInputSocket(2)->relinkConnections(operation->getInputSocket(2), 2, graph);
		this->getInputSocket(3)->relinkConnections(operation->getInputSocket(3), 3, graph);
	
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
	
		operation->setCurveMapping((CurveMapping *)this->getbNode()->storage);
	
		graph->addOperation(operation);
	}
	else {
		ConstantLevelColorCurveOperation *operation = new ConstantLevelColorCurveOperation();
	
		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, graph);
		this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), 1, graph);
		float col[4];
		this->getInputSocket(2)->getEditorValueColor(col);
		operation->setBlackLevel(col);
		this->getInputSocket(3)->getEditorValueColor(col);
		operation->setWhiteLevel(col);
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
	
		operation->setCurveMapping((CurveMapping *)this->getbNode()->storage);
	
		graph->addOperation(operation);
	}
}
