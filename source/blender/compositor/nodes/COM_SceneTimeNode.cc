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
 * Copyright 2022, Blender Foundation.
 */

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
  const double frameRate = (((double)scene->r.frs_sec) / (double)scene->r.frs_sec_base);

  SecondOperation->set_value(float(frameNumber / frameRate));
  converter.add_operation(SecondOperation);

  frameOperation->set_value(frameNumber);
  converter.add_operation(frameOperation);

  converter.map_output_socket(get_output_socket(0), SecondOperation->get_output_socket());
  converter.map_output_socket(get_output_socket(1), frameOperation->get_output_socket());
}

}  // namespace blender::compositor
