/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"
#include "node_shader_util.hh"

#include "NOD_geometry_nodes_gizmos.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_fn_input_vector_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  int dimensions = 3;
  if (const bNode *node = b.node_or_null()) {
    const auto &storage = *static_cast<NodeInputVector *>(node->storage);
    dimensions = storage.dimensions;
  }
  b.add_output<decl::Vector>("Vector")
      .dimensions(dimensions)
      .custom_draw([](CustomSocketDrawParams &params) {
        params.layout.alignment_set(ui::LayoutAlign::Expand);
        ui::Layout &row = params.layout.row(true);
        row.column(true).prop(
            &params.node_ptr, "vector", ui::ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
        if (gizmos::value_node_has_gizmo(params.tree, params.node)) {
          row.prop(&params.socket_ptr, "pin_gizmo", UI_ITEM_NONE, "", ICON_GIZMO);
        }
      });
}

static int gpu_shader_vector(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack * /*in*/,
                             GPUNodeStack *out)
{
  NodeInputVector *node_storage = static_cast<NodeInputVector *>(node->storage);
  return GPU_link(mat, "set_rgb", GPU_uniform(node_storage->vector), &out->link);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem vector = get_output_default("Vector", NodeItem::Type::Vector3);
  return create_node("constant", NodeItem::Type::Vector3, {{"value", vector}});
}
#endif
NODE_SHADER_MATERIALX_END

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &bnode = builder.node();
  NodeInputVector *node_storage = static_cast<NodeInputVector *>(bnode.storage);
  switch (node_storage->dimensions) {
    case 2: {
      float2 vector(node_storage->vector);
      builder.construct_and_set_matching_fn<mf::CustomMF_Constant<float2>>(vector);
      break;
    }
    case 3: {
      float3 vector(node_storage->vector);
      builder.construct_and_set_matching_fn<mf::CustomMF_Constant<float3>>(vector);
      break;
    }
    case 4: {
      float4 vector(node_storage->vector);
      builder.construct_and_set_matching_fn<mf::CustomMF_Constant<float4>>(vector);
      break;
    }
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeInputVector *data = MEM_new<NodeInputVector>(__func__);
  node->storage = data;
}

static void node_layout_ex(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.use_property_split_set(true);
  layout.use_property_decorate_set(false);
  layout.prop(ptr, "vector_dimensions", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void node_register()
{
  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "FunctionNodeInputVector", FN_NODE_INPUT_VECTOR);
  ntype.ui_name = "Vector";
  ntype.ui_description = "Provide a vector value that can be connected to other nodes in the tree";
  ntype.enum_name_legacy = "INPUT_VECTOR";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.gpu_fn = gpu_shader_vector;
  ntype.draw_buttons_ex = node_layout_ex;
  bke::node_type_storage(
      ntype, "NodeInputVector", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = node_build_multi_function;
  ntype.materialx_fn = node_shader_materialx;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_input_vector_cc
