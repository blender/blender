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
