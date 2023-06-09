/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_EllipseMaskNode.h"
#include "COM_EllipseMaskOperation.h"

#include "COM_ScaleOperation.h"
#include "COM_SetValueOperation.h"

namespace blender::compositor {

EllipseMaskNode::EllipseMaskNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void EllipseMaskNode::convert_to_operations(NodeConverter &converter,
                                            const CompositorContext &context) const
{
  NodeInput *input_socket = this->get_input_socket(0);
  NodeOutput *output_socket = this->get_output_socket(0);

  EllipseMaskOperation *operation;
  operation = new EllipseMaskOperation();
  operation->set_data((NodeEllipseMask *)this->get_bnode()->storage);
  operation->set_mask_type(this->get_bnode()->custom1);
  converter.add_operation(operation);

  if (input_socket->is_linked()) {
    converter.map_input_socket(input_socket, operation->get_input_socket(0));
    converter.map_output_socket(output_socket, operation->get_output_socket());
  }
  else {
    /* Value operation to produce original transparent image */
    SetValueOperation *value_operation = new SetValueOperation();
    value_operation->set_value(0.0f);
    converter.add_operation(value_operation);

    /* Scale that image up to render resolution */
    const RenderData *rd = context.get_render_data();
    const float render_size_factor = context.get_render_percentage_as_factor();
    ScaleFixedSizeOperation *scale_operation = new ScaleFixedSizeOperation();

    scale_operation->set_is_aspect(false);
    scale_operation->set_is_crop(false);
    scale_operation->set_offset(0.0f, 0.0f);
    scale_operation->set_new_width(rd->xsch * render_size_factor);
    scale_operation->set_new_height(rd->ysch * render_size_factor);
    scale_operation->get_input_socket(0)->set_resize_mode(ResizeMode::Align);
    converter.add_operation(scale_operation);

    converter.add_link(value_operation->get_output_socket(0),
                       scale_operation->get_input_socket(0));
    converter.add_link(scale_operation->get_output_socket(0), operation->get_input_socket(0));
    converter.map_output_socket(output_socket, operation->get_output_socket(0));
  }

  converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
}

}  // namespace blender::compositor
