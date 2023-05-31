/* SPDX-FileCopyrightText: 2012 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Node.h"
#include "DNA_node_types.h"

namespace blender::compositor {

/**
 * \brief MaskNode
 * \ingroup Node
 */
class MaskNode : public Node {
 public:
  MaskNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
