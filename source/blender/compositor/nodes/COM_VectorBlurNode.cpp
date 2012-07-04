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

#include "COM_VectorBlurNode.h"
#include "DNA_node_types.h"
#include "COM_VectorBlurOperation.h"

VectorBlurNode::VectorBlurNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void VectorBlurNode::convertToOperations(ExecutionSystem *system, CompositorContext *context)
{
	bNode *node = this->getbNode();
	NodeBlurData *vectorBlurSettings = (NodeBlurData *)node->storage;
	VectorBlurOperation *operation = new VectorBlurOperation();
	operation->setbNode(node);
	operation->setVectorBlurSettings(vectorBlurSettings);
	operation->setQuality(context->getQuality());
	this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, system);
	this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), 1, system);
	this->getInputSocket(2)->relinkConnections(operation->getInputSocket(2), 2, system);
	this->getOutputSocket()->relinkConnections(operation->getOutputSocket());
	system->addOperation(operation);
}
