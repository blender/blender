/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

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
