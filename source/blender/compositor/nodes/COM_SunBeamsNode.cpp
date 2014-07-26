/*
 * Copyright 2014, Blender Foundation.
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
 *		Lukas Toenne
 */

#include "COM_SunBeamsNode.h"
#include "COM_SunBeamsOperation.h"

SunBeamsNode::SunBeamsNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void SunBeamsNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	NodeInput *inputSocket = this->getInputSocket(0);
	NodeOutput *outputSocket = this->getOutputSocket(0);
	NodeSunBeams *data = (NodeSunBeams *)getbNode()->storage;

	SunBeamsOperation *operation = new SunBeamsOperation();
	operation->setData(*data);
	converter.addOperation(operation);

	converter.mapInputSocket(inputSocket, operation->getInputSocket(0));
	converter.mapOutputSocket(outputSocket, operation->getOutputSocket(0));
}
