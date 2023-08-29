/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_TrackPositionNode.h"

#include "COM_ConvertOperation.h"
#include "COM_TrackPositionOperation.h"

#include "DNA_movieclip_types.h"

#include "BKE_node.hh"

namespace blender::compositor {

TrackPositionNode::TrackPositionNode(bNode *editor_node) : Node(editor_node)
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
  operation->set_movie_clip(clip);
  operation->set_tracking_object(trackpos_data->tracking_object);
  operation->set_track_name(trackpos_data->track_name);
  operation->set_framenumber(frame_number);
  operation->set_axis(axis);
  operation->set_position(CMP_NODE_TRACK_POSITION_ABSOLUTE);
  operation->set_relative_frame(frame_number + delta);
  operation->set_speed_output(true);
  converter.add_operation(operation);
  return operation;
}

void TrackPositionNode::convert_to_operations(NodeConverter &converter,
                                              const CompositorContext &context) const
{
  const bNode *editor_node = this->get_bnode();
  MovieClip *clip = (MovieClip *)editor_node->id;
  NodeTrackPosData *trackpos_data = (NodeTrackPosData *)editor_node->storage;

  NodeOutput *outputX = this->get_output_socket(0);
  NodeOutput *outputY = this->get_output_socket(1);
  NodeOutput *output_speed = this->get_output_socket(2);

  int frame_number;
  if (editor_node->custom1 == CMP_NODE_TRACK_POSITION_ABSOLUTE_FRAME) {
    frame_number = editor_node->custom2;
  }
  else {
    frame_number = context.get_framenumber();
  }

  TrackPositionOperation *operationX = new TrackPositionOperation();
  operationX->set_movie_clip(clip);
  operationX->set_tracking_object(trackpos_data->tracking_object);
  operationX->set_track_name(trackpos_data->track_name);
  operationX->set_framenumber(frame_number);
  operationX->set_axis(0);
  operationX->set_position(static_cast<CMPNodeTrackPositionMode>(editor_node->custom1));
  operationX->set_relative_frame(editor_node->custom2);
  converter.add_operation(operationX);
  converter.map_output_socket(outputX, operationX->get_output_socket());

  TrackPositionOperation *operationY = new TrackPositionOperation();
  operationY->set_movie_clip(clip);
  operationY->set_tracking_object(trackpos_data->tracking_object);
  operationY->set_track_name(trackpos_data->track_name);
  operationY->set_framenumber(frame_number);
  operationY->set_axis(1);
  operationY->set_position(static_cast<CMPNodeTrackPositionMode>(editor_node->custom1));
  operationY->set_relative_frame(editor_node->custom2);
  converter.add_operation(operationY);
  converter.map_output_socket(outputY, operationY->get_output_socket());

  TrackPositionOperation *operationMotionPreX = create_motion_operation(
      converter, clip, trackpos_data, 0, frame_number, -1);
  TrackPositionOperation *operationMotionPreY = create_motion_operation(
      converter, clip, trackpos_data, 1, frame_number, -1);
  TrackPositionOperation *operationMotionPostX = create_motion_operation(
      converter, clip, trackpos_data, 0, frame_number, 1);
  TrackPositionOperation *operationMotionPostY = create_motion_operation(
      converter, clip, trackpos_data, 1, frame_number, 1);

  CombineChannelsOperation *combine_operation = new CombineChannelsOperation();
  converter.add_operation(combine_operation);
  converter.add_link(operationMotionPreX->get_output_socket(),
                     combine_operation->get_input_socket(0));
  converter.add_link(operationMotionPreY->get_output_socket(),
                     combine_operation->get_input_socket(1));
  converter.add_link(operationMotionPostX->get_output_socket(),
                     combine_operation->get_input_socket(2));
  converter.add_link(operationMotionPostY->get_output_socket(),
                     combine_operation->get_input_socket(3));
  converter.map_output_socket(output_speed, combine_operation->get_output_socket());
}

}  // namespace blender::compositor
