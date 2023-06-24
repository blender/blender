/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_sepcomb_rgb_cc {

static void sh_node_seprgb_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_output<decl::Float>("R");
  b.add_output<decl::Float>("G");
  b.add_output<decl::Float>("B");
}

static int gpu_shader_seprgb(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "separate_rgb", in, out);
}

class SeparateRGBFunction : public mf::MultiFunction {
 public:
  SeparateRGBFunction()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Separate RGB", signature};
      builder.single_input<ColorGeometry4f>("Color");
      builder.single_output<float>("R");
      builder.single_output<float>("G");
      builder.single_output<float>("B");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<ColorGeometry4f> &colors = params.readonly_single_input<ColorGeometry4f>(0,
                                                                                          "Color");
    MutableSpan<float> rs = params.uninitialized_single_output<float>(1, "R");
    MutableSpan<float> gs = params.uninitialized_single_output<float>(2, "G");
    MutableSpan<float> bs = params.uninitialized_single_output<float>(3, "B");

    mask.foreach_index([&](const int64_t i) {
      ColorGeometry4f color = colors[i];
      rs[i] = color.r;
      gs[i] = color.g;
      bs[i] = color.b;
    });
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

  sh_fn_node_type_base(
      &ntype, SH_NODE_SEPRGB_LEGACY, "Separate RGB (Legacy)", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::sh_node_seprgb_declare;
  ntype.gpu_fn = file_ns::gpu_shader_seprgb;
  ntype.build_multi_function = file_ns::sh_node_seprgb_build_multi_function;
  ntype.gather_link_search_ops = nullptr;
  ntype.gather_add_node_search_ops = nullptr;

  nodeRegisterType(&ntype);
}

namespace blender::nodes::node_shader_sepcomb_rgb_cc {

static void sh_node_combrgb_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("R").min(0.0f).max(1.0f);
  b.add_input<decl::Float>("G").min(0.0f).max(1.0f);
  b.add_input<decl::Float>("B").min(0.0f).max(1.0f);
  b.add_output<decl::Color>("Image");
}

static int gpu_shader_combrgb(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData * /*execdata*/,
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "combine_rgb", in, out);
}

static void sh_node_combrgb_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI3_SO<float, float, float, ColorGeometry4f>(
      "Combine RGB", [](float r, float g, float b) { return ColorGeometry4f(r, g, b, 1.0f); });
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes::node_shader_sepcomb_rgb_cc

void register_node_type_sh_combrgb()
{
  namespace file_ns = blender::nodes::node_shader_sepcomb_rgb_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(
      &ntype, SH_NODE_COMBRGB_LEGACY, "Combine RGB (Legacy)", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::sh_node_combrgb_declare;
  ntype.gpu_fn = file_ns::gpu_shader_combrgb;
  ntype.build_multi_function = file_ns::sh_node_combrgb_build_multi_function;
  ntype.gather_link_search_ops = nullptr;
  ntype.gather_add_node_search_ops = nullptr;

  nodeRegisterType(&ntype);
}
