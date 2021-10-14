/*
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
 * Copyright 2011, Blender Foundation.
 */

#include "COM_ScaleNode.h"

#include "BKE_node.h"
#include "COM_ScaleOperation.h"
#include "COM_SetValueOperation.h"

namespace blender::compositor {

ScaleNode::ScaleNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void ScaleNode::convert_to_operations(NodeConverter &converter,
                                      const CompositorContext &context) const
{
  bNode *bnode = this->get_bnode();

  NodeInput *input_socket = this->get_input_socket(0);
  NodeInput *input_xsocket = this->get_input_socket(1);
  NodeInput *input_ysocket = this->get_input_socket(2);
  NodeOutput *output_socket = this->get_output_socket(0);

  switch (bnode->custom1) {
    case CMP_SCALE_RELATIVE: {
      ScaleRelativeOperation *operation = new ScaleRelativeOperation();
      converter.add_operation(operation);

      converter.map_input_socket(input_socket, operation->get_input_socket(0));
      converter.map_input_socket(input_xsocket, operation->get_input_socket(1));
      converter.map_input_socket(input_ysocket, operation->get_input_socket(2));
      converter.map_output_socket(output_socket, operation->get_output_socket(0));

      operation->set_variable_size(input_xsocket->is_linked() || input_ysocket->is_linked());
      operation->set_scale_canvas_max_size(context.get_render_size() * 1.5f);

      break;
    }
    case CMP_SCALE_SCENEPERCENT: {
      SetValueOperation *scale_factor_operation = new SetValueOperation();
      scale_factor_operation->set_value(context.get_render_percentage_as_factor());
      converter.add_operation(scale_factor_operation);

      ScaleRelativeOperation *operation = new ScaleRelativeOperation();
      converter.add_operation(operation);

      converter.map_input_socket(input_socket, operation->get_input_socket(0));
      converter.add_link(scale_factor_operation->get_output_socket(),
                         operation->get_input_socket(1));
      converter.add_link(scale_factor_operation->get_output_socket(),
                         operation->get_input_socket(2));
      converter.map_output_socket(output_socket, operation->get_output_socket(0));

      operation->set_variable_size(input_xsocket->is_linked() || input_ysocket->is_linked());
      operation->set_scale_canvas_max_size(context.get_render_size() * 1.5f);

      break;
    }
    case CMP_SCALE_RENDERPERCENT: {
      const RenderData *rd = context.get_render_data();
      const float render_size_factor = context.get_render_percentage_as_factor();
      ScaleFixedSizeOperation *operation = new ScaleFixedSizeOperation();
      /* framing options */
      operation->set_is_aspect((bnode->custom2 & CMP_SCALE_RENDERSIZE_FRAME_ASPECT) != 0);
      operation->set_is_crop((bnode->custom2 & CMP_SCALE_RENDERSIZE_FRAME_CROP) != 0);
      operation->set_offset(bnode->custom3, bnode->custom4);
      operation->set_new_width(rd->xsch * render_size_factor);
      operation->set_new_height(rd->ysch * render_size_factor);
      converter.add_operation(operation);

      converter.map_input_socket(input_socket, operation->get_input_socket(0));
      converter.map_output_socket(output_socket, operation->get_output_socket(0));

      operation->set_variable_size(input_xsocket->is_linked() || input_ysocket->is_linked());
      operation->set_scale_canvas_max_size(context.get_render_size() * 3.0f);

      break;
    }
    case CMP_SCALE_ABSOLUTE: {
      /* TODO: what is the use of this one.... perhaps some issues when the ui was updated... */
      ScaleAbsoluteOperation *operation = new ScaleAbsoluteOperation();
      converter.add_operation(operation);

      converter.map_input_socket(input_socket, operation->get_input_socket(0));
      converter.map_input_socket(input_xsocket, operation->get_input_socket(1));
      converter.map_input_socket(input_ysocket, operation->get_input_socket(2));
      converter.map_output_socket(output_socket, operation->get_output_socket(0));

      operation->set_variable_size(input_xsocket->is_linked() || input_ysocket->is_linked());
      operation->set_scale_canvas_max_size(context.get_render_size() * 1.5f);

      break;
    }
  }
}

}  // namespace blender::compositor
