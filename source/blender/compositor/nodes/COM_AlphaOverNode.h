/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. */

#pragma once

#include "COM_Node.h"

namespace blender::compositor {

/**
 * \brief AlphaOverNode
 * \ingroup Node
 */
class AlphaOverNode : public Node {
 public:
  AlphaOverNode(bNode *editor_node) : Node(editor_node) {}
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
