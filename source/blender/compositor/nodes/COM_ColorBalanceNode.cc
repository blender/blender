/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorBalanceNode.h"
#include "COM_ColorBalanceASCCDLOperation.h"
#include "COM_ColorBalanceLGGOperation.h"

namespace blender::compositor {

ColorBalanceNode::ColorBalanceNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void ColorBalanceNode::convert_to_operations(NodeConverter &converter,
                                             const CompositorContext & /*context*/) const
{
  const bNode *node = this->get_bnode();
  NodeColorBalance *n = (NodeColorBalance *)node->storage;

  NodeInput *input_socket = this->get_input_socket(0);
  NodeInput *input_image_socket = this->get_input_socket(1);
  NodeOutput *output_socket = this->get_output_socket(0);

  NodeOperation *operation;
  if (node->custom1 == 0) {
    ColorBalanceLGGOperation *operationLGG = new ColorBalanceLGGOperation();

    float lift_lgg[3], gamma_inv[3];
    for (int c = 0; c < 3; c++) {
      lift_lgg[c] = 2.0f - n->lift[c];
      gamma_inv[c] = (n->gamma[c] != 0.0f) ? 1.0f / n->gamma[c] : 1000000.0f;
    }

    operationLGG->set_gain(n->gain);
    operationLGG->set_lift(lift_lgg);
    operationLGG->set_gamma_inv(gamma_inv);
    operation = operationLGG;
  }
  else {
    ColorBalanceASCCDLOperation *operationCDL = new ColorBalanceASCCDLOperation();

    float offset[3];
    copy_v3_fl(offset, n->offset_basis);
    add_v3_v3(offset, n->offset);

    operationCDL->set_offset(offset);
    operationCDL->set_power(n->power);
    operationCDL->set_slope(n->slope);
    operation = operationCDL;
  }
  converter.add_operation(operation);

  converter.map_input_socket(input_socket, operation->get_input_socket(0));
  converter.map_input_socket(input_image_socket, operation->get_input_socket(1));
  converter.map_output_socket(output_socket, operation->get_output_socket(0));
}

}  // namespace blender::compositor
