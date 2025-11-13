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

#include "IMB_colormanagement.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_rgb_to_bw_cc {

static void cmp_node_rgbtobw_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_output<decl::Float>("Val");
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  float luminance_coefficients[3];
  IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);

  return GPU_stack_link(
      material, node, "color_to_luminance", inputs, outputs, GPU_constant(luminance_coefficients));
}

using blender::compositor::Color;

static float color_to_luminance(const float4 &color, const float3 &luminance_coefficients)
{
  return math::dot(color.xyz(), luminance_coefficients);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  float3 luminance_coefficients;
  IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);

  builder.construct_and_set_matching_fn_cb([=]() {
    return mf::build::SI1_SO<Color, float>(
        "RGB to BW",
        [=](const Color &color) -> float {
          return color_to_luminance(float4(color), luminance_coefficients);
        },
        mf::build::exec_presets::AllSpanOrSingle());
  });
}

}  // namespace blender::nodes::node_composite_rgb_to_bw_cc

static void register_node_type_cmp_rgbtobw()
{
  namespace file_ns = blender::nodes::node_composite_rgb_to_bw_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeRGBToBW", CMP_NODE_RGBTOBW);
  ntype.ui_name = "RGB to BW";
  ntype.ui_description = "Convert RGB input into grayscale using luminance";
  ntype.enum_name_legacy = "RGBTOBW";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_rgbtobw_declare;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Default);
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_rgbtobw)
