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

#include "COM_ViewerNode.h"
#include "BKE_global.h"

#include "COM_ViewerOperation.h"
#include "COM_ExecutionSystem.h"

ViewerNode::ViewerNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void ViewerNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *editorNode = this->getbNode();
	bool do_output = (editorNode->flag & NODE_DO_OUTPUT_RECALC || context.isRendering()) && (editorNode->flag & NODE_DO_OUTPUT);
	bool ignore_alpha = (editorNode->custom2 & CMP_NODE_OUTPUT_IGNORE_ALPHA) != 0;

	NodeInput *imageSocket = this->getInputSocket(0);
	NodeInput *alphaSocket = this->getInputSocket(1);
	NodeInput *depthSocket = this->getInputSocket(2);
	Image *image = (Image *)this->getbNode()->id;
	ImageUser *imageUser = (ImageUser *) this->getbNode()->storage;
	ViewerOperation *viewerOperation = new ViewerOperation();
	viewerOperation->setbNodeTree(context.getbNodeTree());
	viewerOperation->setImage(image);
	viewerOperation->setImageUser(imageUser);
	viewerOperation->setChunkOrder((OrderOfChunks)editorNode->custom1);
	viewerOperation->setCenterX(editorNode->custom3);
	viewerOperation->setCenterY(editorNode->custom4);
	/* alpha socket gives either 1 or a custom alpha value if "use alpha" is enabled */
	viewerOperation->setUseAlphaInput(ignore_alpha || alphaSocket->isLinked());

	viewerOperation->setViewSettings(context.getViewSettings());
	viewerOperation->setDisplaySettings(context.getDisplaySettings());

	viewerOperation->setResolutionInputSocketIndex(0);
	if (!imageSocket->isLinked()) {
		if (alphaSocket->isLinked()) {
			viewerOperation->setResolutionInputSocketIndex(1);
		}
	}

	converter.addOperation(viewerOperation);
	converter.mapInputSocket(imageSocket, viewerOperation->getInputSocket(0));
	/* only use alpha link if "use alpha" is enabled */
	if (ignore_alpha)
		converter.addInputValue(viewerOperation->getInputSocket(1), 1.0f);
	else
		converter.mapInputSocket(alphaSocket, viewerOperation->getInputSocket(1));
	converter.mapInputSocket(depthSocket, viewerOperation->getInputSocket(2));

	converter.addNodeInputPreview(imageSocket);

	if (do_output)
		converter.registerViewer(viewerOperation);
}
