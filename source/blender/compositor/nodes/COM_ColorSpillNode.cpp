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

#include "COM_ColorSpillNode.h"
#include "BKE_node.h"
#include "COM_ColorSpillOperation.h"

ColorSpillNode::ColorSpillNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void ColorSpillNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *editorsnode = getbNode();
	
	NodeInput *inputSocketImage = this->getInputSocket(0);
	NodeInput *inputSocketFac = this->getInputSocket(1);
	NodeOutput *outputSocketImage = this->getOutputSocket(0);
	
	ColorSpillOperation *operation;
	if (editorsnode->custom2 == 0) {
		// Simple color spill
		operation = new ColorSpillOperation();
	}
	else {
		// Average color spill
		operation = new ColorSpillAverageOperation();
	}
	operation->setSettings((NodeColorspill *)editorsnode->storage);
	operation->setSpillChannel(editorsnode->custom1 - 1); // Channel for spilling
	converter.addOperation(operation);
	
	converter.mapInputSocket(inputSocketImage, operation->getInputSocket(0));
	converter.mapInputSocket(inputSocketFac, operation->getInputSocket(1));
	converter.mapOutputSocket(outputSocketImage, operation->getOutputSocket());
}
