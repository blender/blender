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

#include "BLI_float2.hh"
#include "BLI_float4.hh"

namespace blender::nodes {

static void sh_node_tex_brick_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").min(-10000.0f).max(10000.0f).implicit_field();
  b.add_input<decl::Color>("Color1").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Color>("Color2").default_value({0.2f, 0.2f, 0.2f, 1.0f});
  b.add_input<decl::Color>("Mortar").default_value({0.0f, 0.0f, 0.0f, 1.0f}).no_muted_links();
  b.add_input<decl::Float>("Scale")
      .min(-1000.0f)
      .max(1000.0f)
      .default_value(5.0f)
      .no_muted_links();
  b.add_input<decl::Float>("Mortar Size")
      .min(0.0f)
      .max(0.125f)
      .default_value(0.02f)
      .no_muted_links();
  b.add_input<decl::Float>("Mortar Smooth").min(0.0f).max(1.0f).no_muted_links();
  b.add_input<decl::Float>("Bias").min(-1.0f).max(1.0f).no_muted_links();
  b.add_input<decl::Float>("Brick Width")
      .min(0.01f)
      .max(100.0f)
      .default_value(0.5f)
      .no_muted_links();
  b.add_input<decl::Float>("Row Height")
      .min(0.01f)
      .max(100.0f)
      .default_value(0.25f)
      .no_muted_links();
  b.add_output<decl::Color>("Color");
  b.add_output<decl::Float>("Fac");
};

}  // namespace blender::nodes

static void node_shader_init_tex_brick(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexBrick *tex = (NodeTexBrick *)MEM_callocN(sizeof(NodeTexBrick), "NodeTexBrick");
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);

  tex->offset = 0.5f;
  tex->squash = 1.0f;
  tex->offset_freq = 2;
  tex->squash_freq = 2;

  node->storage = tex;

  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (STREQ(sock->name, "Mortar Smooth")) {
      ((bNodeSocketValueFloat *)sock->default_value)->value = 0.1f;
    }
  }
}

static int node_shader_gpu_tex_brick(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);
  NodeTexBrick *tex = (NodeTexBrick *)node->storage;
  float offset_freq = tex->offset_freq;
  float squash_freq = tex->squash_freq;
  return GPU_stack_link(mat,
                        node,
                        "node_tex_brick",
                        in,
                        out,
                        GPU_uniform(&tex->offset),
                        GPU_constant(&offset_freq),
                        GPU_uniform(&tex->squash),
                        GPU_constant(&squash_freq));
}

namespace blender::nodes {

class BrickFunction : public fn::MultiFunction {
 private:
  const float offset_;
  const int offset_freq_;
  const float squash_;
  const int squash_freq_;

 public:
  BrickFunction(const float offset,
                const int offset_freq,
                const float squash,
                const int squash_freq)
      : offset_(offset), offset_freq_(offset_freq), squash_(squash), squash_freq_(squash_freq)
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"BrickTexture"};
    signature.single_input<float3>("Vector");
    signature.single_input<ColorGeometry4f>("Color1");
    signature.single_input<ColorGeometry4f>("Color2");
    signature.single_input<ColorGeometry4f>("Mortar");
    signature.single_input<float>("Scale");
    signature.single_input<float>("Mortar Size");
    signature.single_input<float>("Mortar Smooth");
    signature.single_input<float>("Bias");
    signature.single_input<float>("Brick Width");
    signature.single_input<float>("Row Height");
    signature.single_output<ColorGeometry4f>("Color");
    signature.single_output<float>("Fac");
    return signature.build();
  }

  /* Fast integer noise. */
  static float brick_noise(uint n)
  {
    n = (n + 1013) & 0x7fffffff;
    n = (n >> 13) ^ n;
    const uint nn = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
    return 0.5f * ((float)nn / 1073741824.0f);
  }

  static float smoothstepf(const float f)
  {
    const float ff = f * f;
    return (3.0f * ff - 2.0f * ff * f);
  }

  static float2 brick(float3 p,
                      float mortar_size,
                      float mortar_smooth,
                      float bias,
                      float brick_width,
                      float row_height,
                      float offset_amount,
                      int offset_frequency,
                      float squash_amount,
                      int squash_frequency)
  {
    float offset = 0.0f;

    const int rownum = (int)floorf(p.y / row_height);

    if (offset_frequency && squash_frequency) {
      brick_width *= (rownum % squash_frequency) ? 1.0f : squash_amount;
      offset = (rownum % offset_frequency) ? 0.0f : (brick_width * offset_amount);
    }

    const int bricknum = (int)floorf((p.x + offset) / brick_width);

    const float x = (p.x + offset) - brick_width * bricknum;
    const float y = p.y - row_height * rownum;

    const float tint = clamp_f(
        brick_noise((rownum << 16) + (bricknum & 0xFFFF)) + bias, 0.0f, 1.0f);
    float min_dist = std::min(std::min(x, y), std::min(brick_width - x, row_height - y));

    float mortar;
    if (min_dist >= mortar_size) {
      mortar = 0.0f;
    }
    else if (mortar_smooth == 0.0f) {
      mortar = 1.0f;
    }
    else {
      min_dist = 1.0f - min_dist / mortar_size;
      mortar = (min_dist < mortar_smooth) ? smoothstepf(min_dist / mortar_smooth) : 1.0f;
    }

    return float2(tint, mortar);
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
    const VArray<ColorGeometry4f> &color1_values = params.readonly_single_input<ColorGeometry4f>(
        1, "Color1");
    const VArray<ColorGeometry4f> &color2_values = params.readonly_single_input<ColorGeometry4f>(
        2, "Color2");
    const VArray<ColorGeometry4f> &mortar_values = params.readonly_single_input<ColorGeometry4f>(
        3, "Mortar");
    const VArray<float> &scale = params.readonly_single_input<float>(4, "Scale");
    const VArray<float> &mortar_size = params.readonly_single_input<float>(5, "Mortar Size");
    const VArray<float> &mortar_smooth = params.readonly_single_input<float>(6, "Mortar Smooth");
    const VArray<float> &bias = params.readonly_single_input<float>(7, "Bias");
    const VArray<float> &brick_width = params.readonly_single_input<float>(8, "Brick Width");
    const VArray<float> &row_height = params.readonly_single_input<float>(9, "Row Height");

    MutableSpan<ColorGeometry4f> r_color =
        params.uninitialized_single_output_if_required<ColorGeometry4f>(10, "Color");
    MutableSpan<float> r_fac = params.uninitialized_single_output_if_required<float>(11, "Fac");

    const bool store_fac = !r_fac.is_empty();
    const bool store_color = !r_color.is_empty();

    for (int64_t i : mask) {
      const float2 f2 = brick(vector[i] * scale[i],
                              mortar_size[i],
                              mortar_smooth[i],
                              bias[i],
                              brick_width[i],
                              row_height[i],
                              offset_,
                              offset_freq_,
                              squash_,
                              squash_freq_);

      float4 color_data, color1, color2, mortar;
      copy_v4_v4(color_data, color1_values[i]);
      copy_v4_v4(color1, color1_values[i]);
      copy_v4_v4(color2, color2_values[i]);
      copy_v4_v4(mortar, mortar_values[i]);
      const float tint = f2.x;
      const float f = f2.y;

      if (f != 1.0f) {
        const float facm = 1.0f - tint;
        color_data = color1 * facm + color2 * tint;
      }

      if (store_color) {
        color_data = color_data * (1.0f - f) + mortar * f;
        copy_v4_v4(r_color[i], color_data);
      }
      if (store_fac) {
        r_fac[i] = f;
      }
    }
  }
};

static void sh_node_brick_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  bNode &node = builder.node();
  NodeTexBrick *tex = (NodeTexBrick *)node.storage;

  builder.construct_and_set_matching_fn<BrickFunction>(
      tex->offset, tex->offset_freq, tex->squash, tex->squash_freq);
}

}  // namespace blender::nodes

void register_node_type_sh_tex_brick(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_TEX_BRICK, "Brick Texture", NODE_CLASS_TEXTURE, 0);
  ntype.declare = blender::nodes::sh_node_tex_brick_declare;
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_init(&ntype, node_shader_init_tex_brick);
  node_type_storage(
      &ntype, "NodeTexBrick", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, node_shader_gpu_tex_brick);
  ntype.build_multi_function = blender::nodes::sh_node_brick_build_multi_function;

  nodeRegisterType(&ntype);
}
