/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Node.h"

namespace blender::compositor {

/**
 * \brief AntiAliasingNode
 * \ingroup Node
 */
class AntiAliasingNode : public Node {
 public:
  AntiAliasingNode(bNode *editor_node) : Node(editor_node) {}
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
