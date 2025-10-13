/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"
namespace blender::nodes::node_shader_gamma_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Color>("Color")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .description("Color input on which correction will be applied");
  b.add_output<decl::Color>("Color").align_with_previous();

  b.add_input<decl::Float>("Gamma")
      .default_value(1.0f)
      .min(0.001f)
      .max(10.0f)
      .subtype(PROP_NONE)
      .description(
          "Gamma correction value, applied as color^gamma.\n"
          "Gamma controls the relative intensity of the mid-tones compared to the full black and "
          "full white");
}

static int node_shader_gpu_gamma(GPUMaterial *mat,
                                 bNode *node,
                                 bNodeExecData * /*execdata*/,
                                 GPUNodeStack *in,
                                 GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_gamma", in, out);
}

using namespace blender::math;

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI2_SO<ColorGeometry4f, float, ColorGeometry4f>(
      "Gamma",
      [](const ColorGeometry4f &color, const float gamma) -> ColorGeometry4f {
        const float3 rgb = float3(color.r, color.g, color.b);
        const float3 rgb_gamma = math::safe_pow(rgb, gamma);
        return ColorGeometry4f(rgb_gamma.x, rgb_gamma.y, rgb_gamma.z, color.a);
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());
  builder.set_matching_fn(fn);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem color = get_input_value("Color", NodeItem::Type::Color3);
  NodeItem gamma = get_input_value("Gamma", NodeItem::Type::Float);
  return color ^ gamma;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_gamma_cc

void register_node_type_sh_gamma()
{
  namespace file_ns = blender::nodes::node_shader_gamma_cc;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeGamma", SH_NODE_GAMMA);
  ntype.ui_name = "Gamma";
  ntype.ui_description = "Apply a gamma correction";
  ntype.enum_name_legacy = "GAMMA";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_gamma;
  ntype.build_multi_function = file_ns::node_build_multi_function;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
