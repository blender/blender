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

#include "COM_TimeNode.h"
#include "DNA_scene_types.h"
#include "COM_SetValueOperation.h"
#include "COM_ExecutionSystem.h"
extern "C" {
	#include "BKE_colortools.h"
}
#include "BLI_utildefines.h"

TimeNode::TimeNode(bNode *editorNode): Node(editorNode) {
}

void TimeNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	SetValueOperation *operation = new SetValueOperation();
	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
	bNode* node = this->getbNode();

	/* stack order output: fac */
	float fac= 0.0f;
	const int framenumber = context->getFramenumber();

	if (framenumber < node->custom1) {
		fac = 0.0f;
	} else if (framenumber > node->custom2) {
		fac = 1.0f;
	} else 	if(node->custom1 < node->custom2) {
		fac= (context->getFramenumber() - node->custom1)/(float)(node->custom2-node->custom1);
	}

	fac= curvemapping_evaluateF((CurveMapping*)node->storage, 0, fac);
	operation->setValue(CLAMPIS(fac, 0.0f, 1.0f));
	graph->addOperation(operation);
}
