/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_KeyingScreenNode.h"
#include "COM_KeyingScreenOperation.h"

namespace blender::compositor {

KeyingScreenNode::KeyingScreenNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void KeyingScreenNode::convert_to_operations(NodeConverter &converter,
                                             const CompositorContext &context) const
{
  const bNode *editor_node = this->get_bnode();
  MovieClip *clip = (MovieClip *)editor_node->id;
  NodeKeyingScreenData *keyingscreen_data = (NodeKeyingScreenData *)editor_node->storage;

  NodeOutput *output_screen = this->get_output_socket(0);

  /* Always connect the output image. */
  KeyingScreenOperation *operation = new KeyingScreenOperation();
  operation->set_movie_clip(clip);
  operation->set_tracking_object(keyingscreen_data->tracking_object);
  operation->set_framenumber(context.get_framenumber());
  converter.add_operation(operation);

  converter.map_output_socket(output_screen, operation->get_output_socket());
}

}  // namespace blender::compositor
