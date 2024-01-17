/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_scene_types.h"

#include "COM_Node.h"

namespace blender::compositor {

/**
 * \brief DefocusNode
 * \ingroup Node
 */
class DefocusNode : public Node {
 public:
  DefocusNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
  const Scene *get_scene(const CompositorContext &context) const;
};

}  // namespace blender::compositor
