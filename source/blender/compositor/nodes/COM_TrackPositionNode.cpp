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
#  include "DNA_movieclip_types.h"

#  include "BKE_node.h"
}

TrackPositionNode::TrackPositionNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void TrackPositionNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *editorNode = this->getbNode();
	MovieClip *clip = (MovieClip *) editorNode->id;
	NodeTrackPosData *trackpos_data = (NodeTrackPosData *) editorNode->storage;
	
	NodeOutput *outputX = this->getOutputSocket(0);
	NodeOutput *outputY = this->getOutputSocket(1);
	NodeOutput *outputSpeedX = this->getOutputSocket(2);
	NodeOutput *outputSpeedY = this->getOutputSocket(3);

	int frame_number;
	if (editorNode->custom1 == CMP_TRACKPOS_ABSOLUTE_FRAME) {
		frame_number = editorNode->custom2;
	}
	else {
		frame_number = context.getFramenumber();
	}

	TrackPositionOperation *operationX = new TrackPositionOperation();
	operationX->setMovieClip(clip);
	operationX->setTrackingObject(trackpos_data->tracking_object);
	operationX->setTrackName(trackpos_data->track_name);
	operationX->setFramenumber(frame_number);
	operationX->setAxis(0);
	operationX->setPosition(editorNode->custom1);
	operationX->setRelativeFrame(editorNode->custom2);
	converter.addOperation(operationX);

	TrackPositionOperation *operationY = new TrackPositionOperation();
	operationY->setMovieClip(clip);
	operationY->setTrackingObject(trackpos_data->tracking_object);
	operationY->setTrackName(trackpos_data->track_name);
	operationY->setFramenumber(frame_number);
	operationY->setAxis(1);
	operationY->setPosition(editorNode->custom1);
	operationY->setRelativeFrame(editorNode->custom2);
	converter.addOperation(operationY);

	TrackPositionOperation *operationSpeedX = new TrackPositionOperation();
	operationSpeedX->setMovieClip(clip);
	operationSpeedX->setTrackingObject(trackpos_data->tracking_object);
	operationSpeedX->setTrackName(trackpos_data->track_name);
	operationSpeedX->setFramenumber(frame_number);
	operationSpeedX->setAxis(0);
	operationSpeedX->setPosition(editorNode->custom1);
	operationSpeedX->setRelativeFrame(editorNode->custom2);
	operationSpeedX->setSpeedOutput(true);
	converter.addOperation(operationSpeedX);

	TrackPositionOperation *operationSpeedY = new TrackPositionOperation();
	operationSpeedY->setMovieClip(clip);
	operationSpeedY->setTrackingObject(trackpos_data->tracking_object);
	operationSpeedY->setTrackName(trackpos_data->track_name);
	operationSpeedY->setFramenumber(frame_number);
	operationSpeedY->setAxis(1);
	operationSpeedY->setPosition(editorNode->custom1);
	operationSpeedY->setRelativeFrame(editorNode->custom2);
	operationSpeedY->setSpeedOutput(true);
	converter.addOperation(operationSpeedY);

	converter.mapOutputSocket(outputX, operationX->getOutputSocket());
	converter.mapOutputSocket(outputY, operationY->getOutputSocket());
	converter.mapOutputSocket(outputSpeedX, operationSpeedX->getOutputSocket());
	converter.mapOutputSocket(outputSpeedY, operationSpeedY->getOutputSocket());
}
