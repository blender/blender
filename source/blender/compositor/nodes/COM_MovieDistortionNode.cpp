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

#include "COM_MovieDistortionNode.h"

#include "COM_MovieDistortionOperation.h"
#include "COM_ExecutionSystem.h"
#include "DNA_movieclip_types.h"

MovieDistortionNode::MovieDistortionNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void MovieDistortionNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *bnode = this->getbNode();
	MovieClip *clip = (MovieClip *)bnode->id;
	
	NodeInput *inputSocket = this->getInputSocket(0);
	NodeOutput *outputSocket = this->getOutputSocket(0);
	
	MovieDistortionOperation *operation = new MovieDistortionOperation(bnode->custom1 == 1);
	operation->setMovieClip(clip);
	operation->setFramenumber(context.getFramenumber());
	converter.addOperation(operation);

	converter.mapInputSocket(inputSocket, operation->getInputSocket(0));
	converter.mapOutputSocket(outputSocket, operation->getOutputSocket(0));
}
