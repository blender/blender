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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

#include "node_shader_util.hh"

#include "BKE_context.h"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_vertex_color_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>(N_("Color"));
  b.add_output<decl::Float>(N_("Alpha"));
}

static void node_shader_buts_vertex_color(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  PointerRNA obptr = CTX_data_pointer_get(C, "active_object");
  if (obptr.data && RNA_enum_get(&obptr, "type") == OB_MESH) {
    PointerRNA dataptr = RNA_pointer_get(&obptr, "data");

    if (U.experimental.use_sculpt_vertex_colors &&
        RNA_collection_length(&dataptr, "sculpt_vertex_colors")) {
      uiItemPointerR(
          layout, ptr, "layer_name", &dataptr, "sculpt_vertex_colors", "", ICON_GROUP_VCOL);
    }
    else {
      uiItemPointerR(layout, ptr, "layer_name", &dataptr, "vertex_colors", "", ICON_GROUP_VCOL);
    }
  }
  else {
    uiItemL(layout, TIP_("No mesh in active object"), ICON_ERROR);
  }
}

static void node_shader_init_vertex_color(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeShaderVertexColor *vertexColor = MEM_cnew<NodeShaderVertexColor>("NodeShaderVertexColor");
  node->storage = vertexColor;
}

static int node_shader_gpu_vertex_color(GPUMaterial *mat,
                                        bNode *node,
                                        bNodeExecData *UNUSED(execdata),
                                        GPUNodeStack *in,
                                        GPUNodeStack *out)
{
  NodeShaderVertexColor *vertexColor = (NodeShaderVertexColor *)node->storage;
  if (U.experimental.use_sculpt_vertex_colors) {
    GPUNodeLink *vertexColorLink = GPU_attribute(mat, CD_PROP_COLOR, vertexColor->layer_name);
    return GPU_stack_link(mat, node, "node_vertex_color", in, out, vertexColorLink);
  }
  GPUNodeLink *vertexColorLink = GPU_attribute(mat, CD_MCOL, vertexColor->layer_name);
  return GPU_stack_link(mat, node, "node_vertex_color", in, out, vertexColorLink);
}

}  // namespace blender::nodes::node_shader_vertex_color_cc

void register_node_type_sh_vertex_color()
{
  namespace file_ns = blender::nodes::node_shader_vertex_color_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_VERTEX_COLOR, "Vertex Color", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_vertex_color;
  node_type_init(&ntype, file_ns::node_shader_init_vertex_color);
  node_type_storage(
      &ntype, "NodeShaderVertexColor", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, file_ns::node_shader_gpu_vertex_color);

  nodeRegisterType(&ntype);
}
