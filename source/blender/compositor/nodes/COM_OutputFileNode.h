/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Node.h"

#include "COM_OutputFileMultiViewOperation.h"

#include "DNA_node_types.h"

namespace blender::compositor {

/**
 * \brief OutputFileNode
 * \ingroup Node
 */
class OutputFileNode : public Node {
 public:
  OutputFileNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;

 private:
  void add_preview_to_first_linked_input(NodeConverter &converter) const;
  void add_input_sockets(OutputOpenExrMultiLayerOperation &operation) const;
  void map_input_sockets(NodeConverter &converter,
                         OutputOpenExrMultiLayerOperation &operation) const;
};

}  // namespace blender::compositor
