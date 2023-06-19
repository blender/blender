/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Node.h"
#include "DNA_node_types.h"

namespace blender::compositor {

/**
 * \brief MovieClipNode
 * \ingroup Node
 */
class MovieClipNode : public Node {
 public:
  MovieClipNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
