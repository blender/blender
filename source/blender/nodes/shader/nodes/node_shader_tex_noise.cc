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

static void sh_node_tex_noise_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>(N_("Vector")).implicit_field();
  b.add_input<decl::Float>(N_("W")).min(-1000.0f).max(1000.0f);
  b.add_input<decl::Float>(N_("Scale")).min(-1000.0f).max(1000.0f).default_value(5.0f);
  b.add_input<decl::Float>(N_("Detail")).min(0.0f).max(16.0f).default_value(2.0f);
  b.add_input<decl::Float>(N_("Roughness"))
      .min(0.0f)
      .max(1.0f)
      .default_value(0.5f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Distortion")).min(-1000.0f).max(1000.0f).default_value(0.0f);
  b.add_output<decl::Float>(N_("Fac")).no_muted_links();
  b.add_output<decl::Color>(N_("Color")).no_muted_links();
};

}  // namespace blender::nodes

static void node_shader_init_tex_noise(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexNoise *tex = (NodeTexNoise *)MEM_callocN(sizeof(NodeTexNoise), "NodeTexNoise");
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->dimensions = 3;

  node->storage = tex;
}

static const char *gpu_shader_get_name(const int dimensions)
{
  BLI_assert(dimensions >= 1 && dimensions <= 4);
  return std::array{"node_noise_texture_1d",
                    "node_noise_texture_2d",
                    "node_noise_texture_3d",
                    "node_noise_texture_4d"}[dimensions - 1];
  return nullptr;
}

static int node_shader_gpu_tex_noise(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  NodeTexNoise *tex = (NodeTexNoise *)node->storage;
  const char *name = gpu_shader_get_name(tex->dimensions);
  return GPU_stack_link(mat, node, name, in, out);
}

static void node_shader_update_tex_noise(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sockVector = nodeFindSocket(node, SOCK_IN, "Vector");
  bNodeSocket *sockW = nodeFindSocket(node, SOCK_IN, "W");

  NodeTexNoise *tex = (NodeTexNoise *)node->storage;
  nodeSetSocketAvailability(sockVector, tex->dimensions != 1);
  nodeSetSocketAvailability(sockW, tex->dimensions == 1 || tex->dimensions == 4);
}

namespace blender::nodes {

class NoiseFunction : public fn::MultiFunction {
 private:
  int dimensions_;

 public:
  NoiseFunction(int dimensions) : dimensions_(dimensions)
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
    fn::MFSignatureBuilder signature{"Noise"};

    if (ELEM(dimensions, 2, 3, 4)) {
      signature.single_input<float3>("Vector");
    }
    if (ELEM(dimensions, 1, 4)) {
      signature.single_input<float>("W");
    }

    signature.single_input<float>("Scale");
    signature.single_input<float>("Detail");
    signature.single_input<float>("Roughness");
    signature.single_input<float>("Distortion");

    signature.single_output<float>("Fac");
    signature.single_output<ColorGeometry4f>("Color");

    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    int param = ELEM(dimensions_, 2, 3, 4) + ELEM(dimensions_, 1, 4);
    const VArray<float> &scale = params.readonly_single_input<float>(param++, "Scale");
    const VArray<float> &detail = params.readonly_single_input<float>(param++, "Detail");
    const VArray<float> &roughness = params.readonly_single_input<float>(param++, "Roughness");
    const VArray<float> &distortion = params.readonly_single_input<float>(param++, "Distortion");

    MutableSpan<float> r_factor = params.uninitialized_single_output_if_required<float>(param++,
                                                                                        "Fac");
    MutableSpan<ColorGeometry4f> r_color =
        params.uninitialized_single_output_if_required<ColorGeometry4f>(param++, "Color");

    const bool compute_factor = !r_factor.is_empty();
    const bool compute_color = !r_color.is_empty();

    switch (dimensions_) {
      case 1: {
        const VArray<float> &w = params.readonly_single_input<float>(0, "W");
        if (compute_factor) {
          for (int64_t i : mask) {
            const float position = w[i] * scale[i];
            r_factor[i] = noise::perlin_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
          }
        }
        if (compute_color) {
          for (int64_t i : mask) {
            const float position = w[i] * scale[i];
            const float3 c = noise::perlin_float3_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          }
        }
        break;
      }
      case 2: {
        const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
        if (compute_factor) {
          for (int64_t i : mask) {
            const float2 position = vector[i] * scale[i];
            r_factor[i] = noise::perlin_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
          }
        }
        if (compute_color) {
          for (int64_t i : mask) {
            const float2 position = vector[i] * scale[i];
            const float3 c = noise::perlin_float3_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          }
        }
        break;
      }
      case 3: {
        const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
        if (compute_factor) {
          for (int64_t i : mask) {
            const float3 position = vector[i] * scale[i];
            r_factor[i] = noise::perlin_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
          }
        }
        if (compute_color) {
          for (int64_t i : mask) {
            const float3 position = vector[i] * scale[i];
            const float3 c = noise::perlin_float3_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          }
        }
        break;
      }
      case 4: {
        const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
        const VArray<float> &w = params.readonly_single_input<float>(1, "W");
        if (compute_factor) {
          for (int64_t i : mask) {
            const float3 position_vector = vector[i] * scale[i];
            const float position_w = w[i] * scale[i];
            const float4 position{
                position_vector[0], position_vector[1], position_vector[2], position_w};
            r_factor[i] = noise::perlin_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
          }
        }
        if (compute_color) {
          for (int64_t i : mask) {
            const float3 position_vector = vector[i] * scale[i];
            const float position_w = w[i] * scale[i];
            const float4 position{
                position_vector[0], position_vector[1], position_vector[2], position_w};
            const float3 c = noise::perlin_float3_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
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
  NodeTexNoise *tex = (NodeTexNoise *)node.storage;
  builder.construct_and_set_matching_fn<NoiseFunction>(tex->dimensions);
}

}  // namespace blender::nodes

/* node type definition */
void register_node_type_sh_tex_noise(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_TEX_NOISE, "Noise Texture", NODE_CLASS_TEXTURE, 0);
  ntype.declare = blender::nodes::sh_node_tex_noise_declare;
  node_type_init(&ntype, node_shader_init_tex_noise);
  node_type_storage(
      &ntype, "NodeTexNoise", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, node_shader_gpu_tex_noise);
  node_type_update(&ntype, node_shader_update_tex_noise);
  ntype.build_multi_function = blender::nodes::sh_node_noise_build_multi_function;

  nodeRegisterType(&ntype);
}
