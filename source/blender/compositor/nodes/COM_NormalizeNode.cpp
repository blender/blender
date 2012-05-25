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

#include "COM_NormalizeNode.h"
#include "COM_NormalizeOperation.h"
#include "COM_ExecutionSystem.h"

NormalizeNode::NormalizeNode(bNode *editorNode): Node(editorNode)
{
}

void NormalizeNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context)
{
	NormalizeOperation *operation = new NormalizeOperation();

	this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, graph);
	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));

	graph->addOperation(operation);
}
