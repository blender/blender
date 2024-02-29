/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_CornerPinNode.h"

#include "COM_PlaneCornerPinOperation.h"
#include "COM_SMAAOperation.h"

namespace blender::compositor {

CornerPinNode::CornerPinNode(bNode *editor_node) : Node(editor_node) {}

void CornerPinNode::convert_to_operations(NodeConverter &converter,
                                          const CompositorContext & /*context*/) const
{
  PlaneCornerPinMaskOperation *plane_mask_operation = new PlaneCornerPinMaskOperation();
  converter.add_operation(plane_mask_operation);

  SMAAEdgeDetectionOperation *smaa_edge_detection = new SMAAEdgeDetectionOperation();
  converter.add_operation(smaa_edge_detection);

  converter.add_link(plane_mask_operation->get_output_socket(),
                     smaa_edge_detection->get_input_socket(0));

  SMAABlendingWeightCalculationOperation *smaa_blending_weights =
      new SMAABlendingWeightCalculationOperation();
  converter.add_operation(smaa_blending_weights);

  converter.add_link(smaa_edge_detection->get_output_socket(),
                     smaa_blending_weights->get_input_socket(0));

  SMAANeighborhoodBlendingOperation *smaa_neighborhood = new SMAANeighborhoodBlendingOperation();
  converter.add_operation(smaa_neighborhood);

  converter.add_link(plane_mask_operation->get_output_socket(),
                     smaa_neighborhood->get_input_socket(0));
  converter.add_link(smaa_blending_weights->get_output_socket(),
                     smaa_neighborhood->get_input_socket(1));

  converter.map_output_socket(this->get_output_socket(1), smaa_neighborhood->get_output_socket());

  PlaneCornerPinWarpImageOperation *warp_image_operation = new PlaneCornerPinWarpImageOperation();
  converter.add_operation(warp_image_operation);
  converter.map_input_socket(this->get_input_socket(0), warp_image_operation->get_input_socket(0));

  /* NOTE: socket order differs between UI node and operations:
   * bNode uses intuitive order following top-down layout:
   *   upper-left, upper-right, lower-left, lower-right
   * Operations use same order as the tracking blenkernel functions expect:
   *   lower-left, lower-right, upper-right, upper-left
   */
  const int node_corner_index[4] = {3, 4, 2, 1};
  for (int i = 0; i < 4; i++) {
    NodeInput *corner_input = get_input_socket(node_corner_index[i]);
    converter.map_input_socket(corner_input, warp_image_operation->get_input_socket(i + 1));
    converter.map_input_socket(corner_input, plane_mask_operation->get_input_socket(i));
  }

  SetAlphaMultiplyOperation *set_alpha_operation = new SetAlphaMultiplyOperation();
  converter.add_operation(set_alpha_operation);
  converter.add_link(warp_image_operation->get_output_socket(),
                     set_alpha_operation->get_input_socket(0));
  converter.add_link(smaa_neighborhood->get_output_socket(),
                     set_alpha_operation->get_input_socket(1));
  converter.map_output_socket(this->get_output_socket(0),
                              set_alpha_operation->get_output_socket());
}

}  // namespace blender::compositor
