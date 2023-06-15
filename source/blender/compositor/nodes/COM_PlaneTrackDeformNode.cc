/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_PlaneTrackDeformNode.h"

#include "COM_PlaneTrackOperation.h"

namespace blender::compositor {

PlaneTrackDeformNode::PlaneTrackDeformNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void PlaneTrackDeformNode::convert_to_operations(NodeConverter &converter,
                                                 const CompositorContext &context) const
{
  const bNode *editor_node = this->get_bnode();
  MovieClip *clip = (MovieClip *)editor_node->id;
  NodePlaneTrackDeformData *data = (NodePlaneTrackDeformData *)editor_node->storage;

  int frame_number = context.get_framenumber();

  NodeInput *input_image = this->get_input_socket(0);
  NodeOutput *output_warped_image = this->get_output_socket(0);
  NodeOutput *output_plane = this->get_output_socket(1);

  PlaneTrackWarpImageOperation *warp_image_operation = new PlaneTrackWarpImageOperation();
  warp_image_operation->set_movie_clip(clip);
  warp_image_operation->set_tracking_object(data->tracking_object);
  warp_image_operation->set_plane_track_name(data->plane_track_name);
  warp_image_operation->set_framenumber(frame_number);
  if (data->flag & CMP_NODE_PLANE_TRACK_DEFORM_FLAG_MOTION_BLUR) {
    warp_image_operation->set_motion_blur_samples(data->motion_blur_samples);
    warp_image_operation->set_motion_blur_shutter(data->motion_blur_shutter);
  }
  converter.add_operation(warp_image_operation);

  converter.map_input_socket(input_image, warp_image_operation->get_input_socket(0));
  converter.map_output_socket(output_warped_image, warp_image_operation->get_output_socket());

  PlaneTrackMaskOperation *plane_mask_operation = new PlaneTrackMaskOperation();
  plane_mask_operation->set_movie_clip(clip);
  plane_mask_operation->set_tracking_object(data->tracking_object);
  plane_mask_operation->set_plane_track_name(data->plane_track_name);
  plane_mask_operation->set_framenumber(frame_number);
  if (data->flag & CMP_NODE_PLANE_TRACK_DEFORM_FLAG_MOTION_BLUR) {
    plane_mask_operation->set_motion_blur_samples(data->motion_blur_samples);
    plane_mask_operation->set_motion_blur_shutter(data->motion_blur_shutter);
  }
  converter.add_operation(plane_mask_operation);

  converter.map_output_socket(output_plane, plane_mask_operation->get_output_socket());
}

}  // namespace blender::compositor
