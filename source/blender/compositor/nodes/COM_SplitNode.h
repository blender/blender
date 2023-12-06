/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Node.h"
#include "DNA_node_types.h"

namespace blender::compositor {

/**
 * \brief SplitNode
 * \ingroup Node
 */
class SplitNode : public Node {
 public:
  SplitNode(bNode *node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
