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

#include "COM_TrackPositionNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_TrackPositionOperation.h"

extern "C" {
	#include "DNA_movieclip_types.h"
}

TrackPositionNode::TrackPositionNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void TrackPositionNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	OutputSocket *outputX = this->getOutputSocket(0);
	OutputSocket *outputY = this->getOutputSocket(1);

	bNode *editorNode = this->getbNode();
	MovieClip *clip = (MovieClip *) editorNode->id;

	NodeTrackPosData *trackpos_data = (NodeTrackPosData *) editorNode->storage;

	TrackPositionOperation *operationX = new TrackPositionOperation();
	TrackPositionOperation *operationY = new TrackPositionOperation();

	operationX->setMovieClip(clip);
	operationX->setTrackingObject(trackpos_data->tracking_object);
	operationX->setTrackName(trackpos_data->track_name);
	operationX->setFramenumber(context->getFramenumber());
	operationX->setAxis(0);
	operationX->setRelative(editorNode->custom1);

	operationY->setMovieClip(clip);
	operationY->setTrackingObject(trackpos_data->tracking_object);
	operationY->setTrackName(trackpos_data->track_name);
	operationY->setFramenumber(context->getFramenumber());
	operationY->setAxis(1);
	operationY->setRelative(editorNode->custom1);

	outputX->relinkConnections(operationX->getOutputSocket());
	outputY->relinkConnections(operationY->getOutputSocket());

	graph->addOperation(operationX);
}
