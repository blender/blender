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

#include "COM_TextureNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_TextureOperation.h"

TextureNode::TextureNode(bNode *editorNode): Node(editorNode)
{
}

void TextureNode::convertToOperations(ExecutionSystem *system, CompositorContext * context)
{
	bNode *editorNode = this->getbNode();
	Tex *texture = (Tex*)editorNode->id;
	TextureOperation *operation = new TextureOperation();
	this->getOutputSocket(1)->relinkConnections(operation->getOutputSocket());
	this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, system);
	this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), true, 1, system);
	operation->setTexture(texture);
	operation->setScene(context->getScene());
	system->addOperation(operation);
	addPreviewOperation(system, operation->getOutputSocket(), 9);

	if (this->getOutputSocket(0)->isConnected()) {
		TextureAlphaOperation *alphaOperation = new TextureAlphaOperation();
		this->getOutputSocket(0)->relinkConnections(alphaOperation->getOutputSocket());
		addLink(system, operation->getInputSocket(0)->getConnection()->getFromSocket(), alphaOperation->getInputSocket(0));
		addLink(system, operation->getInputSocket(1)->getConnection()->getFromSocket(), alphaOperation->getInputSocket(1));
		alphaOperation->setTexture(texture);
		alphaOperation->setScene(context->getScene());
		system->addOperation(alphaOperation);
	}
}
