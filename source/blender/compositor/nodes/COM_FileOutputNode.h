/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Node.h"

#include "COM_FileOutputOperation.h"

#include "DNA_node_types.h"

namespace blender::compositor {

/**
 * \brief FileOutputNode
 * \ingroup Node
 */
class FileOutputNode : public Node {
 public:
  FileOutputNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
