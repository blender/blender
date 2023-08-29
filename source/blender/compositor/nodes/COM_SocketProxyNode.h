/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Node.h"

namespace blender::compositor {

/**
 * \brief SocketProxyNode
 * \ingroup Node
 */
class SocketProxyNode : public Node {
 public:
  SocketProxyNode(bNode *editor_node,
                  bNodeSocket *editor_input,
                  bNodeSocket *editor_output,
                  bool use_conversion);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;

  bool get_use_conversion() const
  {
    return use_conversion_;
  }
  void set_use_conversion(bool use_conversion)
  {
    use_conversion_ = use_conversion;
  }

 private:
  /** If true, the proxy will convert input and output data to/from the proxy socket types. */
  bool use_conversion_;
};

class SocketBufferNode : public Node {
 public:
  SocketBufferNode(bNode *editor_node, bNodeSocket *editor_input, bNodeSocket *editor_output);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;
};

}  // namespace blender::compositor
