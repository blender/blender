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

#include "COM_CompositorNode.h"
#include "COM_CompositorOperation.h"
#include "COM_ExecutionSystem.h"

CompositorNode::CompositorNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void CompositorNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *imageSocket = this->getInputSocket(0);
	InputSocket *alphaSocket = this->getInputSocket(1);
	if (imageSocket->isConnected()) {
		CompositorOperation *colourAlphaProg = new CompositorOperation();
		colourAlphaProg->setScene(context->getScene());
		colourAlphaProg->setbNodeTree(context->getbNodeTree());
		imageSocket->relinkConnections(colourAlphaProg->getInputSocket(0));
		alphaSocket->relinkConnections(colourAlphaProg->getInputSocket(1));
		graph->addOperation(colourAlphaProg);
		addPreviewOperation(graph, colourAlphaProg->getInputSocket(0));
	}
}
