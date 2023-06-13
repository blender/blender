/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BKE_context.h"

#include "DEG_depsgraph_query.h"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_vertex_color_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Color");
  b.add_output<decl::Float>("Alpha");
}

static void node_shader_buts_vertex_color(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  PointerRNA obptr = CTX_data_pointer_get(C, "active_object");
  if (obptr.data && RNA_enum_get(&obptr, "type") == OB_MESH) {
    PointerRNA eval_obptr;

    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    DEG_get_evaluated_rna_pointer(depsgraph, &obptr, &eval_obptr);
    PointerRNA dataptr = RNA_pointer_get(&eval_obptr, "data");
    uiItemPointerR(layout, ptr, "layer_name", &dataptr, "color_attributes", "", ICON_GROUP_VCOL);
  }
  else {
    uiItemL(layout, TIP_("No mesh in active object"), ICON_ERROR);
  }
}

static void node_shader_init_vertex_color(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderVertexColor *vertexColor = MEM_cnew<NodeShaderVertexColor>("NodeShaderVertexColor");
  node->storage = vertexColor;
}

static int node_shader_gpu_vertex_color(GPUMaterial *mat,
                                        bNode *node,
                                        bNodeExecData * /*execdata*/,
                                        GPUNodeStack *in,
                                        GPUNodeStack *out)
{
  NodeShaderVertexColor *vertexColor = (NodeShaderVertexColor *)node->storage;
  /* NOTE: Using #CD_AUTO_FROM_NAME is necessary because there are multiple color attribute types,
   * and the type may change during evaluation anyway. This will also make EEVEE and Cycles
   * consistent. See #93179. */

  GPUNodeLink *vertexColorLink;

  if (vertexColor->layer_name[0]) {
    vertexColorLink = GPU_attribute(mat, CD_AUTO_FROM_NAME, vertexColor->layer_name);
  }
  else { /* Fall back on active render color attribute. */
    vertexColorLink = GPU_attribute_default_color(mat);
  }

  return GPU_stack_link(mat, node, "node_vertex_color", in, out, vertexColorLink);
}

}  // namespace blender::nodes::node_shader_vertex_color_cc

void register_node_type_sh_vertex_color()
{
  namespace file_ns = blender::nodes::node_shader_vertex_color_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_VERTEX_COLOR, "Color Attribute", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_vertex_color;
  ntype.initfunc = file_ns::node_shader_init_vertex_color;
  node_type_storage(
      &ntype, "NodeShaderVertexColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_vertex_color;

  nodeRegisterType(&ntype);
}
