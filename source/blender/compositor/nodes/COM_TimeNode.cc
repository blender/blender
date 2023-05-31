/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_TimeNode.h"

#include "COM_SetValueOperation.h"

#include "BKE_colortools.h"

namespace blender::compositor {

TimeNode::TimeNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void TimeNode::convert_to_operations(NodeConverter &converter,
                                     const CompositorContext &context) const
{
  SetValueOperation *operation = new SetValueOperation();
  const bNode *node = this->get_bnode();

  /* stack order output: fac */
  float fac = 0.0f;
  const int framenumber = context.get_framenumber();

  if (framenumber < node->custom1) {
    fac = 0.0f;
  }
  else if (framenumber > node->custom2) {
    fac = 1.0f;
  }
  else if (node->custom1 < node->custom2) {
    fac = (context.get_framenumber() - node->custom1) / float(node->custom2 - node->custom1);
  }

  BKE_curvemapping_init((CurveMapping *)node->storage);
  fac = BKE_curvemapping_evaluateF((CurveMapping *)node->storage, 0, fac);
  operation->set_value(clamp_f(fac, 0.0f, 1.0f));
  converter.add_operation(operation);

  converter.map_output_socket(get_output_socket(0), operation->get_output_socket());
}

}  // namespace blender::compositor
