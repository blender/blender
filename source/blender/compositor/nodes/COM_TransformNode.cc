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

#include "COM_TransformNode.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"
#include "COM_SetSamplerOperation.h"
#include "COM_TranslateOperation.h"

namespace blender::compositor {

TransformNode::TransformNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void TransformNode::convert_to_operations(NodeConverter &converter,
                                          const CompositorContext &context) const
{
  NodeInput *image_input = this->get_input_socket(0);
  NodeInput *x_input = this->get_input_socket(1);
  NodeInput *y_input = this->get_input_socket(2);
  NodeInput *angle_input = this->get_input_socket(3);
  NodeInput *scale_input = this->get_input_socket(4);

  switch (context.get_execution_model()) {
    case eExecutionModel::Tiled: {
      ScaleRelativeOperation *scale_operation = new ScaleRelativeOperation();
      converter.add_operation(scale_operation);

      RotateOperation *rotate_operation = new RotateOperation();
      rotate_operation->set_do_degree2_rad_conversion(false);
      converter.add_operation(rotate_operation);

      TranslateOperation *translate_operation = new TranslateOperation();
      converter.add_operation(translate_operation);

      SetSamplerOperation *sampler = new SetSamplerOperation();
      sampler->set_sampler((PixelSampler)this->get_bnode()->custom1);
      converter.add_operation(sampler);

      converter.map_input_socket(image_input, sampler->get_input_socket(0));
      converter.add_link(sampler->get_output_socket(), scale_operation->get_input_socket(0));
      converter.map_input_socket(scale_input, scale_operation->get_input_socket(1));
      converter.map_input_socket(scale_input,
                                 scale_operation->get_input_socket(2));  // xscale = yscale

      converter.add_link(scale_operation->get_output_socket(),
                         rotate_operation->get_input_socket(0));
      converter.map_input_socket(angle_input, rotate_operation->get_input_socket(1));

      converter.add_link(rotate_operation->get_output_socket(),
                         translate_operation->get_input_socket(0));
      converter.map_input_socket(x_input, translate_operation->get_input_socket(1));
      converter.map_input_socket(y_input, translate_operation->get_input_socket(2));

      converter.map_output_socket(get_output_socket(), translate_operation->get_output_socket());
      break;
    }
    case eExecutionModel::FullFrame: {
      ScaleRelativeOperation *scale_operation = new ScaleRelativeOperation();
      converter.add_operation(scale_operation);

      RotateOperation *rotate_operation = new RotateOperation();
      rotate_operation->set_do_degree2_rad_conversion(false);
      converter.add_operation(rotate_operation);

      TranslateOperation *translate_operation = new TranslateCanvasOperation();
      converter.add_operation(translate_operation);

      PixelSampler sampler = (PixelSampler)this->get_bnode()->custom1;
      scale_operation->set_sampler(sampler);
      rotate_operation->set_sampler(sampler);
      scale_operation->set_scale_canvas_max_size(context.get_render_size());

      converter.map_input_socket(image_input, scale_operation->get_input_socket(0));
      converter.map_input_socket(scale_input, scale_operation->get_input_socket(1));
      converter.map_input_socket(scale_input,
                                 scale_operation->get_input_socket(2));  // xscale = yscale

      converter.add_link(scale_operation->get_output_socket(),
                         rotate_operation->get_input_socket(0));
      converter.map_input_socket(angle_input, rotate_operation->get_input_socket(1));

      converter.add_link(rotate_operation->get_output_socket(),
                         translate_operation->get_input_socket(0));
      converter.map_input_socket(x_input, translate_operation->get_input_socket(1));
      converter.map_input_socket(y_input, translate_operation->get_input_socket(2));

      converter.map_output_socket(get_output_socket(), translate_operation->get_output_socket());
      break;
    }
  }
}

}  // namespace blender::compositor
