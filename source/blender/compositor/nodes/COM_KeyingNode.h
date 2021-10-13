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

#pragma once

#include "COM_Node.h"

namespace blender::compositor {

/**
 * \brief KeyingNode
 * \ingroup Node
 */
class KeyingNode : public Node {
 protected:
  NodeOperationOutput *setup_pre_blur(NodeConverter &converter,
                                      NodeInput *input_image,
                                      int size) const;
  NodeOperationOutput *setup_post_blur(NodeConverter &converter,
                                       NodeOperationOutput *post_blur_input,
                                       int size) const;
  NodeOperationOutput *setup_dilate_erode(NodeConverter &converter,
                                          NodeOperationOutput *dilate_erode_input,
                                          int distance) const;
  NodeOperationOutput *setup_feather(NodeConverter &converter,
                                     const CompositorContext &context,
                                     NodeOperationOutput *feather_input,
                                     int falloff,
                                     int distance) const;
  NodeOperationOutput *setup_despill(NodeConverter &converter,
                                     NodeOperationOutput *despill_input,
                                     NodeInput *input_screen,
                                     float factor,
                                     float color_balance) const;
  NodeOperationOutput *setup_clip(NodeConverter &converter,
                                  NodeOperationOutput *clip_input,
                                  int kernel_radius,
                                  float kernel_tolerance,
                                  float clip_black,
                                  float clip_white,
                                  bool edge_matte) const;

 public:
  KeyingNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
