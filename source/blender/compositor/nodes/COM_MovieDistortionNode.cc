/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MovieDistortionNode.h"

#include "COM_MovieDistortionOperation.h"

namespace blender::compositor {

MovieDistortionNode::MovieDistortionNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void MovieDistortionNode::convert_to_operations(NodeConverter &converter,
                                                const CompositorContext &context) const
{
  const bNode *bnode = this->get_bnode();
  MovieClip *clip = (MovieClip *)bnode->id;

  NodeInput *input_socket = this->get_input_socket(0);
  NodeOutput *output_socket = this->get_output_socket(0);

  MovieDistortionOperation *operation = new MovieDistortionOperation(bnode->custom1 == 1);
  operation->set_movie_clip(clip);
  operation->set_framenumber(context.get_framenumber());
  converter.add_operation(operation);

  converter.map_input_socket(input_socket, operation->get_input_socket(0));
  converter.map_output_socket(output_socket, operation->get_output_socket(0));
}

}  // namespace blender::compositor
