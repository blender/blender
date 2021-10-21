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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

#include "../node_shader_util.h"

#include "BLI_noise.hh"

namespace blender::nodes {

static void sh_node_tex_white_noise_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").min(-10000.0f).max(10000.0f).implicit_field();
  b.add_input<decl::Float>("W").min(-10000.0f).max(10000.0f);
  b.add_output<decl::Float>("Value");
  b.add_output<decl::Color>("Color");
};

}  // namespace blender::nodes

static void node_shader_init_tex_white_noise(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = 3;
}

static const char *gpu_shader_get_name(const int dimensions)
{
  BLI_assert(dimensions >= 1 && dimensions <= 4);
  return std::array{"node_white_noise_1d",
                    "node_white_noise_2d",
                    "node_white_noise_3d",
                    "node_white_noise_4d"}[dimensions - 1];
}

static int gpu_shader_tex_white_noise(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData *UNUSED(execdata),
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  const char *name = gpu_shader_get_name(node->custom1);
  return GPU_stack_link(mat, node, name, in, out);
}

static void node_shader_update_tex_white_noise(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sockVector = nodeFindSocket(node, SOCK_IN, "Vector");
  bNodeSocket *sockW = nodeFindSocket(node, SOCK_IN, "W");

  nodeSetSocketAvailability(sockVector, node->custom1 != 1);
  nodeSetSocketAvailability(sockW, node->custom1 == 1 || node->custom1 == 4);
}

namespace blender::nodes {

class WhiteNoiseFunction : public fn::MultiFunction {
 private:
  int dimensions_;

 public:
  WhiteNoiseFunction(int dimensions) : dimensions_(dimensions)
  {
    BLI_assert(dimensions >= 1 && dimensions <= 4);
    static std::array<fn::MFSignature, 4> signatures{
        create_signature(1),
        create_signature(2),
        create_signature(3),
        create_signature(4),
    };
    this->set_signature(&signatures[dimensions - 1]);
  }

  static fn::MFSignature create_signature(int dimensions)
  {
    fn::MFSignatureBuilder signature{"WhiteNoise"};

    if (ELEM(dimensions, 2, 3, 4)) {
      signature.single_input<float3>("Vector");
    }
    if (ELEM(dimensions, 1, 4)) {
      signature.single_input<float>("W");
    }

    signature.single_output<float>("Value");
    signature.single_output<ColorGeometry4f>("Color");

    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    int param = ELEM(dimensions_, 2, 3, 4) + ELEM(dimensions_, 1, 4);

    MutableSpan<float> r_value = params.uninitialized_single_output_if_required<float>(param++,
                                                                                       "Value");
    MutableSpan<ColorGeometry4f> r_color =
        params.uninitialized_single_output_if_required<ColorGeometry4f>(param++, "Color");

    const bool compute_value = !r_value.is_empty();
    const bool compute_color = !r_color.is_empty();

    switch (dimensions_) {
      case 1: {
        const VArray<float> &w = params.readonly_single_input<float>(0, "W");
        if (compute_color) {
          for (int64_t i : mask) {
            const float3 c = noise::hash_float_to_float3(w[i]);
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          }
        }
        if (compute_value) {
          for (int64_t i : mask) {
            r_value[i] = noise::hash_float_to_float(w[i]);
          }
        }
        break;
      }
      case 2: {
        const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
        if (compute_color) {
          for (int64_t i : mask) {
            const float3 c = noise::hash_float_to_float3(float2(vector[i].x, vector[i].y));
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          }
        }
        if (compute_value) {
          for (int64_t i : mask) {
            r_value[i] = noise::hash_float_to_float(float2(vector[i].x, vector[i].y));
          }
        }
        break;
      }
      case 3: {
        const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
        if (compute_color) {
          for (int64_t i : mask) {
            const float3 c = noise::hash_float_to_float3(vector[i]);
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          }
        }
        if (compute_value) {
          for (int64_t i : mask) {
            r_value[i] = noise::hash_float_to_float(vector[i]);
          }
        }
        break;
      }
      case 4: {
        const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
        const VArray<float> &w = params.readonly_single_input<float>(1, "W");
        if (compute_color) {
          for (int64_t i : mask) {
            const float3 c = noise::hash_float_to_float3(
                float4(vector[i].x, vector[i].y, vector[i].z, w[i]));
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          }
        }
        if (compute_value) {
          for (int64_t i : mask) {
            r_value[i] = noise::hash_float_to_float(
                float4(vector[i].x, vector[i].y, vector[i].z, w[i]));
          }
        }
        break;
      }
    }
  }
};

static void sh_node_noise_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  bNode &node = builder.node();
  builder.construct_and_set_matching_fn<WhiteNoiseFunction>((int)node.custom1);
}

}  // namespace blender::nodes

void register_node_type_sh_tex_white_noise(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(
      &ntype, SH_NODE_TEX_WHITE_NOISE, "White Noise Texture", NODE_CLASS_TEXTURE, 0);
  ntype.declare = blender::nodes::sh_node_tex_white_noise_declare;
  node_type_init(&ntype, node_shader_init_tex_white_noise);
  node_type_gpu(&ntype, gpu_shader_tex_white_noise);
  node_type_update(&ntype, node_shader_update_tex_white_noise);
  ntype.build_multi_function = blender::nodes::sh_node_noise_build_multi_function;

  nodeRegisterType(&ntype);
}
