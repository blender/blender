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

NormalizeNode::NormalizeNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void NormalizeNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	NormalizeOperation *operation = new NormalizeOperation();
	converter.addOperation(operation);

	converter.mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
	converter.mapOutputSocket(getOutputSocket(0), operation->getOutputSocket(0));
}
