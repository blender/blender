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
 *		Dalai Felinto
 */

#include "COM_DisplaceNode.h"
#include "COM_DisplaceOperation.h"
#include "COM_DisplaceSimpleOperation.h"
#include "COM_ExecutionSystem.h"

DisplaceNode::DisplaceNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void DisplaceNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	NodeOperation *operation;
	if (context.getQuality() == COM_QUALITY_LOW)
		operation = new DisplaceSimpleOperation();
	else
		operation = new DisplaceOperation();
	converter.addOperation(operation);

	converter.mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
	converter.mapInputSocket(getInputSocket(1), operation->getInputSocket(1));
	converter.mapInputSocket(getInputSocket(2), operation->getInputSocket(2));
	converter.mapInputSocket(getInputSocket(3), operation->getInputSocket(3));
	converter.mapOutputSocket(getOutputSocket(0), operation->getOutputSocket());
}
