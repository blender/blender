/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2014 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_sepcomb_xyz_cc {

static void sh_node_sepxyz_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>(N_("Vector")).min(-10000.0f).max(10000.0f);
  b.add_output<decl::Float>(N_("X"));
  b.add_output<decl::Float>(N_("Y"));
  b.add_output<decl::Float>(N_("Z"));
}

static int gpu_shader_sepxyz(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData *UNUSED(execdata),
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "separate_xyz", in, out);
}

class MF_SeparateXYZ : public fn::MultiFunction {
 public:
  MF_SeparateXYZ()
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"Separate XYZ"};
    signature.single_input<float3>("XYZ");
    signature.single_output<float>("X");
    signature.single_output<float>("Y");
    signature.single_output<float>("Z");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<float3> &vectors = params.readonly_single_input<float3>(0, "XYZ");
    MutableSpan<float> xs = params.uninitialized_single_output<float>(1, "X");
    MutableSpan<float> ys = params.uninitialized_single_output<float>(2, "Y");
    MutableSpan<float> zs = params.uninitialized_single_output<float>(3, "Z");

    for (int64_t i : mask) {
      float3 xyz = vectors[i];
      xs[i] = xyz.x;
      ys[i] = xyz.y;
      zs[i] = xyz.z;
    }
  }
};

static void sh_node_sepxyz_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static MF_SeparateXYZ separate_fn;
  builder.set_matching_fn(separate_fn);
}

}  // namespace blender::nodes::node_shader_sepcomb_xyz_cc

void register_node_type_sh_sepxyz()
{
  namespace file_ns = blender::nodes::node_shader_sepcomb_xyz_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_SEPXYZ, "Separate XYZ", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::sh_node_sepxyz_declare;
  node_type_gpu(&ntype, file_ns::gpu_shader_sepxyz);
  ntype.build_multi_function = file_ns::sh_node_sepxyz_build_multi_function;

  nodeRegisterType(&ntype);
}

namespace blender::nodes::node_shader_sepcomb_xyz_cc {

static void sh_node_combxyz_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>(N_("X")).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>(N_("Y")).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>(N_("Z")).min(-10000.0f).max(10000.0f);
  b.add_output<decl::Vector>(N_("Vector"));
}

static int gpu_shader_combxyz(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData *UNUSED(execdata),
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "combine_xyz", in, out);
}

static void sh_node_combxyz_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static fn::CustomMF_SI_SI_SI_SO<float, float, float, float3> fn{
      "Combine Vector", [](float x, float y, float z) { return float3(x, y, z); }};
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes::node_shader_sepcomb_xyz_cc

void register_node_type_sh_combxyz()
{
  namespace file_ns = blender::nodes::node_shader_sepcomb_xyz_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_COMBXYZ, "Combine XYZ", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::sh_node_combxyz_declare;
  node_type_gpu(&ntype, file_ns::gpu_shader_combxyz);
  ntype.build_multi_function = file_ns::sh_node_combxyz_build_multi_function;

  nodeRegisterType(&ntype);
}
