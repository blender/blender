/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_LensDistortionNode.h"
#include "COM_ProjectorLensDistortionOperation.h"
#include "COM_ScreenLensDistortionOperation.h"

namespace blender::compositor {

LensDistortionNode::LensDistortionNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void LensDistortionNode::convert_to_operations(NodeConverter &converter,
                                               const CompositorContext & /*context*/) const
{
  const bNode *editor_node = this->get_bnode();
  NodeLensDist *data = (NodeLensDist *)editor_node->storage;
  if (data->proj) {
    ProjectorLensDistortionOperation *operation = new ProjectorLensDistortionOperation();
    converter.add_operation(operation);

    converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
    converter.map_input_socket(get_input_socket(2), operation->get_input_socket(1));
    converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
  }
  else {
    ScreenLensDistortionOperation *operation = new ScreenLensDistortionOperation();
    operation->set_fit(data->fit);
    operation->set_jitter(data->jit);

    if (!get_input_socket(1)->is_linked()) {
      operation->set_distortion(get_input_socket(1)->get_editor_value_float());
    }
    if (!get_input_socket(2)->is_linked()) {
      operation->set_dispersion(get_input_socket(2)->get_editor_value_float());
    }

    converter.add_operation(operation);

    converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
    converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
    converter.map_input_socket(get_input_socket(2), operation->get_input_socket(2));
    converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
  }
}

}  // namespace blender::compositor
