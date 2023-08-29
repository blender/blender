/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SplitViewerNode.h"

#include "COM_SplitOperation.h"
#include "COM_ViewerOperation.h"

namespace blender::compositor {

SplitViewerNode::SplitViewerNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void SplitViewerNode::convert_to_operations(NodeConverter &converter,
                                            const CompositorContext &context) const
{
  const bNode *editor_node = this->get_bnode();
  bool is_active = (editor_node->flag & NODE_DO_OUTPUT_RECALC || context.is_rendering()) &&
                   (editor_node->flag & NODE_DO_OUTPUT);

  NodeInput *image1Socket = this->get_input_socket(0);
  NodeInput *image2Socket = this->get_input_socket(1);
  Image *image = (Image *)this->get_bnode()->id;
  ImageUser *image_user = (ImageUser *)this->get_bnode()->storage;
  Scene *scene = context.get_scene();

  SplitOperation *split_viewer_operation = new SplitOperation();
  split_viewer_operation->set_split_percentage(this->get_bnode()->custom1);
  split_viewer_operation->set_xsplit(!this->get_bnode()->custom2);

  converter.add_operation(split_viewer_operation);
  converter.map_input_socket(image1Socket, split_viewer_operation->get_input_socket(0));
  converter.map_input_socket(image2Socket, split_viewer_operation->get_input_socket(1));

  ViewerOperation *viewer_operation = new ViewerOperation();
  viewer_operation->set_image(image);
  viewer_operation->set_image_user(image_user);
  viewer_operation->set_view_settings(&scene->view_settings);
  viewer_operation->set_display_settings(&scene->display_settings);
  viewer_operation->set_render_data(context.get_render_data());
  viewer_operation->set_view_name(context.get_view_name());

  /* defaults - the viewer node has these options but not exposed for split view
   * we could use the split to define an area of interest on one axis at least */
  viewer_operation->set_chunk_order(ChunkOrdering::Default);
  viewer_operation->setCenterX(0.5f);
  viewer_operation->setCenterY(0.5f);

  converter.add_operation(viewer_operation);
  converter.add_link(split_viewer_operation->get_output_socket(),
                     viewer_operation->get_input_socket(0));

  converter.add_preview(split_viewer_operation->get_output_socket());

  if (is_active) {
    converter.register_viewer(viewer_operation);
  }
}

}  // namespace blender::compositor
