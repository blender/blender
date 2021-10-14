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
 * Copyright 2011, Blender Foundation.
 */

#include "COM_TextureNode.h"
#include "COM_TextureOperation.h"

namespace blender::compositor {

TextureNode::TextureNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void TextureNode::convert_to_operations(NodeConverter &converter,
                                        const CompositorContext &context) const
{
  bNode *editor_node = this->get_bnode();
  Tex *texture = (Tex *)editor_node->id;
  TextureOperation *operation = new TextureOperation();
  const ColorManagedDisplaySettings *display_settings = context.get_display_settings();
  bool scene_color_manage = !STREQ(display_settings->display_device, "None");
  operation->set_texture(texture);
  operation->set_render_data(context.get_render_data());
  operation->set_scene_color_manage(scene_color_manage);
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
  converter.map_output_socket(get_output_socket(1), operation->get_output_socket());

  converter.add_preview(operation->get_output_socket());

  TextureAlphaOperation *alpha_operation = new TextureAlphaOperation();
  alpha_operation->set_texture(texture);
  alpha_operation->set_render_data(context.get_render_data());
  alpha_operation->set_scene_color_manage(scene_color_manage);
  converter.add_operation(alpha_operation);

  converter.map_input_socket(get_input_socket(0), alpha_operation->get_input_socket(0));
  converter.map_input_socket(get_input_socket(1), alpha_operation->get_input_socket(1));
  converter.map_output_socket(get_output_socket(0), alpha_operation->get_output_socket());
}

}  // namespace blender::compositor
