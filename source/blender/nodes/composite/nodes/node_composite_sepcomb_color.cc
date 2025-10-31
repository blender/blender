/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_color.h"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

static void node_cmp_combsep_color_init(bNodeTree * /*ntree*/, bNode *node)
{
  NodeCMPCombSepColor *data = MEM_callocN<NodeCMPCombSepColor>(__func__);
  data->mode = CMP_NODE_COMBSEP_COLOR_RGB;
  data->ycc_mode = BLI_YCC_ITU_BT709;
  node->storage = data;
}

/* **************** SEPARATE COLOR ******************** */

namespace blender::nodes::node_composite_separate_color_cc {

NODE_STORAGE_FUNCS(NodeCMPCombSepColor)

static void cmp_node_separate_color_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Float>("Red").label_fn([](bNode node) {
    switch (node_storage(node).mode) {
      case CMP_NODE_COMBSEP_COLOR_RGB:
      default:
        return IFACE_("Red");
      case CMP_NODE_COMBSEP_COLOR_HSV:
      case CMP_NODE_COMBSEP_COLOR_HSL:
        return IFACE_("Hue");
      case CMP_NODE_COMBSEP_COLOR_YCC:
      case CMP_NODE_COMBSEP_COLOR_YUV:
        return IFACE_("Y");
    }
  });
  b.add_output<decl::Float>("Green").label_fn([](bNode node) {
    switch (node_storage(node).mode) {
      case CMP_NODE_COMBSEP_COLOR_RGB:
      default:
        return IFACE_("Green");
      case CMP_NODE_COMBSEP_COLOR_HSV:
      case CMP_NODE_COMBSEP_COLOR_HSL:
        return IFACE_("Saturation");
      case CMP_NODE_COMBSEP_COLOR_YCC:
        return IFACE_("Cb");
      case CMP_NODE_COMBSEP_COLOR_YUV:
        return IFACE_("U");
    }
  });
  b.add_output<decl::Float>("Blue").label_fn([](bNode node) {
    switch (node_storage(node).mode) {
      case CMP_NODE_COMBSEP_COLOR_RGB:
      default:
        return IFACE_("Blue");
      case CMP_NODE_COMBSEP_COLOR_HSV:
        return CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "Value");
      case CMP_NODE_COMBSEP_COLOR_HSL:
        return IFACE_("Lightness");
      case CMP_NODE_COMBSEP_COLOR_YCC:
        return IFACE_("Cr");
      case CMP_NODE_COMBSEP_COLOR_YUV:
        return IFACE_("V");
    }
  });
  b.add_output<decl::Float>("Alpha");
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  switch (node_storage(*node).mode) {
    case CMP_NODE_COMBSEP_COLOR_RGB:
      return GPU_stack_link(material, node, "node_composite_separate_rgba", inputs, outputs);
    case CMP_NODE_COMBSEP_COLOR_HSV:
      return GPU_stack_link(material, node, "node_composite_separate_hsva", inputs, outputs);
    case CMP_NODE_COMBSEP_COLOR_HSL:
      return GPU_stack_link(material, node, "node_composite_separate_hsla", inputs, outputs);
    case CMP_NODE_COMBSEP_COLOR_YUV:
      return GPU_stack_link(
          material, node, "node_composite_separate_yuva_itu_709", inputs, outputs);
    case CMP_NODE_COMBSEP_COLOR_YCC:
      switch (node_storage(*node).ycc_mode) {
        case BLI_YCC_ITU_BT601:
          return GPU_stack_link(
              material, node, "node_composite_separate_ycca_itu_601", inputs, outputs);
        case BLI_YCC_ITU_BT709:
          return GPU_stack_link(
              material, node, "node_composite_separate_ycca_itu_709", inputs, outputs);
        case BLI_YCC_JFIF_0_255:
          return GPU_stack_link(
              material, node, "node_composite_separate_ycca_jpeg", inputs, outputs);
      }
  }

  return false;
}

using blender::compositor::Color;

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto rgba_function = mf::build::SI1_SO4<Color, float, float, float, float>(
      "Separate Color RGBA",
      [](const Color &color, float &r, float &g, float &b, float &a) -> void {
        r = color.r;
        g = color.g;
        b = color.b;
        a = color.a;
      },
      mf::build::exec_presets::AllSpanOrSingle());

  static auto hsva_function = mf::build::SI1_SO4<Color, float, float, float, float>(
      "Separate Color HSVA",
      [](const Color &color, float &h, float &s, float &v, float &a) -> void {
        rgb_to_hsv(color.r, color.g, color.b, &h, &s, &v);
        a = color.a;
      },
      mf::build::exec_presets::AllSpanOrSingle());

  static auto hsla_function = mf::build::SI1_SO4<Color, float, float, float, float>(
      "Separate Color HSLA",
      [](const Color &color, float &h, float &s, float &l, float &a) -> void {
        rgb_to_hsl(color.r, color.g, color.b, &h, &s, &l);
        a = color.a;
      },
      mf::build::exec_presets::AllSpanOrSingle());

  static auto yuva_function = mf::build::SI1_SO4<Color, float, float, float, float>(
      "Separate Color YUVA",
      [](const Color &color, float &y, float &u, float &v, float &a) -> void {
        rgb_to_yuv(color.r, color.g, color.b, &y, &u, &v, BLI_YUV_ITU_BT709);
        a = color.a;
      },
      mf::build::exec_presets::AllSpanOrSingle());

  static auto ycca_itu_601_function = mf::build::SI1_SO4<Color, float, float, float, float>(
      "Separate Color YCCA ITU 601",
      [](const Color &color, float &y, float &cb, float &cr, float &a) -> void {
        rgb_to_ycc(color.r, color.g, color.b, &y, &cb, &cr, BLI_YCC_ITU_BT601);
        y /= 255.0f;
        cb /= 255.0f;
        cr /= 255.0f;
        a = color.a;
      },
      mf::build::exec_presets::AllSpanOrSingle());

  static auto ycca_itu_709_function = mf::build::SI1_SO4<Color, float, float, float, float>(
      "Separate Color YCCA ITU 709",
      [](const Color &color, float &y, float &cb, float &cr, float &a) -> void {
        rgb_to_ycc(color.r, color.g, color.b, &y, &cb, &cr, BLI_YCC_ITU_BT709);
        y /= 255.0f;
        cb /= 255.0f;
        cr /= 255.0f;
        a = color.a;
      },
      mf::build::exec_presets::AllSpanOrSingle());

  static auto ycca_jpeg_function = mf::build::SI1_SO4<Color, float, float, float, float>(
      "Separate Color YCCA JPEG",
      [](const Color &color, float &y, float &cb, float &cr, float &a) -> void {
        rgb_to_ycc(color.r, color.g, color.b, &y, &cb, &cr, BLI_YCC_JFIF_0_255);
        y /= 255.0f;
        cb /= 255.0f;
        cr /= 255.0f;
        a = color.a;
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

static void register_node_type_cmp_separate_color()
{
  namespace file_ns = blender::nodes::node_composite_separate_color_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeSeparateColor", CMP_NODE_SEPARATE_COLOR);
  ntype.ui_name = "Separate Color";
  ntype.ui_description = "Split an image into its composite color channels";
  ntype.enum_name_legacy = "SEPARATE_COLOR";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_separate_color_declare;
  ntype.initfunc = node_cmp_combsep_color_init;
  blender::bke::node_type_storage(
      ntype, "NodeCMPCombSepColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_separate_color)

/* **************** COMBINE COLOR ******************** */

namespace blender::nodes::node_composite_combine_color_cc {

NODE_STORAGE_FUNCS(NodeCMPCombSepColor)

static void cmp_node_combine_color_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("Red")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .label_fn([](bNode node) {
        switch (node_storage(node).mode) {
          case CMP_NODE_COMBSEP_COLOR_RGB:
          default:
            return IFACE_("Red");
          case CMP_NODE_COMBSEP_COLOR_HSV:
          case CMP_NODE_COMBSEP_COLOR_HSL:
            return IFACE_("Hue");
          case CMP_NODE_COMBSEP_COLOR_YCC:
          case CMP_NODE_COMBSEP_COLOR_YUV:
            return IFACE_("Y");
        }
      });
  b.add_input<decl::Float>("Green")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .label_fn([](bNode node) {
        switch (node_storage(node).mode) {
          case CMP_NODE_COMBSEP_COLOR_RGB:
          default:
            return IFACE_("Green");
          case CMP_NODE_COMBSEP_COLOR_HSV:
          case CMP_NODE_COMBSEP_COLOR_HSL:
            return IFACE_("Saturation");
          case CMP_NODE_COMBSEP_COLOR_YCC:
            return IFACE_("Cb");
          case CMP_NODE_COMBSEP_COLOR_YUV:
            return IFACE_("U");
        }
      });
  b.add_input<decl::Float>("Blue")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .label_fn([](bNode node) {
        switch (node_storage(node).mode) {
          case CMP_NODE_COMBSEP_COLOR_RGB:
          default:
            return IFACE_("Blue");
          case CMP_NODE_COMBSEP_COLOR_HSV:
            return CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "Value");
          case CMP_NODE_COMBSEP_COLOR_HSL:
            return IFACE_("Lightness");
          case CMP_NODE_COMBSEP_COLOR_YCC:
            return IFACE_("Cr");
          case CMP_NODE_COMBSEP_COLOR_YUV:
            return IFACE_("V");
        }
      });
  b.add_input<decl::Float>("Alpha").default_value(1.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_output<decl::Color>("Image");
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  switch (node_storage(*node).mode) {
    case CMP_NODE_COMBSEP_COLOR_RGB:
      return GPU_stack_link(material, node, "node_composite_combine_rgba", inputs, outputs);
    case CMP_NODE_COMBSEP_COLOR_HSV:
      return GPU_stack_link(material, node, "node_composite_combine_hsva", inputs, outputs);
    case CMP_NODE_COMBSEP_COLOR_HSL:
      return GPU_stack_link(material, node, "node_composite_combine_hsla", inputs, outputs);
    case CMP_NODE_COMBSEP_COLOR_YUV:
      return GPU_stack_link(
          material, node, "node_composite_combine_yuva_itu_709", inputs, outputs);
    case CMP_NODE_COMBSEP_COLOR_YCC:
      switch (node_storage(*node).ycc_mode) {
        case BLI_YCC_ITU_BT601:
          return GPU_stack_link(
              material, node, "node_composite_combine_ycca_itu_601", inputs, outputs);
        case BLI_YCC_ITU_BT709:
          return GPU_stack_link(
              material, node, "node_composite_combine_ycca_itu_709", inputs, outputs);
        case BLI_YCC_JFIF_0_255:
          return GPU_stack_link(
              material, node, "node_composite_combine_ycca_jpeg", inputs, outputs);
      }
  }

  return false;
}

using blender::compositor::Color;

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto rgba_function = mf::build::SI4_SO<float, float, float, float, Color>(
      "Combine Color RGBA",
      [](const float r, const float g, const float b, const float a) -> Color {
        return Color(r, g, b, a);
      },
      mf::build::exec_presets::Materialized());

  static auto hsva_function = mf::build::SI4_SO<float, float, float, float, Color>(
      "Combine Color HSVA",
      [](const float h, const float s, const float v, const float a) -> Color {
        Color result;
        hsv_to_rgb(h, s, v, &result.r, &result.g, &result.b);
        result.a = a;
        return result;
      },
      mf::build::exec_presets::Materialized());

  static auto hsla_function = mf::build::SI4_SO<float, float, float, float, Color>(
      "Combine Color HSLA",
      [](const float h, const float s, const float l, const float a) -> Color {
        Color result;
        hsl_to_rgb(h, s, l, &result.r, &result.g, &result.b);
        result.a = a;
        return result;
      },
      mf::build::exec_presets::Materialized());

  static auto yuva_function = mf::build::SI4_SO<float, float, float, float, Color>(
      "Combine Color YUVA",
      [](const float y, const float u, const float v, const float a) -> Color {
        Color result;
        yuv_to_rgb(y, u, v, &result.r, &result.g, &result.b, BLI_YUV_ITU_BT709);
        result.a = a;
        return result;
      },
      mf::build::exec_presets::Materialized());

  static auto ycca_itu_601_function = mf::build::SI4_SO<float, float, float, float, Color>(
      "Combine Color YCCA ITU 601",
      [](const float y, const float cb, const float cr, const float a) -> Color {
        Color result;
        ycc_to_rgb(y * 255.0f,
                   cb * 255.0f,
                   cr * 255.0f,
                   &result.r,
                   &result.g,
                   &result.b,
                   BLI_YCC_ITU_BT601);
        result.a = a;
        return result;
      },
      mf::build::exec_presets::Materialized());

  static auto ycca_itu_709_function = mf::build::SI4_SO<float, float, float, float, Color>(
      "Combine Color YCCA ITU 709",
      [](const float y, const float cb, const float cr, const float a) -> Color {
        Color result;
        ycc_to_rgb(y * 255.0f,
                   cb * 255.0f,
                   cr * 255.0f,
                   &result.r,
                   &result.g,
                   &result.b,
                   BLI_YCC_ITU_BT709);
        result.a = a;
        return result;
      },
      mf::build::exec_presets::Materialized());

  static auto ycca_jpeg_function = mf::build::SI4_SO<float, float, float, float, Color>(
      "Combine Color YCCA JPEG",
      [](const float y, const float cb, const float cr, const float a) -> Color {
        Color result;
        ycc_to_rgb(y * 255.0f,
                   cb * 255.0f,
                   cr * 255.0f,
                   &result.r,
                   &result.g,
                   &result.b,
                   BLI_YCC_JFIF_0_255);
        result.a = a;
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

static void register_node_type_cmp_combine_color()
{
  namespace file_ns = blender::nodes::node_composite_combine_color_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeCombineColor", CMP_NODE_COMBINE_COLOR);
  ntype.ui_name = "Combine Color";
  ntype.ui_description = "Combine an image from its composite color channels";
  ntype.enum_name_legacy = "COMBINE_COLOR";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_combine_color_declare;
  ntype.initfunc = node_cmp_combsep_color_init;
  blender::bke::node_type_storage(
      ntype, "NodeCMPCombSepColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_combine_color)
