/*
 * Copyright 2017, Blender Foundation.
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
 * Contributor: IRIE Shinsuke
 */

#include "COM_AntiAliasingNode.h"
#include "COM_SMAAOperation.h"

namespace blender::compositor {

void AntiAliasingNode::convert_to_operations(NodeConverter &converter,
                                             const CompositorContext & /*context*/) const
{
  bNode *node = this->get_bnode();
  NodeAntiAliasingData *data = (NodeAntiAliasingData *)node->storage;

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
