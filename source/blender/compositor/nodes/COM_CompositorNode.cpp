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
	bNode *editorNode = this->getbNode();
	bool is_active = (editorNode->flag & NODE_DO_OUTPUT_RECALC) ||
	                 context->isRendering();

	InputSocket *imageSocket = this->getInputSocket(0);
	InputSocket *alphaSocket = this->getInputSocket(1);
	InputSocket *depthSocket = this->getInputSocket(2);

	CompositorOperation *compositorOperation = new CompositorOperation();
	compositorOperation->setSceneName(context->getScene()->id.name);
	compositorOperation->setRenderData(context->getRenderData());
	compositorOperation->setbNodeTree(context->getbNodeTree());
	compositorOperation->setIgnoreAlpha(editorNode->custom2 & CMP_NODE_OUTPUT_IGNORE_ALPHA);
	compositorOperation->setActive(is_active);
	imageSocket->relinkConnections(compositorOperation->getInputSocket(0), 0, graph);
	alphaSocket->relinkConnections(compositorOperation->getInputSocket(1));
	depthSocket->relinkConnections(compositorOperation->getInputSocket(2));
	graph->addOperation(compositorOperation);

	addPreviewOperation(graph, context, compositorOperation->getInputSocket(0));
}
