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

ViewerNode::ViewerNode(bNode *editorNode): Node(editorNode)
{
}

void ViewerNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context)\
{
	InputSocket *imageSocket = this->getInputSocket(0);
	InputSocket *alphaSocket = this->getInputSocket(1);
	Image *image = (Image*)this->getbNode()->id;
	ImageUser * imageUser = (ImageUser*) this->getbNode()->storage;
	bNode *editorNode = this->getbNode();
	if (imageSocket->isConnected()) {
		ViewerOperation *viewerOperation = new ViewerOperation();
		viewerOperation->setColorManagement(context->getScene()->r.color_mgt_flag & R_COLOR_MANAGEMENT);
		viewerOperation->setColorPredivide(context->getScene()->r.color_mgt_flag & R_COLOR_MANAGEMENT_PREDIVIDE);
		viewerOperation->setbNodeTree(context->getbNodeTree());
		viewerOperation->setImage(image);
		viewerOperation->setImageUser(imageUser);
		viewerOperation->setActive((editorNode->flag & NODE_DO_OUTPUT) && this->isInActiveGroup());
		viewerOperation->setChunkOrder((OrderOfChunks)editorNode->custom1);
		viewerOperation->setCenterX(editorNode->custom3);
		viewerOperation->setCenterY(editorNode->custom4);
		imageSocket->relinkConnections(viewerOperation->getInputSocket(0), 0, graph);
		alphaSocket->relinkConnections(viewerOperation->getInputSocket(1));
		graph->addOperation(viewerOperation);
		addPreviewOperation(graph, viewerOperation->getInputSocket(0));
	}
}
