/* SPDX-FileCopyrightText: 2018 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
