/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_BokehBlurNode.h"
#include "COM_BokehBlurOperation.h"
#include "COM_VariableSizeBokehBlurOperation.h"

namespace blender::compositor {

BokehBlurNode::BokehBlurNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void BokehBlurNode::convert_to_operations(NodeConverter &converter,
                                          const CompositorContext &context) const
{
  const bNode *b_node = this->get_bnode();

  NodeInput *input_size_socket = this->get_input_socket(2);

  bool connected_size_socket = input_size_socket->is_linked();
  const bool extend_bounds = (b_node->custom1 & CMP_NODEFLAG_BLUR_EXTEND_BOUNDS) != 0;

  if ((b_node->custom1 & CMP_NODEFLAG_BLUR_VARIABLE_SIZE) && connected_size_socket) {
    VariableSizeBokehBlurOperation *operation = new VariableSizeBokehBlurOperation();
    operation->set_quality(context.get_quality());
    operation->set_threshold(0.0f);
    operation->set_max_blur(b_node->custom4);
    operation->set_do_scale_size(true);

    converter.add_operation(operation);
    converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
    converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
    converter.map_input_socket(get_input_socket(2), operation->get_input_socket(2));
    converter.map_input_socket(get_input_socket(3), operation->get_input_socket(3));
    converter.map_output_socket(get_output_socket(0), operation->get_output_socket());
  }
  else {
    BokehBlurOperation *operation = new BokehBlurOperation();
    operation->set_quality(context.get_quality());
    operation->set_extend_bounds(extend_bounds);

    converter.add_operation(operation);
    converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
    converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));

    /* NOTE: on the bokeh blur operation the sockets are switched.
     * for this reason the next two lines are correct. Fix for #43771. */
    converter.map_input_socket(get_input_socket(2), operation->get_input_socket(3));
    converter.map_input_socket(get_input_socket(3), operation->get_input_socket(2));

    converter.map_output_socket(get_output_socket(0), operation->get_output_socket());

    if (!connected_size_socket) {
      operation->set_size(this->get_input_socket(2)->get_editor_value_float());
    }
  }
}

}  // namespace blender::compositor
