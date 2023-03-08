/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. */

#include "COM_CompositorNode.h"
#include "COM_CompositorOperation.h"

namespace blender::compositor {

CompositorNode::CompositorNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void CompositorNode::convert_to_operations(NodeConverter &converter,
                                           const CompositorContext &context) const
{
  const bNode *editor_node = this->get_bnode();
  bool is_active = ((editor_node->flag & NODE_DO_OUTPUT_RECALC) || context.is_rendering()) &&
                   (editor_node->flag & NODE_DO_OUTPUT);
  bool ignore_alpha = (editor_node->custom2 & CMP_NODE_OUTPUT_IGNORE_ALPHA) != 0;

  NodeInput *image_socket = this->get_input_socket(0);
  NodeInput *alpha_socket = this->get_input_socket(1);
  NodeInput *depth_socket = this->get_input_socket(2);

  CompositorOperation *compositor_operation = new CompositorOperation();
  compositor_operation->set_scene(context.get_scene());
  compositor_operation->set_scene_name(context.get_scene()->id.name);
  compositor_operation->set_render_data(context.get_render_data());
  compositor_operation->set_view_name(context.get_view_name());
  compositor_operation->set_bnodetree(context.get_bnodetree());
  /* alpha socket gives either 1 or a custom alpha value if "use alpha" is enabled */
  compositor_operation->set_use_alpha_input(ignore_alpha || alpha_socket->is_linked());
  compositor_operation->set_active(is_active);

  converter.add_operation(compositor_operation);
  converter.map_input_socket(image_socket, compositor_operation->get_input_socket(0));
  /* only use alpha link if "use alpha" is enabled */
  if (ignore_alpha) {
    converter.add_input_value(compositor_operation->get_input_socket(1), 1.0f);
  }
  else {
    converter.map_input_socket(alpha_socket, compositor_operation->get_input_socket(1));
  }
  converter.map_input_socket(depth_socket, compositor_operation->get_input_socket(2));

  converter.add_node_input_preview(image_socket);
}

}  // namespace blender::compositor
