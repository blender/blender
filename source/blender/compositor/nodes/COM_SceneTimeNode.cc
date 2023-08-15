/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SceneTimeNode.h"

#include "COM_SetValueOperation.h"

namespace blender::compositor {

SceneTimeNode::SceneTimeNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void SceneTimeNode::convert_to_operations(NodeConverter &converter,
                                          const CompositorContext &context) const
{
  SetValueOperation *SecondOperation = new SetValueOperation();
  SetValueOperation *frameOperation = new SetValueOperation();

  const int frameNumber = context.get_framenumber();
  const Scene *scene = context.get_scene();
  const double frameRate = (double(scene->r.frs_sec) / double(scene->r.frs_sec_base));

  SecondOperation->set_value(float(frameNumber / frameRate));
  converter.add_operation(SecondOperation);

  frameOperation->set_value(frameNumber);
  converter.add_operation(frameOperation);

  converter.map_output_socket(get_output_socket(0), SecondOperation->get_output_socket());
  converter.map_output_socket(get_output_socket(1), frameOperation->get_output_socket());
}

}  // namespace blender::compositor
