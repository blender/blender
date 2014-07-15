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

#include "COM_SwitchNode.h"

SwitchNode::SwitchNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void SwitchNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bool condition = this->getbNode()->custom1;
	
	NodeOperationOutput *result;
	if (!condition)
		result = converter.addInputProxy(getInputSocket(0), false);
	else
		result = converter.addInputProxy(getInputSocket(1), false);
	
	converter.mapOutputSocket(getOutputSocket(0), result);
}
