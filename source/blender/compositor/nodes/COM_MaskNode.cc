/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2012 Blender Foundation. */

#include "COM_MaskNode.h"
#include "COM_MaskOperation.h"
#include "COM_ScaleOperation.h"

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

  const bNode *editor_node = this->get_bnode();
  const NodeMask *data = (const NodeMask *)editor_node->storage;
  Mask *mask = (Mask *)editor_node->id;

  /* Always connect the output image. */
  MaskOperation *operation = new MaskOperation();

  if (editor_node->custom1 & CMP_NODE_MASK_FLAG_SIZE_FIXED) {
    operation->set_mask_width(data->size_x);
    operation->set_mask_height(data->size_y);
  }
  else if (editor_node->custom1 & CMP_NODE_MASK_FLAG_SIZE_FIXED_SCENE) {
    operation->set_mask_width(data->size_x * render_size_factor);
    operation->set_mask_height(data->size_y * render_size_factor);
  }
  else {
    operation->set_mask_width(rd->xsch * render_size_factor);
    operation->set_mask_height(rd->ysch * render_size_factor);
  }

  operation->set_mask(mask);
  operation->set_framenumber(context.get_framenumber());
  operation->set_feather(bool(editor_node->custom1 & CMP_NODE_MASK_FLAG_NO_FEATHER) == 0);

  if ((editor_node->custom1 & CMP_NODE_MASK_FLAG_MOTION_BLUR) && (editor_node->custom2 > 1) &&
      (editor_node->custom3 > FLT_EPSILON))
  {
    operation->set_motion_blur_samples(editor_node->custom2);
    operation->set_motion_blur_shutter(editor_node->custom3);
  }

  converter.add_operation(operation);

  ScaleFixedSizeOperation *scale_operation = new ScaleFixedSizeOperation();
  scale_operation->set_variable_size(true);
  /* Consider aspect ratio from scene. */
  const int new_height = rd->xasp / rd->yasp * operation->get_mask_height();
  scale_operation->set_new_height(new_height);
  scale_operation->set_new_width(operation->get_mask_width());
  scale_operation->set_is_aspect(false);
  scale_operation->set_is_crop(false);
  scale_operation->set_offset(0.0f, 0.0f);
  scale_operation->set_scale_canvas_max_size({float(data->size_x), float(data->size_y)});

  converter.add_operation(scale_operation);
  converter.add_link(operation->get_output_socket(0), scale_operation->get_input_socket(0));

  converter.map_output_socket(output_mask, scale_operation->get_output_socket(0));
}

}  // namespace blender::compositor
