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

void CompositorNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *editorNode = this->getbNode();
	bool is_active = (editorNode->flag & NODE_DO_OUTPUT_RECALC) ||
	                 context.isRendering();
	bool ignore_alpha = editorNode->custom2 & CMP_NODE_OUTPUT_IGNORE_ALPHA;

	NodeInput *imageSocket = this->getInputSocket(0);
	NodeInput *alphaSocket = this->getInputSocket(1);
	NodeInput *depthSocket = this->getInputSocket(2);

	CompositorOperation *compositorOperation = new CompositorOperation();
	compositorOperation->setSceneName(context.getScene()->id.name);
	compositorOperation->setRenderData(context.getRenderData());
	compositorOperation->setbNodeTree(context.getbNodeTree());
	/* alpha socket gives either 1 or a custom alpha value if "use alpha" is enabled */
	compositorOperation->setUseAlphaInput(ignore_alpha || alphaSocket->isLinked());
	compositorOperation->setActive(is_active);
	
	converter.addOperation(compositorOperation);
	converter.mapInputSocket(imageSocket, compositorOperation->getInputSocket(0));
	/* only use alpha link if "use alpha" is enabled */
	if (ignore_alpha)
		converter.addInputValue(compositorOperation->getInputSocket(1), 1.0f);
	else
		converter.mapInputSocket(alphaSocket, compositorOperation->getInputSocket(1));
	converter.mapInputSocket(depthSocket, compositorOperation->getInputSocket(2));
	
	converter.addNodeInputPreview(imageSocket);
}
