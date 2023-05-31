/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
