/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorCorrectionNode.h"
#include "COM_ColorCorrectionOperation.h"

namespace blender::compositor {

ColorCorrectionNode::ColorCorrectionNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void ColorCorrectionNode::convert_to_operations(NodeConverter &converter,
                                                const CompositorContext & /*context*/) const
{
  const bNode *editor_node = get_bnode();

  ColorCorrectionOperation *operation = new ColorCorrectionOperation();
  operation->set_data((NodeColorCorrection *)editor_node->storage);
  operation->set_red_channel_enabled((editor_node->custom1 & 1) != 0);
  operation->set_green_channel_enabled((editor_node->custom1 & 2) != 0);
  operation->set_blue_channel_enabled((editor_node->custom1 & 4) != 0);
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
}

}  // namespace blender::compositor
