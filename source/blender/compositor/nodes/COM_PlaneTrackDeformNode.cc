/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_PlaneTrackDeformNode.h"

#include "COM_PlaneTrackOperation.h"
#include "COM_SMAAOperation.h"
#include "COM_SetAlphaMultiplyOperation.h"

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

  SMAAOperation *smaa_operation = new SMAAOperation();
  converter.add_operation(smaa_operation);

  converter.add_link(plane_mask_operation->get_output_socket(),
                     smaa_operation->get_input_socket(0));

  converter.map_output_socket(this->get_output_socket(1), smaa_operation->get_output_socket());

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

  converter.map_input_socket(this->get_input_socket(0), warp_image_operation->get_input_socket(0));

  SetAlphaMultiplyOperation *set_alpha_operation = new SetAlphaMultiplyOperation();
  converter.add_operation(set_alpha_operation);
  converter.add_link(warp_image_operation->get_output_socket(),
                     set_alpha_operation->get_input_socket(0));
  converter.add_link(smaa_operation->get_output_socket(),
                     set_alpha_operation->get_input_socket(1));
  converter.map_output_socket(this->get_output_socket(0),
                              set_alpha_operation->get_output_socket());
}

}  // namespace blender::compositor
