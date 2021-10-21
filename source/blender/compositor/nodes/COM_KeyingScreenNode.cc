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
 * Copyright 2012, Blender Foundation.
 */

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
  bNode *editor_node = this->get_bnode();
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
