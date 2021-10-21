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
  bNode *editor_node = this->get_bnode();
  bool do_output = (editor_node->flag & NODE_DO_OUTPUT_RECALC || context.is_rendering()) &&
                   (editor_node->flag & NODE_DO_OUTPUT);

  NodeInput *image1Socket = this->get_input_socket(0);
  NodeInput *image2Socket = this->get_input_socket(1);
  Image *image = (Image *)this->get_bnode()->id;
  ImageUser *image_user = (ImageUser *)this->get_bnode()->storage;

  SplitOperation *split_viewer_operation = new SplitOperation();
  split_viewer_operation->set_split_percentage(this->get_bnode()->custom1);
  split_viewer_operation->set_xsplit(!this->get_bnode()->custom2);

  converter.add_operation(split_viewer_operation);
  converter.map_input_socket(image1Socket, split_viewer_operation->get_input_socket(0));
  converter.map_input_socket(image2Socket, split_viewer_operation->get_input_socket(1));

  ViewerOperation *viewer_operation = new ViewerOperation();
  viewer_operation->set_image(image);
  viewer_operation->set_image_user(image_user);
  viewer_operation->set_view_settings(context.get_view_settings());
  viewer_operation->set_display_settings(context.get_display_settings());
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

  if (do_output) {
    converter.register_viewer(viewer_operation);
  }
}

}  // namespace blender::compositor
