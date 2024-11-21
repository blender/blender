/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** SET ALPHA ******************** */

namespace blender::nodes::node_composite_setalpha_cc {

NODE_STORAGE_FUNCS(NodeSetAlpha)

static void cmp_node_setalpha_declare(NodeDeclarationBuilder &b)
{
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
  NodeSetAlpha *settings = MEM_cnew<NodeSetAlpha>(__func__);
  node->storage = settings;
  settings->mode = CMP_NODE_SETALPHA_MODE_APPLY;
}

static void node_composit_buts_set_alpha(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

static CMPNodeSetAlphaMode get_mode(const bNode &node)
{
  return static_cast<CMPNodeSetAlphaMode>(node_storage(node).mode);
}

class SetAlphaShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    if (get_mode(bnode()) == CMP_NODE_SETALPHA_MODE_APPLY) {
      GPU_stack_link(material, &bnode(), "node_composite_set_alpha_apply", inputs, outputs);
      return;
    }

    GPU_stack_link(material, &bnode(), "node_composite_set_alpha_replace", inputs, outputs);
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new SetAlphaShaderNode(node);
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

void register_node_type_cmp_setalpha()
{
  namespace file_ns = blender::nodes::node_composite_setalpha_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SETALPHA, "Set Alpha", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_setalpha_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_set_alpha;
  ntype.initfunc = file_ns::node_composit_init_setalpha;
  blender::bke::node_type_storage(
      &ntype, "NodeSetAlpha", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
