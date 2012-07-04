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
		CompositorOperation *colorAlphaProg = new CompositorOperation();
		colorAlphaProg->setRenderData(context->getRenderData());
		colorAlphaProg->setbNodeTree(context->getbNodeTree());
		imageSocket->relinkConnections(colorAlphaProg->getInputSocket(0));
		alphaSocket->relinkConnections(colorAlphaProg->getInputSocket(1));
		graph->addOperation(colorAlphaProg);
		addPreviewOperation(graph, colorAlphaProg->getInputSocket(0));
	}
}
