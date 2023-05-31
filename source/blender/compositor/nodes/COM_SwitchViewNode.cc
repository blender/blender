/* SPDX-FileCopyrightText: 2015 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SwitchViewNode.h"

namespace blender::compositor {

SwitchViewNode::SwitchViewNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void SwitchViewNode::convert_to_operations(NodeConverter &converter,
                                           const CompositorContext &context) const
{
  NodeOperationOutput *result;
  const char *view_name = context.get_view_name();
  const bNode *bnode = this->get_bnode();

  /* get the internal index of the socket with a matching name */
  int nr = BLI_findstringindex(&bnode->inputs, view_name, offsetof(bNodeSocket, name));
  nr = MAX2(nr, 0);

  result = converter.add_input_proxy(get_input_socket(nr), false);
  converter.map_output_socket(get_output_socket(0), result);
}

}  // namespace blender::compositor
