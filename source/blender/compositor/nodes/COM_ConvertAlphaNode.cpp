/*
 * Copyright 2012, Blender Foundation.
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
 *		Dalai Felinto
 */

#include "COM_ConvertAlphaNode.h"
#include "COM_ConvertPremulToKeyOperation.h"
#include "COM_ConvertKeyToPremulOperation.h"
#include "COM_ExecutionSystem.h"

void ConvertAlphaNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	NodeOperation* operation = NULL;
	bNode* node = this->getbNode();

	/* value hardcoded in rna_nodetree.c */
	if (node->custom1 == 1){
		operation = new ConvertPremulToKeyOperation();
	}
	else {
		operation = new ConvertKeyToPremulOperation();
	} 

	this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());

	graph->addOperation(operation);
}
