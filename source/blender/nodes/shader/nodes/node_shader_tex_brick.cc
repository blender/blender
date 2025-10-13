/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BKE_texture.h"

#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"

#include "NOD_multi_function.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_tex_brick_cc {

static void sh_node_tex_brick_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").min(-10000.0f).max(10000.0f).implicit_field(
      NODE_DEFAULT_INPUT_POSITION_FIELD);
  b.add_input<decl::Color>("Color1")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .description("Color of the first reference brick");
  b.add_input<decl::Color>("Color2")
      .default_value({0.2f, 0.2f, 0.2f, 1.0f})
      .description("Color of the second reference brick");
  b.add_input<decl::Color>("Mortar")
      .default_value({0.0f, 0.0f, 0.0f, 1.0f})
      .no_muted_links()
      .description("Color of the area between bricks");
  b.add_input<decl::Float>("Scale")
      .min(-1000.0f)
      .max(1000.0f)
      .default_value(5.0f)
      .no_muted_links()
      .description("Scale of the texture");
  b.add_input<decl::Float>("Mortar Size")
      .min(0.0f)
      .max(0.125f)
      .default_value(0.02f)
      .no_muted_links()
      .description(
          "Size of the filling between the bricks (known as \"mortar\"). "
          "0 means no mortar");
  b.add_input<decl::Float>("Mortar Smooth")
      .min(0.0f)
      .max(1.0f)
      .default_value(0.1f)
      .no_muted_links()
      .description(
          "Blurs/softens the edge between the mortar and the bricks. "
          "This can be useful with a texture and displacement textures");
  b.add_input<decl::Float>("Bias").min(-1.0f).max(1.0f).no_muted_links().description(
      "The color variation between Color1 and Color2. "
      "Values of -1 and 1 only use one of the two colors. "
      "Values in between mix the colors");
  b.add_input<decl::Float>("Brick Width")
      .min(0.01f)
      .max(100.0f)
      .default_value(0.5f)
      .no_muted_links()
      .description("Ratio of brick's width relative to the texture scale");
  b.add_input<decl::Float>("Row Height")
      .min(0.01f)
      .max(100.0f)
      .default_value(0.25f)
      .no_muted_links()
      .description("Ratio of brick's row height relative to the texture scale");
  b.add_output<decl::Color>("Color");
  b.add_output<decl::Float>("Factor", "Fac");
}

static void node_shader_buts_tex_brick(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = &layout->column(true);
  col->prop(
      ptr, "offset", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, IFACE_("Offset"), ICON_NONE);
  col->prop(ptr, "offset_frequency", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Frequency"), ICON_NONE);

  col = &layout->column(true);
  col->prop(ptr, "squash", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Squash"), ICON_NONE);
  col->prop(ptr, "squash_frequency", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Frequency"), ICON_NONE);
}

static void node_shader_init_tex_brick(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexBrick *tex = MEM_callocN<NodeTexBrick>(__func__);
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);

  tex->offset = 0.5f;
  tex->squash = 1.0f;
  tex->offset_freq = 2;
  tex->squash_freq = 2;

  node->storage = tex;
}

static int node_shader_gpu_tex_brick(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
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

class BrickFunction : public mf::MultiFunction {
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
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"BrickTexture", signature};
      builder.single_input<float3>("Vector");
      builder.single_input<ColorGeometry4f>("Color1");
      builder.single_input<ColorGeometry4f>("Color2");
      builder.single_input<ColorGeometry4f>("Mortar");
      builder.single_input<float>("Scale");
      builder.single_input<float>("Mortar Size");
      builder.single_input<float>("Mortar Smooth");
      builder.single_input<float>("Bias");
      builder.single_input<float>("Brick Width");
      builder.single_input<float>("Row Height");
      builder.single_output<ColorGeometry4f>("Color", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float>("Fac", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);
  }

  /* Fast integer noise. */
  static float brick_noise(uint n)
  {
    n = (n + 1013) & 0x7fffffff;
    n = (n >> 13) ^ n;
    const uint nn = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
    return 0.5f * (float(nn) / 1073741824.0f);
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

    const int rownum = int(floorf(p.y / row_height));

    if (offset_frequency && squash_frequency) {
      brick_width *= (rownum % squash_frequency) ? 1.0f : squash_amount;
      offset = (rownum % offset_frequency) ? 0.0f : (brick_width * offset_amount);
    }

    const int bricknum = int(floorf((p.x + offset) / brick_width));

    const float x = (p.x + offset) - brick_width * bricknum;
    const float y = p.y - row_height * rownum;

    const float tint = clamp_f(
        brick_noise((rownum << 16) + (bricknum & 0xFFFF)) + bias, 0.0f, 1.0f);
    float min_dist = std::min({x, y, brick_width - x, row_height - y});

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

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
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

    mask.foreach_index([&](const int64_t i) {
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
    });
  }
};

static void sh_node_brick_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &node = builder.node();
  NodeTexBrick *tex = (NodeTexBrick *)node.storage;

  builder.construct_and_set_matching_fn<BrickFunction>(
      tex->offset, tex->offset_freq, tex->squash, tex->squash_freq);
}

}  // namespace blender::nodes::node_shader_tex_brick_cc

void register_node_type_sh_tex_brick()
{
  namespace file_ns = blender::nodes::node_shader_tex_brick_cc;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeTexBrick", SH_NODE_TEX_BRICK);
  ntype.ui_name = "Brick Texture";
  ntype.ui_description = "Generate a procedural texture producing bricks";
  ntype.enum_name_legacy = "TEX_BRICK";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::sh_node_tex_brick_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_brick;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Middle);
  ntype.initfunc = file_ns::node_shader_init_tex_brick;
  blender::bke::node_type_storage(
      ntype, "NodeTexBrick", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_brick;
  ntype.build_multi_function = file_ns::sh_node_brick_build_multi_function;
  blender::bke::node_type_size(ntype, 165, 140, NODE_DEFAULT_MAX_WIDTH);

  blender::bke::node_register_type(ntype);
}
