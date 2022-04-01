/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_sepcomb_rgb_cc {

static void sh_node_seprgb_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>(N_("Image")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_output<decl::Float>(N_("R"));
  b.add_output<decl::Float>(N_("G"));
  b.add_output<decl::Float>(N_("B"));
}

static int gpu_shader_seprgb(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData *UNUSED(execdata),
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "separate_rgb", in, out);
}

class SeparateRGBFunction : public fn::MultiFunction {
 public:
  SeparateRGBFunction()
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"Separate RGB"};
    signature.single_input<ColorGeometry4f>("Color");
    signature.single_output<float>("R");
    signature.single_output<float>("G");
    signature.single_output<float>("B");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<ColorGeometry4f> &colors = params.readonly_single_input<ColorGeometry4f>(0,
                                                                                          "Color");
    MutableSpan<float> rs = params.uninitialized_single_output<float>(1, "R");
    MutableSpan<float> gs = params.uninitialized_single_output<float>(2, "G");
    MutableSpan<float> bs = params.uninitialized_single_output<float>(3, "B");

    for (int64_t i : mask) {
      ColorGeometry4f color = colors[i];
      rs[i] = color.r;
      gs[i] = color.g;
      bs[i] = color.b;
    }
  }
};

static void sh_node_seprgb_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static SeparateRGBFunction fn;
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes::node_shader_sepcomb_rgb_cc

void register_node_type_sh_seprgb()
{
  namespace file_ns = blender::nodes::node_shader_sepcomb_rgb_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_SEPRGB, "Separate RGB", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::sh_node_seprgb_declare;
  node_type_gpu(&ntype, file_ns::gpu_shader_seprgb);
  ntype.build_multi_function = file_ns::sh_node_seprgb_build_multi_function;

  nodeRegisterType(&ntype);
}

namespace blender::nodes::node_shader_sepcomb_rgb_cc {

static void sh_node_combrgb_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>(N_("R")).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("G")).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("B")).min(0.0f).max(1.0f);
  b.add_output<decl::Color>(N_("Image"));
}

static int gpu_shader_combrgb(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData *UNUSED(execdata),
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "combine_rgb", in, out);
}

static void sh_node_combrgb_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static fn::CustomMF_SI_SI_SI_SO<float, float, float, ColorGeometry4f> fn{
      "Combine RGB", [](float r, float g, float b) { return ColorGeometry4f(r, g, b, 1.0f); }};
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes::node_shader_sepcomb_rgb_cc

void register_node_type_sh_combrgb()
{
  namespace file_ns = blender::nodes::node_shader_sepcomb_rgb_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_COMBRGB, "Combine RGB", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::sh_node_combrgb_declare;
  node_type_gpu(&ntype, file_ns::gpu_shader_combrgb);
  ntype.build_multi_function = file_ns::sh_node_combrgb_build_multi_function;

  nodeRegisterType(&ntype);
}
