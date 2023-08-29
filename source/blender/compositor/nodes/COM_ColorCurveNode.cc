/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorCurveNode.h"
#include "COM_ColorCurveOperation.h"

namespace blender::compositor {

ColorCurveNode::ColorCurveNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void ColorCurveNode::convert_to_operations(NodeConverter &converter,
                                           const CompositorContext & /*context*/) const
{
  if (this->get_input_socket(2)->is_linked() || this->get_input_socket(3)->is_linked()) {
    ColorCurveOperation *operation = new ColorCurveOperation();
    operation->set_curve_mapping((const CurveMapping *)this->get_bnode()->storage);
    converter.add_operation(operation);

    converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
    converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
    converter.map_input_socket(get_input_socket(2), operation->get_input_socket(2));
    converter.map_input_socket(get_input_socket(3), operation->get_input_socket(3));

    converter.map_output_socket(get_output_socket(0), operation->get_output_socket());
  }
  else {
    ConstantLevelColorCurveOperation *operation = new ConstantLevelColorCurveOperation();
    float col[4];
    this->get_input_socket(2)->get_editor_value_color(col);
    operation->set_black_level(col);
    this->get_input_socket(3)->get_editor_value_color(col);
    operation->set_white_level(col);
    operation->set_curve_mapping((const CurveMapping *)this->get_bnode()->storage);
    converter.add_operation(operation);

    converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
    converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
    converter.map_output_socket(get_output_socket(0), operation->get_output_socket());
  }
}

}  // namespace blender::compositor
