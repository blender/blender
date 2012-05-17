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

#include "COM_BokehImageNode.h"
#include "DNA_scene_types.h"
#include "COM_BokehImageOperation.h"
#include "COM_ExecutionSystem.h"

BokehImageNode::BokehImageNode(bNode *editorNode): Node(editorNode)
{
}

void BokehImageNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context)
{
	BokehImageOperation *operation = new BokehImageOperation();
	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
	graph->addOperation(operation);
	operation->setData((NodeBokehImage*)this->getbNode()->storage);
	addPreviewOperation(graph, operation->getOutputSocket(0), 9);
}
