/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_VectorCurveNode.h"
#include "COM_VectorCurveOperation.h"

namespace blender::compositor {

VectorCurveNode::VectorCurveNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void VectorCurveNode::convert_to_operations(NodeConverter &converter,
                                            const CompositorContext & /*context*/) const
{
  VectorCurveOperation *operation = new VectorCurveOperation();
  operation->set_curve_mapping((const CurveMapping *)this->get_bnode()->storage);
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket());
}

}  // namespace blender::compositor
