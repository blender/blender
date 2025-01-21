/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"
#include "BLI_math_color.h"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** SEPARATE YCCA ******************** */

namespace blender::nodes::node_composite_separate_ycca_cc {

static void cmp_node_sepycca_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Float>("Y").translation_context(BLT_I18NCONTEXT_COLOR);
  b.add_output<decl::Float>("Cb");
  b.add_output<decl::Float>("Cr");
  b.add_output<decl::Float>("A").translation_context(BLT_I18NCONTEXT_COLOR);
}

static void node_composit_init_mode_sepycca(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = 1; /* BLI_YCC_ITU_BT709 */
}

using namespace blender::compositor;

class SeparateYCCAShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material, &bnode(), get_shader_function_name(), inputs, outputs);
  }

  int get_mode()
  {
    return bnode().custom1;
  }

  const char *get_shader_function_name()
  {
    switch (get_mode()) {
      case BLI_YCC_ITU_BT601:
        return "node_composite_separate_ycca_itu_601";
      case BLI_YCC_ITU_BT709:
        return "node_composite_separate_ycca_itu_709";
      case BLI_YCC_JFIF_0_255:
        return "node_composite_separate_ycca_jpeg";
    }

    BLI_assert_unreachable();
    return nullptr;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new SeparateYCCAShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto ycca_itu_601_function = mf::build::SI1_SO4<float4, float, float, float, float>(
      "Separate Color YCCA ITU 601",
      [](const float4 &color, float &y, float &cb, float &cr, float &a) -> void {
        rgb_to_ycc(color.x, color.y, color.z, &y, &cb, &cr, BLI_YCC_ITU_BT601);
        y /= 255.0f;
        cb /= 255.0f;
        cr /= 255.0f;
        a = color.w;
      },
      mf::build::exec_presets::AllSpanOrSingle());

  static auto ycca_itu_709_function = mf::build::SI1_SO4<float4, float, float, float, float>(
      "Separate Color YCCA ITU 709",
      [](const float4 &color, float &y, float &cb, float &cr, float &a) -> void {
        rgb_to_ycc(color.x, color.y, color.z, &y, &cb, &cr, BLI_YCC_ITU_BT709);
        y /= 255.0f;
        cb /= 255.0f;
        cr /= 255.0f;
        a = color.w;
      },
      mf::build::exec_presets::AllSpanOrSingle());

  static auto ycca_jpeg_function = mf::build::SI1_SO4<float4, float, float, float, float>(
      "Separate Color YCCA JPEG",
      [](const float4 &color, float &y, float &cb, float &cr, float &a) -> void {
        rgb_to_ycc(color.x, color.y, color.z, &y, &cb, &cr, BLI_YCC_JFIF_0_255);
        y /= 255.0f;
        cb /= 255.0f;
        cr /= 255.0f;
        a = color.w;
      },
      mf::build::exec_presets::AllSpanOrSingle());

  switch (builder.node().custom1) {
    case BLI_YCC_ITU_BT601:
      builder.set_matching_fn(ycca_itu_601_function);
      break;
    case BLI_YCC_ITU_BT709:
      builder.set_matching_fn(ycca_itu_709_function);
      break;
    case BLI_YCC_JFIF_0_255:
      builder.set_matching_fn(ycca_jpeg_function);
      break;
  }
}

}  // namespace blender::nodes::node_composite_separate_ycca_cc

void register_node_type_cmp_sepycca()
{
  namespace file_ns = blender::nodes::node_composite_separate_ycca_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeSepYCCA", CMP_NODE_SEPYCCA_LEGACY);
  ntype.ui_name = "Separate YCbCrA (Legacy)";
  ntype.ui_description = "Deprecated";
  ntype.enum_name_legacy = "SEPYCCA";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_sepycca_declare;
  ntype.initfunc = file_ns::node_composit_init_mode_sepycca;
  ntype.gather_link_search_ops = nullptr;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}

/* **************** COMBINE YCCA ******************** */

namespace blender::nodes::node_composite_combine_ycca_cc {

static void cmp_node_combycca_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Y")
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(0)
      .translation_context(BLT_I18NCONTEXT_COLOR);
  b.add_input<decl::Float>("Cb")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(1);
  b.add_input<decl::Float>("Cr")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(2);
  b.add_input<decl::Float>("A")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(3)
      .translation_context(BLT_I18NCONTEXT_COLOR);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_mode_combycca(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = 1; /* BLI_YCC_ITU_BT709 */
}

using namespace blender::compositor;

class CombineYCCAShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material, &bnode(), get_shader_function_name(), inputs, outputs);
  }

  int get_mode()
  {
    return bnode().custom1;
  }

  const char *get_shader_function_name()
  {
    switch (get_mode()) {
      case BLI_YCC_ITU_BT601:
        return "node_composite_combine_ycca_itu_601";
      case BLI_YCC_ITU_BT709:
        return "node_composite_combine_ycca_itu_709";
      case BLI_YCC_JFIF_0_255:
        return "node_composite_combine_ycca_jpeg";
    }

    BLI_assert_unreachable();
    return nullptr;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new CombineYCCAShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto ycca_itu_601_function = mf::build::SI4_SO<float, float, float, float, float4>(
      "Combine Color YCCA ITU 601",
      [](const float y, const float cb, const float cr, const float a) -> float4 {
        float4 result;
        ycc_to_rgb(y * 255.0f,
                   cb * 255.0f,
                   cr * 255.0f,
                   &result.x,
                   &result.y,
                   &result.z,
                   BLI_YCC_ITU_BT601);
        result.w = a;
        return result;
      },
      mf::build::exec_presets::Materialized());

  static auto ycca_itu_709_function = mf::build::SI4_SO<float, float, float, float, float4>(
      "Combine Color YCCA ITU 709",
      [](const float y, const float cb, const float cr, const float a) -> float4 {
        float4 result;
        ycc_to_rgb(y * 255.0f,
                   cb * 255.0f,
                   cr * 255.0f,
                   &result.x,
                   &result.y,
                   &result.z,
                   BLI_YCC_ITU_BT709);
        result.w = a;
        return result;
      },
      mf::build::exec_presets::Materialized());

  static auto ycca_jpeg_function = mf::build::SI4_SO<float, float, float, float, float4>(
      "Combine Color YCCA JPEG",
      [](const float y, const float cb, const float cr, const float a) -> float4 {
        float4 result;
        ycc_to_rgb(y * 255.0f,
                   cb * 255.0f,
                   cr * 255.0f,
                   &result.x,
                   &result.y,
                   &result.z,
                   BLI_YCC_JFIF_0_255);
        result.w = a;
        return result;
      },
      mf::build::exec_presets::Materialized());

  switch (builder.node().custom1) {
    case BLI_YCC_ITU_BT601:
      builder.set_matching_fn(ycca_itu_601_function);
      break;
    case BLI_YCC_ITU_BT709:
      builder.set_matching_fn(ycca_itu_709_function);
      break;
    case BLI_YCC_JFIF_0_255:
      builder.set_matching_fn(ycca_jpeg_function);
      break;
  }
}

}  // namespace blender::nodes::node_composite_combine_ycca_cc

void register_node_type_cmp_combycca()
{
  namespace file_ns = blender::nodes::node_composite_combine_ycca_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeCombYCCA", CMP_NODE_COMBYCCA_LEGACY);
  ntype.ui_name = "Combine YCbCrA (Legacy)";
  ntype.ui_description = "Deprecated";
  ntype.enum_name_legacy = "COMBYCCA";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_combycca_declare;
  ntype.initfunc = file_ns::node_composit_init_mode_combycca;
  ntype.gather_link_search_ops = nullptr;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
