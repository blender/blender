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

#include "COM_SplitViewerNode.h"
#include "BKE_global.h"

#include "COM_SplitOperation.h"
#include "COM_ViewerOperation.h"
#include "COM_ExecutionSystem.h"

SplitViewerNode::SplitViewerNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void SplitViewerNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *editorNode = this->getbNode();
	bool is_active = ((editorNode->flag & NODE_DO_OUTPUT_RECALC || context.isRendering()) &&
	                  (editorNode->flag & NODE_DO_OUTPUT) && this->isInActiveGroup());

	NodeInput *image1Socket = this->getInputSocket(0);
	NodeInput *image2Socket = this->getInputSocket(1);
	Image *image = (Image *)this->getbNode()->id;
	ImageUser *imageUser = (ImageUser *) this->getbNode()->storage;

	SplitOperation *splitViewerOperation = new SplitOperation();
	splitViewerOperation->setSplitPercentage(this->getbNode()->custom1);
	splitViewerOperation->setXSplit(!this->getbNode()->custom2);

	converter.addOperation(splitViewerOperation);
	converter.mapInputSocket(image1Socket, splitViewerOperation->getInputSocket(0));
	converter.mapInputSocket(image2Socket, splitViewerOperation->getInputSocket(1));

	ViewerOperation *viewerOperation = new ViewerOperation();
	viewerOperation->setImage(image);
	viewerOperation->setImageUser(imageUser);
	viewerOperation->setActive(is_active);
	viewerOperation->setViewSettings(context.getViewSettings());
	viewerOperation->setDisplaySettings(context.getDisplaySettings());

	/* defaults - the viewer node has these options but not exposed for split view
	 * we could use the split to define an area of interest on one axis at least */
	viewerOperation->setChunkOrder(COM_ORDER_OF_CHUNKS_DEFAULT);
	viewerOperation->setCenterX(0.5f);
	viewerOperation->setCenterY(0.5f);

	converter.addOperation(viewerOperation);
	converter.addLink(splitViewerOperation->getOutputSocket(), viewerOperation->getInputSocket(0));

	converter.addPreview(splitViewerOperation->getOutputSocket());
}
