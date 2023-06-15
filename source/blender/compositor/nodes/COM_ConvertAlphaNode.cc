/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ConvertAlphaNode.h"
#include "COM_ConvertOperation.h"

namespace blender::compositor {

void ConvertAlphaNode::convert_to_operations(NodeConverter &converter,
                                             const CompositorContext & /*context*/) const
{
  NodeOperation *operation = nullptr;
  const bNode *node = this->get_bnode();

  /* value hardcoded in rna_nodetree.c */
  if (node->custom1 == 1) {
    operation = new ConvertPremulToStraightOperation();
  }
  else {
    operation = new ConvertStraightToPremulOperation();
  }

  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket());
}

}  // namespace blender::compositor
