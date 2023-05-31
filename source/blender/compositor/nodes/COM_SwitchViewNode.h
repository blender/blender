/* SPDX-FileCopyrightText: 2015 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Node.h"
#include "COM_NodeOperation.h"
#include "DNA_node_types.h"

namespace blender::compositor {

/**
 * \brief SwitchViewNode
 * \ingroup Node
 */
class SwitchViewNode : public Node {
 public:
  SwitchViewNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
