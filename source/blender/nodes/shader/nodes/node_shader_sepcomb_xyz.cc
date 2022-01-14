/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 */

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

class MF_SeparateXYZ : public blender::fn::MultiFunction {
 public:
  MF_SeparateXYZ()
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Separate XYZ"};
    signature.single_input<blender::float3>("XYZ");
    signature.single_output<float>("X");
    signature.single_output<float>("Y");
    signature.single_output<float>("Z");
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<blender::float3> &vectors =
        params.readonly_single_input<blender::float3>(0, "XYZ");
    blender::MutableSpan<float> xs = params.uninitialized_single_output<float>(1, "X");
    blender::MutableSpan<float> ys = params.uninitialized_single_output<float>(2, "Y");
    blender::MutableSpan<float> zs = params.uninitialized_single_output<float>(3, "Z");

    for (int64_t i : mask) {
      blender::float3 xyz = vectors[i];
      xs[i] = xyz.x;
      ys[i] = xyz.y;
      zs[i] = xyz.z;
    }
  }
};

static void sh_node_sepxyz_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
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

static void sh_node_combxyz_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static blender::fn::CustomMF_SI_SI_SI_SO<float, float, float, blender::float3> fn{
      "Combine Vector", [](float x, float y, float z) { return blender::float3(x, y, z); }};
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
