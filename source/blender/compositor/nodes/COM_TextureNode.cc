/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  const bNode *editor_node = this->get_bnode();
  Tex *texture = (Tex *)editor_node->id;
  TextureOperation *operation = new TextureOperation();
  bool scene_color_manage = !STREQ(context.get_scene()->display_settings.display_device, "None");
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
