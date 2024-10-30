/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_color.h"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

static void node_cmp_combsep_color_init(bNodeTree * /*ntree*/, bNode *node)
{
  NodeCMPCombSepColor *data = MEM_cnew<NodeCMPCombSepColor>(__func__);
  data->mode = CMP_NODE_COMBSEP_COLOR_RGB;
  data->ycc_mode = BLI_YCC_ITU_BT709;
  node->storage = data;
}

static void node_cmp_combsep_color_label(const ListBase *sockets, CMPNodeCombSepColorMode mode)
{
  bNodeSocket *sock1 = (bNodeSocket *)sockets->first;
  bNodeSocket *sock2 = sock1->next;
  bNodeSocket *sock3 = sock2->next;

  node_sock_label_clear(sock1);
  node_sock_label_clear(sock2);
  node_sock_label_clear(sock3);

  switch (mode) {
    case CMP_NODE_COMBSEP_COLOR_RGB:
      node_sock_label(sock1, "Red");
      node_sock_label(sock2, "Green");
      node_sock_label(sock3, "Blue");
      break;
    case CMP_NODE_COMBSEP_COLOR_HSV:
      node_sock_label(sock1, "Hue");
      node_sock_label(sock2, "Saturation");
      node_sock_label(sock3, "Value");
      break;
    case CMP_NODE_COMBSEP_COLOR_HSL:
      node_sock_label(sock1, "Hue");
      node_sock_label(sock2, "Saturation");
      node_sock_label(sock3, "Lightness");
      break;
    case CMP_NODE_COMBSEP_COLOR_YCC:
      node_sock_label(sock1, "Y");
      node_sock_label(sock2, "Cb");
      node_sock_label(sock3, "Cr");
      break;
    case CMP_NODE_COMBSEP_COLOR_YUV:
      node_sock_label(sock1, "Y");
      node_sock_label(sock2, "U");
      node_sock_label(sock3, "V");
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

/* **************** SEPARATE COLOR ******************** */

namespace blender::nodes::node_composite_separate_color_cc {

NODE_STORAGE_FUNCS(NodeCMPCombSepColor)

static void cmp_node_separate_color_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Float>("Red");
  b.add_output<decl::Float>("Green");
  b.add_output<decl::Float>("Blue");
  b.add_output<decl::Float>("Alpha");
}

static void cmp_node_separate_color_update(bNodeTree * /*ntree*/, bNode *node)
{
  const NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)node->storage;
  node_cmp_combsep_color_label(&node->outputs, (CMPNodeCombSepColorMode)storage->mode);
}

using namespace blender::realtime_compositor;

class SeparateColorShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material, &bnode(), get_shader_function_name(), inputs, outputs);
  }

  const char *get_shader_function_name()
  {
    switch (node_storage(bnode()).mode) {
      case CMP_NODE_COMBSEP_COLOR_RGB:
        return "node_composite_separate_rgba";
      case CMP_NODE_COMBSEP_COLOR_HSV:
        return "node_composite_separate_hsva";
      case CMP_NODE_COMBSEP_COLOR_HSL:
        return "node_composite_separate_hsla";
      case CMP_NODE_COMBSEP_COLOR_YUV:
        return "node_composite_separate_yuva_itu_709";
      case CMP_NODE_COMBSEP_COLOR_YCC:
        switch (node_storage(bnode()).ycc_mode) {
          case BLI_YCC_ITU_BT601:
            return "node_composite_separate_ycca_itu_601";
          case BLI_YCC_ITU_BT709:
            return "node_composite_separate_ycca_itu_709";
          case BLI_YCC_JFIF_0_255:
            return "node_composite_separate_ycca_jpeg";
        }
    }

    BLI_assert_unreachable();
    return nullptr;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new SeparateColorShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto rgba_function = mf::build::SI1_SO4<float4, float, float, float, float>(
      "Separate Color RGBA",
      [](const float4 &color, float &r, float &g, float &b, float &a) -> void {
        r = color.x;
        g = color.y;
        b = color.z;
        a = color.w;
      },
      mf::build::exec_presets::AllSpanOrSingle());

  static auto hsva_function = mf::build::SI1_SO4<float4, float, float, float, float>(
      "Separate Color HSVA",
      [](const float4 &color, float &h, float &s, float &v, float &a) -> void {
        rgb_to_hsv(color.x, color.y, color.z, &h, &s, &v);
        a = color.w;
      },
      mf::build::exec_presets::AllSpanOrSingle());

  static auto hsla_function = mf::build::SI1_SO4<float4, float, float, float, float>(
      "Separate Color HSLA",
      [](const float4 &color, float &h, float &s, float &l, float &a) -> void {
        rgb_to_hsl(color.x, color.y, color.z, &h, &s, &l);
        a = color.w;
      },
      mf::build::exec_presets::AllSpanOrSingle());

  static auto yuva_function = mf::build::SI1_SO4<float4, float, float, float, float>(
      "Separate Color YUVA",
      [](const float4 &color, float &y, float &u, float &v, float &a) -> void {
        rgb_to_yuv(color.x, color.y, color.z, &y, &u, &v, BLI_YUV_ITU_BT709);
        a = color.w;
      },
      mf::build::exec_presets::AllSpanOrSingle());

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

  switch (node_storage(builder.node()).mode) {
    case CMP_NODE_COMBSEP_COLOR_RGB:
      builder.set_matching_fn(rgba_function);
      break;
    case CMP_NODE_COMBSEP_COLOR_HSV:
      builder.set_matching_fn(hsva_function);
      break;
    case CMP_NODE_COMBSEP_COLOR_HSL:
      builder.set_matching_fn(hsla_function);
      break;
    case CMP_NODE_COMBSEP_COLOR_YUV:
      builder.set_matching_fn(yuva_function);
      break;
    case CMP_NODE_COMBSEP_COLOR_YCC:
      switch (node_storage(builder.node()).ycc_mode) {
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
}

}  // namespace blender::nodes::node_composite_separate_color_cc

void register_node_type_cmp_separate_color()
{
  namespace file_ns = blender::nodes::node_composite_separate_color_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SEPARATE_COLOR, "Separate Color", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_separate_color_declare;
  ntype.initfunc = node_cmp_combsep_color_init;
  blender::bke::node_type_storage(
      &ntype, "NodeCMPCombSepColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.updatefunc = file_ns::cmp_node_separate_color_update;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}

/* **************** COMBINE COLOR ******************** */

namespace blender::nodes::node_composite_combine_color_cc {

NODE_STORAGE_FUNCS(NodeCMPCombSepColor)

static void cmp_node_combine_color_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Red")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Green")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(1);
  b.add_input<decl::Float>("Blue")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(2);
  b.add_input<decl::Float>("Alpha")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(3);
  b.add_output<decl::Color>("Image");
}

static void cmp_node_combine_color_update(bNodeTree * /*ntree*/, bNode *node)
{
  const NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)node->storage;
  node_cmp_combsep_color_label(&node->inputs, (CMPNodeCombSepColorMode)storage->mode);
}

using namespace blender::realtime_compositor;

class CombineColorShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material, &bnode(), get_shader_function_name(), inputs, outputs);
  }

  const char *get_shader_function_name()
  {
    switch (node_storage(bnode()).mode) {
      case CMP_NODE_COMBSEP_COLOR_RGB:
        return "node_composite_combine_rgba";
      case CMP_NODE_COMBSEP_COLOR_HSV:
        return "node_composite_combine_hsva";
      case CMP_NODE_COMBSEP_COLOR_HSL:
        return "node_composite_combine_hsla";
      case CMP_NODE_COMBSEP_COLOR_YUV:
        return "node_composite_combine_yuva_itu_709";
      case CMP_NODE_COMBSEP_COLOR_YCC:
        switch (node_storage(bnode()).ycc_mode) {
          case BLI_YCC_ITU_BT601:
            return "node_composite_combine_ycca_itu_601";
          case BLI_YCC_ITU_BT709:
            return "node_composite_combine_ycca_itu_709";
          case BLI_YCC_JFIF_0_255:
            return "node_composite_combine_ycca_jpeg";
        }
    }

    BLI_assert_unreachable();
    return nullptr;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new CombineColorShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto rgba_function = mf::build::SI4_SO<float, float, float, float, float4>(
      "Combine Color RGBA",
      [](const float r, const float g, const float b, const float a) -> float4 {
        return float4(r, g, b, a);
      },
      mf::build::exec_presets::Materialized());

  static auto hsva_function = mf::build::SI4_SO<float, float, float, float, float4>(
      "Combine Color HSVA",
      [](const float h, const float s, const float v, const float a) -> float4 {
        float4 result;
        hsv_to_rgb(h, s, v, &result.x, &result.y, &result.z);
        result.w = a;
        return result;
      },
      mf::build::exec_presets::Materialized());

  static auto hsla_function = mf::build::SI4_SO<float, float, float, float, float4>(
      "Combine Color HSLA",
      [](const float h, const float s, const float l, const float a) -> float4 {
        float4 result;
        hsl_to_rgb(h, s, l, &result.x, &result.y, &result.z);
        result.w = a;
        return result;
      },
      mf::build::exec_presets::Materialized());

  static auto yuva_function = mf::build::SI4_SO<float, float, float, float, float4>(
      "Combine Color YUVA",
      [](const float y, const float u, const float v, const float a) -> float4 {
        float4 result;
        yuv_to_rgb(y, u, v, &result.x, &result.y, &result.z, BLI_YUV_ITU_BT709);
        result.w = a;
        return result;
      },
      mf::build::exec_presets::Materialized());

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

  switch (node_storage(builder.node()).mode) {
    case CMP_NODE_COMBSEP_COLOR_RGB:
      builder.set_matching_fn(rgba_function);
      break;
    case CMP_NODE_COMBSEP_COLOR_HSV:
      builder.set_matching_fn(hsva_function);
      break;
    case CMP_NODE_COMBSEP_COLOR_HSL:
      builder.set_matching_fn(hsla_function);
      break;
    case CMP_NODE_COMBSEP_COLOR_YUV:
      builder.set_matching_fn(yuva_function);
      break;
    case CMP_NODE_COMBSEP_COLOR_YCC:
      switch (node_storage(builder.node()).ycc_mode) {
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
}

}  // namespace blender::nodes::node_composite_combine_color_cc

void register_node_type_cmp_combine_color()
{
  namespace file_ns = blender::nodes::node_composite_combine_color_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_COMBINE_COLOR, "Combine Color", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_combine_color_declare;
  ntype.initfunc = node_cmp_combsep_color_init;
  blender::bke::node_type_storage(
      &ntype, "NodeCMPCombSepColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.updatefunc = file_ns::cmp_node_combine_color_update;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
