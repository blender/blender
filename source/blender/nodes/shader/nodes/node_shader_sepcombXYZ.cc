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

#include "node_shader_util.h"

/* **************** SEPARATE XYZ ******************** */
static bNodeSocketTemplate sh_node_sepxyz_in[] = {
    {SOCK_VECTOR, N_("Vector"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f},
    {-1, ""},
};
static bNodeSocketTemplate sh_node_sepxyz_out[] = {
    {SOCK_FLOAT, N_("X")},
    {SOCK_FLOAT, N_("Y")},
    {SOCK_FLOAT, N_("Z")},
    {-1, ""},
};

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

static void sh_node_sepxyz_expand_in_mf_network(blender::nodes::NodeMFNetworkBuilder &builder)
{
  static MF_SeparateXYZ separate_fn;
  builder.set_matching_fn(separate_fn);
}

void register_node_type_sh_sepxyz(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_SEPXYZ, "Separate XYZ", NODE_CLASS_CONVERTOR, 0);
  node_type_socket_templates(&ntype, sh_node_sepxyz_in, sh_node_sepxyz_out);
  node_type_gpu(&ntype, gpu_shader_sepxyz);
  ntype.expand_in_mf_network = sh_node_sepxyz_expand_in_mf_network;

  nodeRegisterType(&ntype);
}

/* **************** COMBINE XYZ ******************** */
static bNodeSocketTemplate sh_node_combxyz_in[] = {
    {SOCK_FLOAT, N_("X"), 0.0f, 0.0f, 0.0f, 1.0f, -10000.0f, 10000.0f},
    {SOCK_FLOAT, N_("Y"), 0.0f, 0.0f, 0.0f, 1.0f, -10000.0f, 10000.0f},
    {SOCK_FLOAT, N_("Z"), 0.0f, 0.0f, 0.0f, 1.0f, -10000.0f, 10000.0f},
    {-1, ""},
};
static bNodeSocketTemplate sh_node_combxyz_out[] = {
    {SOCK_VECTOR, N_("Vector")},
    {-1, ""},
};

static int gpu_shader_combxyz(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData *UNUSED(execdata),
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "combine_xyz", in, out);
}

static void sh_node_combxyz_expand_in_mf_network(blender::nodes::NodeMFNetworkBuilder &builder)
{
  static blender::fn::CustomMF_SI_SI_SI_SO<float, float, float, blender::float3> fn{
      "Combine Vector", [](float x, float y, float z) { return blender::float3(x, y, z); }};
  builder.set_matching_fn(fn);
}

void register_node_type_sh_combxyz(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_COMBXYZ, "Combine XYZ", NODE_CLASS_CONVERTOR, 0);
  node_type_socket_templates(&ntype, sh_node_combxyz_in, sh_node_combxyz_out);
  node_type_gpu(&ntype, gpu_shader_combxyz);
  ntype.expand_in_mf_network = sh_node_combxyz_expand_in_mf_network;

  nodeRegisterType(&ntype);
}
