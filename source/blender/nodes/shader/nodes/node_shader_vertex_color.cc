/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BKE_context.hh"

#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_vertex_color_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Color");
  b.add_output<decl::Float>("Alpha");
}

static void node_shader_buts_vertex_color(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  PointerRNA obptr = CTX_data_pointer_get(C, "active_object");
  Object *object = static_cast<Object *>(obptr.data);

  if (object && object->type == OB_MESH) {
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

    if (depsgraph) {
      Object *object_eval = DEG_get_evaluated(depsgraph, object);
      PointerRNA dataptr = RNA_id_pointer_create(static_cast<ID *>(object_eval->data));
      layout->prop_search(ptr, "layer_name", &dataptr, "color_attributes", "", ICON_GROUP_VCOL);
      return;
    }
  }

  layout->prop(ptr, "layer_name", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_GROUP_VCOL);
  layout->label(RPT_("No mesh in active object"), ICON_ERROR);
}

static void node_shader_init_vertex_color(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderVertexColor *vertexColor = MEM_callocN<NodeShaderVertexColor>("NodeShaderVertexColor");
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

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* TODO: some output expected be implemented within the next iteration
   * (see node-definition `<geomcolor>`). */
  return get_output_default(socket_out_->identifier, NodeItem::Type::Any);
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_vertex_color_cc

void register_node_type_sh_vertex_color()
{
  namespace file_ns = blender::nodes::node_shader_vertex_color_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeVertexColor", SH_NODE_VERTEX_COLOR);
  ntype.ui_name = "Color Attribute";
  ntype.ui_description =
      "Retrieve a color attribute, or the default fallback if none is specified";
  ntype.enum_name_legacy = "VERTEX_COLOR";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_vertex_color;
  ntype.initfunc = file_ns::node_shader_init_vertex_color;
  blender::bke::node_type_storage(
      ntype, "NodeShaderVertexColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_vertex_color;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
