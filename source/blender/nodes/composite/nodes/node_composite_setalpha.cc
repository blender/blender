/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "node_composite_util.hh"

/* **************** SET ALPHA ******************** */

namespace blender::nodes::node_composite_setalpha_cc {

NODE_STORAGE_FUNCS(NodeSetAlpha)

static void cmp_node_setalpha_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Alpha")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_setalpha(bNodeTree * /*ntree*/, bNode *node)
{
  NodeSetAlpha *settings = MEM_callocN<NodeSetAlpha>(__func__);
  node->storage = settings;
  settings->mode = CMP_NODE_SETALPHA_MODE_APPLY;
}

static void node_composit_buts_set_alpha(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "mode", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

static CMPNodeSetAlphaMode get_mode(const bNode &node)
{
  return static_cast<CMPNodeSetAlphaMode>(node_storage(node).mode);
}

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  switch (get_mode(*node)) {
    case CMP_NODE_SETALPHA_MODE_APPLY:
      return GPU_stack_link(material, node, "node_composite_set_alpha_apply", inputs, outputs);
    case CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA:
      return GPU_stack_link(material, node, "node_composite_set_alpha_replace", inputs, outputs);
  }

  return false;
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto apply_function = mf::build::SI2_SO<float4, float, float4>(
      "Set Alpha Apply",
      [](const float4 &color, const float alpha) -> float4 { return color * alpha; },
      mf::build::exec_presets::AllSpanOrSingle());

  static auto replace_function = mf::build::SI2_SO<float4, float, float4>(
      "Set Alpha Replace",
      [](const float4 &color, const float alpha) -> float4 { return float4(color.xyz(), alpha); },
      mf::build::exec_presets::AllSpanOrSingle());

  switch (get_mode(builder.node())) {
    case CMP_NODE_SETALPHA_MODE_APPLY:
      builder.set_matching_fn(apply_function);
      break;
    case CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA:
      builder.set_matching_fn(replace_function);
      break;
  }
}

}  // namespace blender::nodes::node_composite_setalpha_cc

static void register_node_type_cmp_setalpha()
{
  namespace file_ns = blender::nodes::node_composite_setalpha_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeSetAlpha", CMP_NODE_SETALPHA);
  ntype.ui_name = "Set Alpha";
  ntype.ui_description = "Add an alpha channel to an image";
  ntype.enum_name_legacy = "SETALPHA";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_setalpha_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_set_alpha;
  ntype.initfunc = file_ns::node_composit_init_setalpha;
  blender::bke::node_type_storage(
      ntype, "NodeSetAlpha", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_setalpha)
