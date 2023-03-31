/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_tex_checker_cc {

static void sh_node_tex_checker_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>(N_("Vector"))
      .min(-10000.0f)
      .max(10000.0f)
      .implicit_field(implicit_field_inputs::position);
  b.add_input<decl::Color>(N_("Color1")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Color>(N_("Color2")).default_value({0.2f, 0.2f, 0.2f, 1.0f});
  b.add_input<decl::Float>(N_("Scale"))
      .min(-10000.0f)
      .max(10000.0f)
      .default_value(5.0f)
      .no_muted_links();
  b.add_output<decl::Color>(N_("Color"));
  b.add_output<decl::Float>(N_("Fac"));
}

static void node_shader_init_tex_checker(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexChecker *tex = MEM_cnew<NodeTexChecker>(__func__);
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);

  node->storage = tex;
}

static int node_shader_gpu_tex_checker(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData * /*execdata*/,
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  return GPU_stack_link(mat, node, "node_tex_checker", in, out);
}

class NodeTexChecker : public mf::MultiFunction {
 public:
  NodeTexChecker()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Checker", signature};
      builder.single_input<float3>("Vector");
      builder.single_input<ColorGeometry4f>("Color1");
      builder.single_input<ColorGeometry4f>("Color2");
      builder.single_input<float>("Scale");
      builder.single_output<ColorGeometry4f>("Color", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float>("Fac");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(IndexMask mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
    const VArray<ColorGeometry4f> &color1 = params.readonly_single_input<ColorGeometry4f>(
        1, "Color1");
    const VArray<ColorGeometry4f> &color2 = params.readonly_single_input<ColorGeometry4f>(
        2, "Color2");
    const VArray<float> &scale = params.readonly_single_input<float>(3, "Scale");
    MutableSpan<ColorGeometry4f> r_color =
        params.uninitialized_single_output_if_required<ColorGeometry4f>(4, "Color");
    MutableSpan<float> r_fac = params.uninitialized_single_output<float>(5, "Fac");

    for (int64_t i : mask) {
      /* Avoid precision issues on unit coordinates. */
      const float3 p = (vector[i] * scale[i] + 0.000001f) * 0.999999f;

      const int xi = abs(int(floorf(p.x)));
      const int yi = abs(int(floorf(p.y)));
      const int zi = abs(int(floorf(p.z)));

      r_fac[i] = ((xi % 2 == yi % 2) == (zi % 2)) ? 1.0f : 0.0f;
    }

    if (!r_color.is_empty()) {
      for (int64_t i : mask) {
        r_color[i] = (r_fac[i] == 1.0f) ? color1[i] : color2[i];
      }
    }
  }
};

static void sh_node_tex_checker_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static NodeTexChecker fn;
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes::node_shader_tex_checker_cc

void register_node_type_sh_tex_checker()
{
  namespace file_ns = blender::nodes::node_shader_tex_checker_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_TEX_CHECKER, "Checker Texture", NODE_CLASS_TEXTURE);
  ntype.declare = file_ns::sh_node_tex_checker_declare;
  ntype.initfunc = file_ns::node_shader_init_tex_checker;
  node_type_storage(
      &ntype, "NodeTexChecker", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_checker;
  ntype.build_multi_function = file_ns::sh_node_tex_checker_build_multi_function;

  nodeRegisterType(&ntype);
}
