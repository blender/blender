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

#include "COM_MaskNode.h"
#include "COM_MaskOperation.h"

namespace blender::compositor {

MaskNode::MaskNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void MaskNode::convert_to_operations(NodeConverter &converter,
                                     const CompositorContext &context) const
{
  const RenderData *rd = context.get_render_data();
  const float render_size_factor = context.get_render_percentage_as_factor();

  NodeOutput *output_mask = this->get_output_socket(0);

  bNode *editor_node = this->get_bnode();
  NodeMask *data = (NodeMask *)editor_node->storage;
  Mask *mask = (Mask *)editor_node->id;

  /* Always connect the output image. */
  MaskOperation *operation = new MaskOperation();

  if (editor_node->custom1 & CMP_NODEFLAG_MASK_FIXED) {
    operation->set_mask_width(data->size_x);
    operation->set_mask_height(data->size_y);
  }
  else if (editor_node->custom1 & CMP_NODEFLAG_MASK_FIXED_SCENE) {
    operation->set_mask_width(data->size_x * render_size_factor);
    operation->set_mask_height(data->size_y * render_size_factor);
  }
  else {
    operation->set_mask_width(rd->xsch * render_size_factor);
    operation->set_mask_height(rd->ysch * render_size_factor);
  }

  operation->set_mask(mask);
  operation->set_framenumber(context.get_framenumber());
  operation->set_feather((bool)(editor_node->custom1 & CMP_NODEFLAG_MASK_NO_FEATHER) == 0);

  if ((editor_node->custom1 & CMP_NODEFLAG_MASK_MOTION_BLUR) && (editor_node->custom2 > 1) &&
      (editor_node->custom3 > FLT_EPSILON)) {
    operation->set_motion_blur_samples(editor_node->custom2);
    operation->set_motion_blur_shutter(editor_node->custom3);
  }

  converter.add_operation(operation);
  converter.map_output_socket(output_mask, operation->get_output_socket());
}

}  // namespace blender::compositor
