/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Node.h"

namespace blender::compositor {

/**
 * \brief KuwaharaNode
 * \ingroup Node
 */
class KuwaharaNode : public Node {
 public:
  KuwaharaNode(bNode *editor_node) : Node(editor_node) {}
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
