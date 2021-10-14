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

#include "COM_FilterNode.h"
#include "BKE_node.h"
#include "COM_ConvolutionEdgeFilterOperation.h"

namespace blender::compositor {

FilterNode::FilterNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void FilterNode::convert_to_operations(NodeConverter &converter,
                                       const CompositorContext & /*context*/) const
{
  NodeInput *input_socket = this->get_input_socket(0);
  NodeInput *input_image_socket = this->get_input_socket(1);
  NodeOutput *output_socket = this->get_output_socket(0);
  ConvolutionFilterOperation *operation = nullptr;

  switch (this->get_bnode()->custom1) {
    case CMP_FILT_SOFT:
      operation = new ConvolutionFilterOperation();
      operation->set3x3Filter(1 / 16.0f,
                              2 / 16.0f,
                              1 / 16.0f,
                              2 / 16.0f,
                              4 / 16.0f,
                              2 / 16.0f,
                              1 / 16.0f,
                              2 / 16.0f,
                              1 / 16.0f);
      break;
    case CMP_FILT_SHARP:
      operation = new ConvolutionFilterOperation();
      operation->set3x3Filter(-1, -1, -1, -1, 9, -1, -1, -1, -1);
      break;
    case CMP_FILT_LAPLACE:
      operation = new ConvolutionEdgeFilterOperation();
      operation->set3x3Filter(-1 / 8.0f,
                              -1 / 8.0f,
                              -1 / 8.0f,
                              -1 / 8.0f,
                              1.0f,
                              -1 / 8.0f,
                              -1 / 8.0f,
                              -1 / 8.0f,
                              -1 / 8.0f);
      break;
    case CMP_FILT_SOBEL:
      operation = new ConvolutionEdgeFilterOperation();
      operation->set3x3Filter(1, 2, 1, 0, 0, 0, -1, -2, -1);
      break;
    case CMP_FILT_PREWITT:
      operation = new ConvolutionEdgeFilterOperation();
      operation->set3x3Filter(1, 1, 1, 0, 0, 0, -1, -1, -1);
      break;
    case CMP_FILT_KIRSCH:
      operation = new ConvolutionEdgeFilterOperation();
      operation->set3x3Filter(5, 5, 5, -3, -3, -3, -2, -2, -2);
      break;
    case CMP_FILT_SHADOW:
      operation = new ConvolutionFilterOperation();
      operation->set3x3Filter(1, 2, 1, 0, 1, 0, -1, -2, -1);
      break;
    default:
      operation = new ConvolutionFilterOperation();
      operation->set3x3Filter(0, 0, 0, 0, 1, 0, 0, 0, 0);
      break;
  }
  converter.add_operation(operation);

  converter.map_input_socket(input_image_socket, operation->get_input_socket(0));
  converter.map_input_socket(input_socket, operation->get_input_socket(1));
  converter.map_output_socket(output_socket, operation->get_output_socket());

  converter.add_preview(operation->get_output_socket(0));
}

}  // namespace blender::compositor
