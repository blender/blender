/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
