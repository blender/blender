/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_color.h"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** SEPARATE YUVA ******************** */

namespace blender::nodes::node_composite_separate_yuva_cc {

static void cmp_node_sepyuva_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Float>("Y").translation_context(BLT_I18NCONTEXT_COLOR);
  b.add_output<decl::Float>("U").translation_context(BLT_I18NCONTEXT_COLOR);
  b.add_output<decl::Float>("V").translation_context(BLT_I18NCONTEXT_COLOR);
  b.add_output<decl::Float>("A").translation_context(BLT_I18NCONTEXT_COLOR);
}

using namespace blender::compositor;

class SeparateYUVAShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material, &bnode(), "node_composite_separate_yuva_itu_709", inputs, outputs);
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new SeparateYUVAShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI1_SO4<float4, float, float, float, float>(
      "Separate Color YUVA",
      [](const float4 &color, float &y, float &u, float &v, float &a) -> void {
        rgb_to_yuv(color.x, color.y, color.z, &y, &u, &v, BLI_YUV_ITU_BT709);
        a = color.w;
      },
      mf::build::exec_presets::AllSpanOrSingle());
  builder.set_matching_fn(function);
}

}  // namespace blender::nodes::node_composite_separate_yuva_cc

void register_node_type_cmp_sepyuva()
{
  namespace file_ns = blender::nodes::node_composite_separate_yuva_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeSepYUVA", CMP_NODE_SEPYUVA_LEGACY);
  ntype.ui_name = "Separate YUVA (Legacy)";
  ntype.ui_description = "Deprecated";
  ntype.enum_name_legacy = "SEPYUVA";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_sepyuva_declare;
  ntype.gather_link_search_ops = nullptr;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}

/* **************** COMBINE YUVA ******************** */

namespace blender::nodes::node_composite_combine_yuva_cc {

static void cmp_node_combyuva_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Y")
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(0)
      .translation_context(BLT_I18NCONTEXT_COLOR);
  b.add_input<decl::Float>("U")
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(1)
      .translation_context(BLT_I18NCONTEXT_COLOR);
  b.add_input<decl::Float>("V")
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(2)
      .translation_context(BLT_I18NCONTEXT_COLOR);
  b.add_input<decl::Float>("A")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(3)
      .translation_context(BLT_I18NCONTEXT_COLOR);
  b.add_output<decl::Color>("Image");
}

using namespace blender::compositor;

class CombineYUVAShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material, &bnode(), "node_composite_combine_yuva_itu_709", inputs, outputs);
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new CombineYUVAShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI4_SO<float, float, float, float, float4>(
      "Combine Color YUVA",
      [](const float y, const float u, const float v, const float a) -> float4 {
        float4 result;
        yuv_to_rgb(y, u, v, &result.x, &result.y, &result.z, BLI_YUV_ITU_BT709);
        result.w = a;
        return result;
      },
      mf::build::exec_presets::Materialized());
  builder.set_matching_fn(function);
}

}  // namespace blender::nodes::node_composite_combine_yuva_cc

void register_node_type_cmp_combyuva()
{
  namespace file_ns = blender::nodes::node_composite_combine_yuva_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeCombYUVA", CMP_NODE_COMBYUVA_LEGACY);
  ntype.ui_name = "Combine YUVA (Legacy)";
  ntype.ui_description = "Deprecated";
  ntype.enum_name_legacy = "COMBYUVA";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_combyuva_declare;
  ntype.gather_link_search_ops = nullptr;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
