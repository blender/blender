/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "NOD_multi_function.hh"

#include "FN_multi_function_builder.hh"

#include "GPU_material.hh"

#include "node_composite_util.hh"

/* **************** Gamma Tools  ******************** */

namespace blender::nodes::node_composite_gamma_cc {

static void cmp_node_gamma_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Gamma")
      .default_value(1.0f)
      .min(0.001f)
      .max(10.0f)
      .subtype(PROP_UNSIGNED)
      .compositor_domain_priority(1)
      .description(
          "Gamma correction value, applied as color^gamma.\n"
          "Typically used to convert from gamma encoded to linear color space, or in the reverse "
          "direction with 1/gamma");
  b.add_output<decl::Color>("Image");
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  return GPU_stack_link(material, node, "node_composite_gamma", inputs, outputs);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto gamma_function = mf::build::SI2_SO<float4, float, float4>(
      "Gamma",
      [](const float4 &color, const float gamma) -> float4 {
        return float4(math::safe_pow(color.xyz(), gamma), color.w);
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());
  builder.set_matching_fn(gamma_function);
}

}  // namespace blender::nodes::node_composite_gamma_cc

static void register_node_type_cmp_gamma()
{
  namespace file_ns = blender::nodes::node_composite_gamma_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeGamma", CMP_NODE_GAMMA);
  ntype.ui_name = "Gamma";
  ntype.ui_description = "Apply gamma correction";
  ntype.enum_name_legacy = "GAMMA";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::cmp_node_gamma_declare;
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_gamma)
