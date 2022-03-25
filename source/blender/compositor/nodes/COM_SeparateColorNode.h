/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. */

#pragma once

#include "COM_Node.h"

namespace blender::compositor {

class SeparateColorNode : public Node {
 public:
  SeparateColorNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;

 protected:
  virtual NodeOperation *get_color_converter(const CompositorContext &context) const = 0;
};

class SeparateRGBANode : public SeparateColorNode {
 public:
  SeparateRGBANode(bNode *editor_node) : SeparateColorNode(editor_node)
  {
  }

  NodeOperation *get_color_converter(const CompositorContext &context) const override;
};

class SeparateHSVANode : public SeparateColorNode {
 public:
  SeparateHSVANode(bNode *editor_node) : SeparateColorNode(editor_node)
  {
  }

  NodeOperation *get_color_converter(const CompositorContext &context) const override;
};

class SeparateYCCANode : public SeparateColorNode {
 public:
  SeparateYCCANode(bNode *editor_node) : SeparateColorNode(editor_node)
  {
  }

  NodeOperation *get_color_converter(const CompositorContext &context) const override;
};

class SeparateYUVANode : public SeparateColorNode {
 public:
  SeparateYUVANode(bNode *editor_node) : SeparateColorNode(editor_node)
  {
  }

  NodeOperation *get_color_converter(const CompositorContext &context) const override;
};

}  // namespace blender::compositor
