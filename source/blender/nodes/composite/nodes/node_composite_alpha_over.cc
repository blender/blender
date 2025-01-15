/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** ALPHAOVER ******************** */

namespace blender::nodes::node_composite_alpha_over_cc {

NODE_STORAGE_FUNCS(NodeTwoFloats)

static void cmp_node_alphaover_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(2);
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Color>("Image", "Image_001")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
}

static void node_alphaover_init(bNodeTree * /*ntree*/, bNode *node)
{
  node->storage = MEM_cnew<NodeTwoFloats>(__func__);
}

static void node_composit_buts_alphaover(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_premultiply", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemR(col, ptr, "premul", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

static bool get_use_premultiply(const bNode &node)
{
  return node.custom1;
}

static float get_premultiply_factor(const bNode &node)
{
  return node_storage(node).x;
}

class AlphaOverShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float premultiply_factor = get_premultiply_factor(bnode());
    if (premultiply_factor != 0.0f) {
      GPU_stack_link(material,
                     &bnode(),
                     "node_composite_alpha_over_mixed",
                     inputs,
                     outputs,
                     GPU_uniform(&premultiply_factor));
      return;
    }

    if (get_use_premultiply(bnode())) {
      GPU_stack_link(material, &bnode(), "node_composite_alpha_over_key", inputs, outputs);
      return;
    }

    GPU_stack_link(material, &bnode(), "node_composite_alpha_over_premultiply", inputs, outputs);
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new AlphaOverShaderNode(node);
}

static float4 alpha_over_mixed(const float factor,
                               const float4 &color,
                               const float4 &over_color,
                               const float premultiply_factor)
{
  if (over_color.w <= 0.0f) {
    return color;
  }

  if (factor == 1.0f && over_color.w >= 1.0f) {
    return over_color;
  }

  float add_factor = 1.0f - premultiply_factor + over_color.w * premultiply_factor;
  float premultiplier = factor * add_factor;
  float multiplier = 1.0f - factor * over_color.w;

  return multiplier * color + float4(float3(premultiplier), factor) * over_color;
}

static float4 alpha_over_key(const float factor, const float4 &color, const float4 &over_color)
{
  if (over_color.w <= 0.0f) {
    return color;
  }

  if (factor == 1.0f && over_color.w >= 1.0f) {
    return over_color;
  }

  return math::interpolate(color, float4(over_color.xyz(), 1.0f), factor * over_color.w);
}

static float4 alpha_over_premultiply(const float factor,
                                     const float4 &color,
                                     const float4 &over_color)
{
  if (over_color.w < 0.0f) {
    return color;
  }

  if (factor == 1.0f && over_color.w >= 1.0f) {
    return over_color;
  }

  float multiplier = 1.0f - factor * over_color.w;
  return multiplier * color + factor * over_color;
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto key_function = mf::build::SI3_SO<float, float4, float4, float4>(
      "Alpha Over Key",
      [=](const float factor, const float4 &color, const float4 &over_color) -> float4 {
        return alpha_over_key(factor, color, over_color);
      },
      mf::build::exec_presets::SomeSpanOrSingle<1, 2>());

  static auto premultiply_function = mf::build::SI3_SO<float, float4, float4, float4>(
      "Alpha Over Premultiply",
      [=](const float factor, const float4 &color, const float4 &over_color) -> float4 {
        return alpha_over_premultiply(factor, color, over_color);
      },
      mf::build::exec_presets::SomeSpanOrSingle<1, 2>());

  const float premultiply_factor = get_premultiply_factor(builder.node());
  if (premultiply_factor != 0.0f) {
    builder.construct_and_set_matching_fn_cb([=]() {
      return mf::build::SI3_SO<float, float4, float4, float4>(
          "Alpha Over Mixed",
          [=](const float factor, const float4 &color, const float4 &over_color) -> float4 {
            return alpha_over_mixed(factor, color, over_color, premultiply_factor);
          },
          mf::build::exec_presets::SomeSpanOrSingle<1, 2>());
    });
  }
  else if (get_use_premultiply(builder.node())) {
    builder.set_matching_fn(key_function);
  }
  else {
    builder.set_matching_fn(premultiply_function);
  }
}

}  // namespace blender::nodes::node_composite_alpha_over_cc

void register_node_type_cmp_alphaover()
{
  namespace file_ns = blender::nodes::node_composite_alpha_over_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeAlphaOver", CMP_NODE_ALPHAOVER);
  ntype.ui_name = "Alpha Over";
  ntype.ui_description = "Overlay a foreground image onto a background image";
  ntype.enum_name_legacy = "ALPHAOVER";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::cmp_node_alphaover_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_alphaover;
  ntype.initfunc = file_ns::node_alphaover_init;
  blender::bke::node_type_storage(
      &ntype, "NodeTwoFloats", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
