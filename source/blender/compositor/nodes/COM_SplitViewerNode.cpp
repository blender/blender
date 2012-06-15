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

#include "COM_SplitViewerOperation.h"
#include "COM_ExecutionSystem.h"

SplitViewerNode::SplitViewerNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void SplitViewerNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *image1Socket = this->getInputSocket(0);
	InputSocket *image2Socket = this->getInputSocket(1);
	Image *image = (Image *)this->getbNode()->id;
	ImageUser *imageUser = (ImageUser *) this->getbNode()->storage;
	if (image1Socket->isConnected() && image2Socket->isConnected()) {
		SplitViewerOperation *splitViewerOperation = new SplitViewerOperation();
		splitViewerOperation->setColorManagement(context->getScene()->r.color_mgt_flag & R_COLOR_MANAGEMENT);
		splitViewerOperation->setColorPredivide(context->getScene()->r.color_mgt_flag & R_COLOR_MANAGEMENT_PREDIVIDE);
		splitViewerOperation->setImage(image);
		splitViewerOperation->setImageUser(imageUser);
		splitViewerOperation->setActive((this->getbNode()->flag & NODE_DO_OUTPUT) && this->isInActiveGroup());
		splitViewerOperation->setSplitPercentage(this->getbNode()->custom1);

		/* defaults - the viewer node has these options but not exposed for split view
		 * we could use the split to define an area of interest on one axis at least */
		splitViewerOperation->setChunkOrder(COM_ORDER_OF_CHUNKS_DEFAULT);
		splitViewerOperation->setCenterX(0.5f);
		splitViewerOperation->setCenterY(0.5f);

		splitViewerOperation->setXSplit(!this->getbNode()->custom2);
		image1Socket->relinkConnections(splitViewerOperation->getInputSocket(0), 0, graph);
		image2Socket->relinkConnections(splitViewerOperation->getInputSocket(1), 1, graph);
		addPreviewOperation(graph, splitViewerOperation->getInputSocket(0));
		graph->addOperation(splitViewerOperation);
	}
}
