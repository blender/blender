/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. */

#pragma once

#include "COM_Node.h"

namespace blender::compositor {

class CombineColorNode : public Node {
 public:
  CombineColorNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;

 protected:
  virtual NodeOperation *get_color_converter(const CompositorContext &context) const = 0;
};

class CombineRGBANode : public CombineColorNode {
 public:
  CombineRGBANode(bNode *editor_node) : CombineColorNode(editor_node)
  {
  }

  NodeOperation *get_color_converter(const CompositorContext &context) const override;
};

class CombineHSVANode : public CombineColorNode {
 public:
  CombineHSVANode(bNode *editor_node) : CombineColorNode(editor_node)
  {
  }

  NodeOperation *get_color_converter(const CompositorContext &context) const override;
};

class CombineYCCANode : public CombineColorNode {
 public:
  CombineYCCANode(bNode *editor_node) : CombineColorNode(editor_node)
  {
  }

  NodeOperation *get_color_converter(const CompositorContext &context) const override;
};

class CombineYUVANode : public CombineColorNode {
 public:
  CombineYUVANode(bNode *editor_node) : CombineColorNode(editor_node)
  {
  }

  NodeOperation *get_color_converter(const CompositorContext &context) const override;
};

}  // namespace blender::compositor
