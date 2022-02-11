/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. */

#pragma once

#include "COM_Node.h"

namespace blender::compositor {

/**
 * \brief SeparateXYZNode
 * \ingroup Node
 */
class CombineXYZNode : public Node {
 public:
  CombineXYZNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
