/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Node.h"

namespace blender::compositor {

class SeparateColorNodeLegacy : public Node {
 public:
  SeparateColorNodeLegacy(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;

 protected:
  virtual NodeOperation *get_color_converter(const CompositorContext &context) const = 0;
};

class SeparateRGBANode : public SeparateColorNodeLegacy {
 public:
  SeparateRGBANode(bNode *editor_node) : SeparateColorNodeLegacy(editor_node) {}

  NodeOperation *get_color_converter(const CompositorContext &context) const override;
};

class SeparateHSVANode : public SeparateColorNodeLegacy {
 public:
  SeparateHSVANode(bNode *editor_node) : SeparateColorNodeLegacy(editor_node) {}

  NodeOperation *get_color_converter(const CompositorContext &context) const override;
};

class SeparateYCCANode : public SeparateColorNodeLegacy {
 public:
  SeparateYCCANode(bNode *editor_node) : SeparateColorNodeLegacy(editor_node) {}

  NodeOperation *get_color_converter(const CompositorContext &context) const override;
};

class SeparateYUVANode : public SeparateColorNodeLegacy {
 public:
  SeparateYUVANode(bNode *editor_node) : SeparateColorNodeLegacy(editor_node) {}

  NodeOperation *get_color_converter(const CompositorContext &context) const override;
};

}  // namespace blender::compositor
