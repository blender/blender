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

static void sh_node_tex_wave_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>(N_("Vector")).implicit_field();
  b.add_input<decl::Float>(N_("Scale")).min(-1000.0f).max(1000.0f).default_value(5.0f);
  b.add_input<decl::Float>(N_("Distortion")).min(-1000.0f).max(1000.0f).default_value(0.0f);
  b.add_input<decl::Float>(N_("Detail")).min(0.0f).max(16.0f).default_value(2.0f);
  b.add_input<decl::Float>(N_("Detail Scale")).min(-1000.0f).max(1000.0f).default_value(1.0f);
  b.add_input<decl::Float>(N_("Detail Roughness"))
      .min(0.0f)
      .max(1.0f)
      .default_value(0.5f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Phase Offset")).min(-1000.0f).max(1000.0f).default_value(0.0f);
  b.add_output<decl::Color>(N_("Color")).no_muted_links();
  b.add_output<decl::Float>(N_("Fac")).no_muted_links();
};

}  // namespace blender::nodes

static void node_shader_init_tex_wave(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexWave *tex = (NodeTexWave *)MEM_callocN(sizeof(NodeTexWave), "NodeTexWave");
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->wave_type = SHD_WAVE_BANDS;
  tex->bands_direction = SHD_WAVE_BANDS_DIRECTION_X;
  tex->rings_direction = SHD_WAVE_RINGS_DIRECTION_X;
  tex->wave_profile = SHD_WAVE_PROFILE_SIN;
  node->storage = tex;
}

static int node_shader_gpu_tex_wave(GPUMaterial *mat,
                                    bNode *node,
                                    bNodeExecData *UNUSED(execdata),
                                    GPUNodeStack *in,
                                    GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  NodeTexWave *tex = (NodeTexWave *)node->storage;
  float wave_type = tex->wave_type;
  float bands_direction = tex->bands_direction;
  float rings_direction = tex->rings_direction;
  float wave_profile = tex->wave_profile;

  return GPU_stack_link(mat,
                        node,
                        "node_tex_wave",
                        in,
                        out,
                        GPU_constant(&wave_type),
                        GPU_constant(&bands_direction),
                        GPU_constant(&rings_direction),
                        GPU_constant(&wave_profile));
}

namespace blender::nodes {

class WaveFunction : public fn::MultiFunction {
 private:
  int wave_type_;
  int bands_direction_;
  int rings_direction_;
  int wave_profile_;

 public:
  WaveFunction(int wave_type, int bands_direction, int rings_direction, int wave_profile)
      : wave_type_(wave_type),
        bands_direction_(bands_direction),
        rings_direction_(rings_direction),
        wave_profile_(wave_profile)
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"MagicFunction"};
    signature.single_input<float3>("Vector");
    signature.single_input<float>("Scale");
    signature.single_input<float>("Distortion");
    signature.single_input<float>("Detail");
    signature.single_input<float>("Detail Scale");
    signature.single_input<float>("Detail Roughness");
    signature.single_input<float>("Phase Offset");
    signature.single_output<ColorGeometry4f>("Color");
    signature.single_output<float>("Fac");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
    const VArray<float> &scale = params.readonly_single_input<float>(1, "Scale");
    const VArray<float> &distortion = params.readonly_single_input<float>(2, "Distortion");
    const VArray<float> &detail = params.readonly_single_input<float>(3, "Detail");
    const VArray<float> &dscale = params.readonly_single_input<float>(4, "Detail Scale");
    const VArray<float> &droughness = params.readonly_single_input<float>(5, "Detail Roughness");
    const VArray<float> &phase = params.readonly_single_input<float>(6, "Phase Offset");

    MutableSpan<ColorGeometry4f> r_color =
        params.uninitialized_single_output_if_required<ColorGeometry4f>(7, "Color");
    MutableSpan<float> r_fac = params.uninitialized_single_output<float>(8, "Fac");

    for (int64_t i : mask) {

      float3 p = vector[i] * scale[i];
      /* Prevent precision issues on unit coordinates. */
      p = (p + 0.000001f) * 0.999999f;

      float n = 0.0f;
      float val = 0.0f;

      switch (wave_type_) {
        case SHD_WAVE_BANDS:
          switch (bands_direction_) {
            case SHD_WAVE_BANDS_DIRECTION_X:
              n = p.x * 20.0f;
              break;
            case SHD_WAVE_BANDS_DIRECTION_Y:
              n = p.y * 20.0f;
              break;
            case SHD_WAVE_BANDS_DIRECTION_Z:
              n = p.z * 20.0f;
              break;
            case SHD_WAVE_BANDS_DIRECTION_DIAGONAL:
              n = (p.x + p.y + p.z) * 10.0f;
              break;
          }
          break;
        case SHD_WAVE_RINGS:
          float3 rp = p;
          switch (rings_direction_) {
            case SHD_WAVE_RINGS_DIRECTION_X:
              rp *= float3(0.0f, 1.0f, 1.0f);
              break;
            case SHD_WAVE_RINGS_DIRECTION_Y:
              rp *= float3(1.0f, 0.0f, 1.0f);
              break;
            case SHD_WAVE_RINGS_DIRECTION_Z:
              rp *= float3(1.0f, 1.0f, 0.0f);
              break;
            case SHD_WAVE_RINGS_DIRECTION_SPHERICAL:
              /* Ignore. */
              break;
          }
          n = len_v3(rp) * 20.0f;
          break;
      }

      n += phase[i];

      if (distortion[i] != 0.0f) {
        n += distortion[i] *
             (noise::perlin_fractal(p * dscale[i], detail[i], droughness[i]) * 2.0f - 1.0f);
      }

      switch (wave_profile_) {
        case SHD_WAVE_PROFILE_SIN:
          val = 0.5f + 0.5f * sinf(n - M_PI_2);
          break;
        case SHD_WAVE_PROFILE_SAW:
          n /= M_PI * 2.0f;
          val = n - floorf(n);
          break;
        case SHD_WAVE_PROFILE_TRI:
          n /= M_PI * 2.0f;
          val = fabsf(n - floorf(n + 0.5f)) * 2.0f;
          break;
      }

      r_fac[i] = val;
    }
    if (!r_color.is_empty()) {
      for (int64_t i : mask) {
        r_color[i] = ColorGeometry4f(r_fac[i], r_fac[i], r_fac[i], 1.0f);
      }
    }
  }
};

static void sh_node_wave_tex_build_multi_function(
    blender::nodes::NodeMultiFunctionBuilder &builder)
{
  bNode &node = builder.node();
  NodeTexWave *tex = (NodeTexWave *)node.storage;
  builder.construct_and_set_matching_fn<WaveFunction>(
      tex->wave_type, tex->bands_direction, tex->rings_direction, tex->wave_profile);
}

}  // namespace blender::nodes

void register_node_type_sh_tex_wave(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_TEX_WAVE, "Wave Texture", NODE_CLASS_TEXTURE, 0);
  ntype.declare = blender::nodes::sh_node_tex_wave_declare;
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_init(&ntype, node_shader_init_tex_wave);
  node_type_storage(&ntype, "NodeTexWave", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, node_shader_gpu_tex_wave);
  ntype.build_multi_function = blender::nodes::sh_node_wave_tex_build_multi_function;

  nodeRegisterType(&ntype);
}
