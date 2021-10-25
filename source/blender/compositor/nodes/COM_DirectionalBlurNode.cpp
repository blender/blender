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

#include "COM_DirectionalBlurNode.h"
#include "DNA_node_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_DirectionalBlurOperation.h"

DirectionalBlurNode::DirectionalBlurNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void DirectionalBlurNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	NodeDBlurData *data = (NodeDBlurData *)this->getbNode()->storage;
	DirectionalBlurOperation *operation = new DirectionalBlurOperation();
	operation->setQuality(context.getQuality());
	operation->setData(data);
	converter.addOperation(operation);
	
	converter.mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
	converter.mapOutputSocket(getOutputSocket(0), operation->getOutputSocket());
}
