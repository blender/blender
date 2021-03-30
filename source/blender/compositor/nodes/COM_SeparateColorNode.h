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

class SeparateColorNode : public Node {
 public:
  SeparateColorNode(bNode *editorNode);
  void convertToOperations(NodeConverter &converter,
                           const CompositorContext &context) const override;

 protected:
  virtual NodeOperation *getColorConverter(const CompositorContext &context) const = 0;
};

class SeparateRGBANode : public SeparateColorNode {
 public:
  SeparateRGBANode(bNode *editorNode) : SeparateColorNode(editorNode)
  {
  }

  NodeOperation *getColorConverter(const CompositorContext &context) const override;
};

class SeparateHSVANode : public SeparateColorNode {
 public:
  SeparateHSVANode(bNode *editorNode) : SeparateColorNode(editorNode)
  {
  }

  NodeOperation *getColorConverter(const CompositorContext &context) const override;
};

class SeparateYCCANode : public SeparateColorNode {
 public:
  SeparateYCCANode(bNode *editorNode) : SeparateColorNode(editorNode)
  {
  }

  NodeOperation *getColorConverter(const CompositorContext &context) const override;
};

class SeparateYUVANode : public SeparateColorNode {
 public:
  SeparateYUVANode(bNode *editorNode) : SeparateColorNode(editorNode)
  {
  }

  NodeOperation *getColorConverter(const CompositorContext &context) const override;
};

}  // namespace blender::compositor
