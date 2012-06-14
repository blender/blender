/*
 * Copyright 2012, Blender Foundation.
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
 *		Sergey Sharybin
 */

#include "COM_KeyingScreenNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_KeyingScreenOperation.h"

extern "C" {
	#include "DNA_movieclip_types.h"
}

KeyingScreenNode::KeyingScreenNode(bNode *editorNode): Node(editorNode)
{
}

void KeyingScreenNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context)
{
	OutputSocket *outputScreen = this->getOutputSocket(0);

	bNode *editorNode = this->getbNode();
	MovieClip *clip = (MovieClip *) editorNode->id;

	NodeKeyingScreenData *keyingscreen_data = (NodeKeyingScreenData *) editorNode->storage;

	// always connect the output image
	KeyingScreenOperation *operation = new KeyingScreenOperation();

	if (outputScreen->isConnected()) {
		outputScreen->relinkConnections(operation->getOutputSocket());
	}

	operation->setMovieClip(clip);
	operation->setTrackingObject(keyingscreen_data->tracking_object);
	operation->setFramenumber(context->getFramenumber());

	graph->addOperation(operation);
}
