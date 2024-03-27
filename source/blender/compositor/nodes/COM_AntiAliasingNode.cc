/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_AntiAliasingNode.h"
#include "COM_SMAAOperation.h"

namespace blender::compositor {

/* Blender encodes the threshold in the [0, 1] range, while the SMAA algorithm expects it in
 * the [0, 0.5] range. */
static float get_threshold(const NodeAntiAliasingData *data)
{
  return data->threshold / 2.0f;
}

/* Blender encodes the local contrast adaptation factor in the [0, 1] range, while the SMAA
 * algorithm expects it in the [0, 10] range. */
static float get_local_contrast_adaptation_factor(const NodeAntiAliasingData *data)
{
  return data->contrast_limit * 10.0f;
}

/* Blender encodes the corner rounding factor in the float [0, 1] range, while the SMAA algorithm
 * expects it in the integer [0, 100] range. */
static int get_corner_rounding(const NodeAntiAliasingData *data)
{
  return int(data->corner_rounding * 100.0f);
}

void AntiAliasingNode::convert_to_operations(NodeConverter &converter,
                                             const CompositorContext & /*context*/) const
{
  const bNode *node = this->get_bnode();
  const NodeAntiAliasingData *data = (const NodeAntiAliasingData *)node->storage;

  SMAAOperation *operation = new SMAAOperation();
  operation->set_threshold(get_threshold(data));
  operation->set_local_contrast_adaptation_factor(get_local_contrast_adaptation_factor(data));
  operation->set_corner_rounding(get_corner_rounding(data));
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket());
}

}  // namespace blender::compositor
