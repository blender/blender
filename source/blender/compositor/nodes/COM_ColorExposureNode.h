/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Node.h"

namespace blender::compositor {

/**
 * \brief ExposureNode
 * \ingroup Node
 */
class ExposureNode : public Node {
 public:
  ExposureNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
