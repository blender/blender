/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_IDMaskNode.h"
#include "COM_IDMaskOperation.h"
#include "COM_SMAAOperation.h"

namespace blender::compositor {

IDMaskNode::IDMaskNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}
void IDMaskNode::convert_to_operations(NodeConverter &converter,
                                       const CompositorContext & /*context*/) const
{
  const bNode *bnode = this->get_bnode();

  IDMaskOperation *operation;
  operation = new IDMaskOperation();
  operation->set_object_index(bnode->custom1);
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  if (bnode->custom2 == 0) {
    converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
  }
  else {
    SMAAOperation *smaa_operation = new SMAAOperation();
    converter.add_operation(smaa_operation);
    converter.add_link(operation->get_output_socket(0), smaa_operation->get_input_socket(0));
    converter.map_output_socket(get_output_socket(0), smaa_operation->get_output_socket());
  }
}

}  // namespace blender::compositor
