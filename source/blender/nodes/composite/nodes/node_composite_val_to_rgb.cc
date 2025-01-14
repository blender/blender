/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "IMB_colormanagement.hh"

#include "BKE_colorband.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** VALTORGB ******************** */

namespace blender::nodes::node_composite_color_ramp_cc {

static void cmp_node_valtorgb_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Fac")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image").compositor_domain_priority(0);
  b.add_output<decl::Float>("Alpha");
}

static void node_composit_init_valtorgb(bNodeTree * /*ntree*/, bNode *node)
{
  node->storage = BKE_colorband_add(true);
}

using namespace blender::compositor;

static ColorBand *get_color_band(const bNode &node)
{
  return static_cast<ColorBand *>(node.storage);
}

class ColorRampShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    ColorBand *color_band = get_color_band(bnode());

    /* Common / easy case optimization. */
    if ((color_band->tot <= 2) && (color_band->color_mode == COLBAND_BLEND_RGB)) {
      float mul_bias[2];
      switch (color_band->ipotype) {
        case COLBAND_INTERP_LINEAR:
          mul_bias[0] = 1.0f / (color_band->data[1].pos - color_band->data[0].pos);
          mul_bias[1] = -mul_bias[0] * color_band->data[0].pos;
          GPU_stack_link(material,
                         &bnode(),
                         "valtorgb_opti_linear",
                         inputs,
                         outputs,
                         GPU_uniform(mul_bias),
                         GPU_uniform(&color_band->data[0].r),
                         GPU_uniform(&color_band->data[1].r));
          return;
        case COLBAND_INTERP_CONSTANT:
          mul_bias[1] = max_ff(color_band->data[0].pos, color_band->data[1].pos);
          GPU_stack_link(material,
                         &bnode(),
                         "valtorgb_opti_constant",
                         inputs,
                         outputs,
                         GPU_uniform(&mul_bias[1]),
                         GPU_uniform(&color_band->data[0].r),
                         GPU_uniform(&color_band->data[1].r));
          return;
        case COLBAND_INTERP_EASE:
          mul_bias[0] = 1.0f / (color_band->data[1].pos - color_band->data[0].pos);
          mul_bias[1] = -mul_bias[0] * color_band->data[0].pos;
          GPU_stack_link(material,
                         &bnode(),
                         "valtorgb_opti_ease",
                         inputs,
                         outputs,
                         GPU_uniform(mul_bias),
                         GPU_uniform(&color_band->data[0].r),
                         GPU_uniform(&color_band->data[1].r));
          return;
        case COLBAND_INTERP_B_SPLINE:
        case COLBAND_INTERP_CARDINAL:
          /* Not optimized yet. Fallback to gradient texture. */
          break;
        default:
          BLI_assert_unreachable();
          return;
      }
    }

    float *array, layer;
    int size;
    BKE_colorband_evaluate_table_rgba(color_band, &array, &size);
    GPUNodeLink *tex = GPU_color_band(material, size, array, &layer);

    if (color_band->ipotype == COLBAND_INTERP_CONSTANT) {
      GPU_stack_link(
          material, &bnode(), "valtorgb_nearest", inputs, outputs, tex, GPU_constant(&layer));
      return;
    }

    GPU_stack_link(material, &bnode(), "valtorgb", inputs, outputs, tex, GPU_constant(&layer));
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new ColorRampShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  ColorBand *color_band = get_color_band(builder.node());
  builder.construct_and_set_matching_fn_cb([=]() {
    return mf::build::SI1_SO2<float, float4, float>(
        "Color Ramp",
        [=](const float factor, float4 &color, float &alpha) -> void {
          BKE_colorband_evaluate(color_band, factor, color);
          alpha = color.w;
        },
        mf::build::exec_presets::AllSpanOrSingle());
  });
}

}  // namespace blender::nodes::node_composite_color_ramp_cc

void register_node_type_cmp_valtorgb()
{
  namespace file_ns = blender::nodes::node_composite_color_ramp_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeValToRGB", CMP_NODE_VALTORGB);
  ntype.ui_name = "Color Ramp";
  ntype.ui_description = "Map values to colors with the use of a gradient";
  ntype.enum_name_legacy = "VALTORGB";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_valtorgb_declare;
  blender::bke::node_type_size(&ntype, 240, 200, 320);
  ntype.initfunc = file_ns::node_composit_init_valtorgb;
  blender::bke::node_type_storage(
      &ntype, "ColorBand", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}

/* **************** RGBTOBW ******************** */

namespace blender::nodes::node_composite_rgb_to_bw_cc {

static void cmp_node_rgbtobw_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Float>("Val");
}

using namespace blender::compositor;

class RGBToBWShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    float luminance_coefficients[3];
    IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);

    GPU_stack_link(material,
                   &bnode(),
                   "color_to_luminance",
                   inputs,
                   outputs,
                   GPU_constant(luminance_coefficients));
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new RGBToBWShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  float3 luminance_coefficients;
  IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);

  builder.construct_and_set_matching_fn_cb([=]() {
    return mf::build::SI1_SO<float4, float>(
        "RGB to BW",
        [=](const float4 &color) -> float {
          return math::dot(color.xyz(), luminance_coefficients);
        },
        mf::build::exec_presets::AllSpanOrSingle());
  });
}

}  // namespace blender::nodes::node_composite_rgb_to_bw_cc

void register_node_type_cmp_rgbtobw()
{
  namespace file_ns = blender::nodes::node_composite_rgb_to_bw_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeRGBToBW", CMP_NODE_RGBTOBW);
  ntype.ui_name = "RGB to BW";
  ntype.ui_description = "Convert RGB input into grayscale using luminance";
  ntype.enum_name_legacy = "RGBTOBW";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_rgbtobw_declare;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::Default);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
