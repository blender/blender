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
#include "COM_RenderLayersProg.h"
#include "DNA_node_types.h"

struct Render;
namespace blender::compositor {

/**
 * \brief RenderLayersNode
 * \ingroup Node
 */
class RenderLayersNode : public Node {
 public:
  RenderLayersNode(bNode *editor_node);
  void convert_to_operations(NodeConverter &converter,
                             const CompositorContext &context) const override;

 private:
  void test_socket_link(NodeConverter &converter,
                        const CompositorContext &context,
                        NodeOutput *output,
                        RenderLayersProg *operation,
                        Scene *scene,
                        int layer_id,
                        bool is_preview) const;
  void test_render_link(NodeConverter &converter,
                        const CompositorContext &context,
                        Render *re) const;

  void missing_socket_link(NodeConverter &converter, NodeOutput *output) const;
  void missing_render_link(NodeConverter &converter) const;
};

}  // namespace blender::compositor
