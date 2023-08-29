/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_CropNode.h"
#include "COM_CropOperation.h"

namespace blender::compositor {

CropNode::CropNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void CropNode::convert_to_operations(NodeConverter &converter,
                                     const CompositorContext & /*context*/) const
{
  const bNode *node = get_bnode();
  NodeTwoXYs *crop_settings = (NodeTwoXYs *)node->storage;
  bool relative = bool(node->custom2);
  bool crop_image = bool(node->custom1);
  CropBaseOperation *operation;
  if (crop_image) {
    operation = new CropImageOperation();
  }
  else {
    operation = new CropOperation();
  }
  operation->set_crop_settings(crop_settings);
  operation->set_relative(relative);
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_output_socket(get_output_socket(), operation->get_output_socket());
}

}  // namespace blender::compositor
