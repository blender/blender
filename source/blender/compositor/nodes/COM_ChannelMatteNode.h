/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2012 Blender Foundation. */

#pragma once

#include "COM_Node.h"

namespace blender::compositor {

/**
 * \brief ChannelMatteNode
 * \ingroup Node
 */
class ChannelMatteNode : public Node {
 public:
  ChannelMatteNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
