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
 * Contributor: Peter Schlaile
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_InpaintNode.h"
#include "DNA_scene_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_InpaintOperation.h"
#include "BLI_math.h"

InpaintNode::InpaintNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void InpaintNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	
	bNode *editorNode = this->getbNode();

	/* if (editorNode->custom1 == CMP_NODE_INPAINT_SIMPLE) { */
	if (true) {
		InpaintSimpleOperation *operation = new InpaintSimpleOperation();
		operation->setIterations(editorNode->custom2);
		converter.addOperation(operation);
		
		converter.mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
		converter.mapOutputSocket(getOutputSocket(0), operation->getOutputSocket(0));
	}
}
