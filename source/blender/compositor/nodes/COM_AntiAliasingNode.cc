/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_AntiAliasingNode.h"
#include "COM_SMAAOperation.h"

namespace blender::compositor {

void AntiAliasingNode::convert_to_operations(NodeConverter &converter,
                                             const CompositorContext & /*context*/) const
{
  const bNode *node = this->get_bnode();
  const NodeAntiAliasingData *data = (const NodeAntiAliasingData *)node->storage;

  /* Edge Detection (First Pass) */
  SMAAEdgeDetectionOperation *operation1 = nullptr;

  operation1 = new SMAAEdgeDetectionOperation();
  operation1->set_threshold(data->threshold);
  operation1->set_local_contrast_adaptation_factor(data->contrast_limit);
  converter.add_operation(operation1);

  converter.map_input_socket(get_input_socket(0), operation1->get_input_socket(0));

  /* Blending Weight Calculation Pixel Shader (Second Pass) */
  SMAABlendingWeightCalculationOperation *operation2 =
      new SMAABlendingWeightCalculationOperation();
  operation2->set_corner_rounding(data->corner_rounding);
  converter.add_operation(operation2);

  converter.add_link(operation1->get_output_socket(), operation2->get_input_socket(0));

  /* Neighborhood Blending Pixel Shader (Third Pass) */
  SMAANeighborhoodBlendingOperation *operation3 = new SMAANeighborhoodBlendingOperation();
  converter.add_operation(operation3);

  converter.map_input_socket(get_input_socket(0), operation3->get_input_socket(0));
  converter.add_link(operation2->get_output_socket(), operation3->get_input_socket(1));
  converter.map_output_socket(get_output_socket(0), operation3->get_output_socket());
}

}  // namespace blender::compositor
