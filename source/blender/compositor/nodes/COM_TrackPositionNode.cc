/*
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
 * Copyright 2012, Blender Foundation.
 */

#include "COM_TrackPositionNode.h"

#include "COM_ConvertOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_TrackPositionOperation.h"

#include "DNA_movieclip_types.h"

#include "BKE_node.h"

TrackPositionNode::TrackPositionNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

static TrackPositionOperation *create_motion_operation(NodeConverter &converter,
                                                       MovieClip *clip,
                                                       NodeTrackPosData *trackpos_data,
                                                       int axis,
                                                       int frame_number,
                                                       int delta)
{
  TrackPositionOperation *operation = new TrackPositionOperation();
  operation->setMovieClip(clip);
  operation->setTrackingObject(trackpos_data->tracking_object);
  operation->setTrackName(trackpos_data->track_name);
  operation->setFramenumber(frame_number);
  operation->setAxis(axis);
  operation->setPosition(CMP_TRACKPOS_ABSOLUTE);
  operation->setRelativeFrame(frame_number + delta);
  operation->setSpeedOutput(true);
  converter.addOperation(operation);
  return operation;
}

void TrackPositionNode::convertToOperations(NodeConverter &converter,
                                            const CompositorContext &context) const
{
  bNode *editorNode = this->getbNode();
  MovieClip *clip = (MovieClip *)editorNode->id;
  NodeTrackPosData *trackpos_data = (NodeTrackPosData *)editorNode->storage;

  NodeOutput *outputX = this->getOutputSocket(0);
  NodeOutput *outputY = this->getOutputSocket(1);
  NodeOutput *outputSpeed = this->getOutputSocket(2);

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
  converter.mapOutputSocket(outputX, operationX->getOutputSocket());

  TrackPositionOperation *operationY = new TrackPositionOperation();
  operationY->setMovieClip(clip);
  operationY->setTrackingObject(trackpos_data->tracking_object);
  operationY->setTrackName(trackpos_data->track_name);
  operationY->setFramenumber(frame_number);
  operationY->setAxis(1);
  operationY->setPosition(editorNode->custom1);
  operationY->setRelativeFrame(editorNode->custom2);
  converter.addOperation(operationY);
  converter.mapOutputSocket(outputY, operationY->getOutputSocket());

  TrackPositionOperation *operationMotionPreX = create_motion_operation(
      converter, clip, trackpos_data, 0, frame_number, -1);
  TrackPositionOperation *operationMotionPreY = create_motion_operation(
      converter, clip, trackpos_data, 1, frame_number, -1);
  TrackPositionOperation *operationMotionPostX = create_motion_operation(
      converter, clip, trackpos_data, 0, frame_number, 1);
  TrackPositionOperation *operationMotionPostY = create_motion_operation(
      converter, clip, trackpos_data, 1, frame_number, 1);

  CombineChannelsOperation *combine_operation = new CombineChannelsOperation();
  converter.addOperation(combine_operation);
  converter.addLink(operationMotionPreX->getOutputSocket(), combine_operation->getInputSocket(0));
  converter.addLink(operationMotionPreY->getOutputSocket(), combine_operation->getInputSocket(1));
  converter.addLink(operationMotionPostX->getOutputSocket(), combine_operation->getInputSocket(2));
  converter.addLink(operationMotionPostY->getOutputSocket(), combine_operation->getInputSocket(3));
  converter.mapOutputSocket(outputSpeed, combine_operation->getOutputSocket());
}
