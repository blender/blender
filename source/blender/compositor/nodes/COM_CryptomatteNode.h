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
 * Copyright 2018, Blender Foundation.
 */

#pragma once

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "COM_CryptomatteOperation.h"
#include "COM_Node.h"

namespace blender::compositor {

/**
 * \brief CryptomatteNode
 * \ingroup Node
 */
class CryptomatteBaseNode : public Node {
 protected:
  CryptomatteBaseNode(bNode *editor_node) : Node(editor_node)
  {
    /* pass */
  }

 public:
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;

 protected:
  virtual CryptomatteOperation *create_cryptomatte_operation(
      NodeConverter &converter,
      const CompositorContext &context,
      const bNode &node,
      const NodeCryptomatte *cryptomatte_settings) const = 0;
};

class CryptomatteNode : public CryptomatteBaseNode {
 public:
  CryptomatteNode(bNode *editor_node) : CryptomatteBaseNode(editor_node)
  {
    /* pass */
  }

 protected:
  CryptomatteOperation *create_cryptomatte_operation(
      NodeConverter &converter,
      const CompositorContext &context,
      const bNode &node,
      const NodeCryptomatte *cryptomatte_settings) const override;

 private:
  static Vector<NodeOperation *> create_input_operations(const CompositorContext &context,
                                                         const bNode &node);
  static void input_operations_from_render_source(const CompositorContext &context,
                                                  const bNode &node,
                                                  Vector<NodeOperation *> &r_input_operations);
  static void input_operations_from_image_source(const CompositorContext &context,
                                                 const bNode &node,
                                                 Vector<NodeOperation *> &r_input_operations);
};

class CryptomatteLegacyNode : public CryptomatteBaseNode {
 public:
  CryptomatteLegacyNode(bNode *editor_node) : CryptomatteBaseNode(editor_node)
  {
    /* pass */
  }

 protected:
  CryptomatteOperation *create_cryptomatte_operation(
      NodeConverter &converter,
      const CompositorContext &context,
      const bNode &node,
      const NodeCryptomatte *cryptomatte_settings) const override;
};

}  // namespace blender::compositor
