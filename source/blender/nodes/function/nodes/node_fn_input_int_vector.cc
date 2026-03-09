/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"
#include "node_shader_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_fn_input_int_vector_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  int dimensions = 3;
  if (const bNode *node = b.node_or_null()) {
    const auto &storage = *static_cast<NodeInputIntVector *>(node->storage);
    dimensions = storage.dimensions;
  }
  b.add_output<decl::IntVector>("Vector")
      .dimensions(dimensions)
      .custom_draw([](CustomSocketDrawParams &params) {
        params.layout.alignment_set(ui::LayoutAlign::Expand);
        ui::Layout &row = params.layout.row(true);
        row.column(true).prop(
            &params.node_ptr, "vector", ui::ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
      });
}

static int node_gpu(GPUMaterial *mat,
                    bNode *node,
                    bNodeExecData * /*execdata*/,
                    GPUNodeStack * /*in*/,
                    GPUNodeStack *out)
{
  NodeInputIntVector *node_storage = static_cast<NodeInputIntVector *>(node->storage);
  /* Passed as float3 for now since GPU material graphs do not support integer vectors. */
  const float3 vector = float3(int3(node_storage->vector));
  return GPU_link(mat, "set_rgb", GPU_uniform(vector), &out->link);
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &bnode = builder.node();
  NodeInputIntVector *node_storage = static_cast<NodeInputIntVector *>(bnode.storage);
  switch (node_storage->dimensions) {
    case 2: {
      int2 vector(node_storage->vector);
      builder.construct_and_set_matching_fn<mf::CustomMF_Constant<int2>>(vector);
      break;
    }
    case 3: {
      int3 vector(node_storage->vector);
      builder.construct_and_set_matching_fn<mf::CustomMF_Constant<int3>>(vector);
      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeInputIntVector *data = MEM_new<NodeInputIntVector>(__func__);
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

  fn_cmp_node_type_base(&ntype, "FunctionNodeInputIntVector");
  ntype.ui_name = "Integer Vector";
  ntype.ui_description =
      "Provide an integer vector value that can be connected to other nodes in the tree";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.gpu_fn = node_gpu;
  ntype.draw_buttons_ex = node_layout_ex;
  bke::node_type_storage(
      ntype, "NodeInputIntVector", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = node_build_multi_function;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_input_int_vector_cc
