/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BKE_texture.h"

#include "BLI_noise.hh"

#include "NOD_multi_function.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_tex_noise_cc {

NODE_STORAGE_FUNCS(NodeTexNoise)

static void sh_node_tex_noise_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").implicit_field(NODE_DEFAULT_INPUT_POSITION_FIELD);
  b.add_input<decl::Float>("W").min(-1000.0f).max(1000.0f).make_available([](bNode &node) {
    /* Default to 1 instead of 4, because it is much faster. */
    node_storage(node).dimensions = 1;
  });
  b.add_input<decl::Float>("Scale").min(-1000.0f).max(1000.0f).default_value(5.0f).description(
      "Scale of the base noise octave");
  b.add_input<decl::Float>("Detail").min(0.0f).max(15.0f).default_value(2.0f).description(
      "The number of noise octaves. Higher values give more detailed noise but increase render "
      "time");
  b.add_input<decl::Float>("Roughness")
      .min(0.0f)
      .max(1.0f)
      .default_value(0.5f)
      .subtype(PROP_FACTOR)
      .description(
          "Blend factor between an octave and its previous one. A value of zero corresponds to "
          "zero detail");
  b.add_input<decl::Float>("Lacunarity")
      .min(0.0f)
      .max(1000.0f)
      .default_value(2.0f)
      .description(
          "The difference between the scale of each two consecutive octaves. Larger values "
          "corresponds to larger scale for higher octaves");
  b.add_input<decl::Float>("Offset")
      .min(-1000.0f)
      .max(1000.0f)
      .default_value(0.0f)
      .make_available([](bNode &node) { node_storage(node).type = SHD_NOISE_RIDGED_MULTIFRACTAL; })
      .description(
          "An added offset to each octave, determines the level where the highest octave will "
          "appear");
  b.add_input<decl::Float>("Gain")
      .min(0.0f)
      .max(1000.0f)
      .default_value(1.0f)
      .make_available([](bNode &node) { node_storage(node).type = SHD_NOISE_RIDGED_MULTIFRACTAL; })
      .description("An extra multiplier to tune the magnitude of octaves");
  b.add_input<decl::Float>("Distortion")
      .min(-1000.0f)
      .max(1000.0f)
      .default_value(0.0f)
      .description("Amount of distortion");
  b.add_output<decl::Float>("Factor", "Fac").no_muted_links();
  b.add_output<decl::Color>("Color").no_muted_links();
}

static void node_shader_buts_tex_noise(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "noise_dimensions", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  layout->prop(ptr, "noise_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  if (ELEM(RNA_enum_get(ptr, "noise_type"), SHD_NOISE_FBM)) {
    layout->prop(ptr, "normalize", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  }
}

static void node_shader_init_tex_noise(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexNoise *tex = MEM_callocN<NodeTexNoise>(__func__);
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->dimensions = 3;
  tex->type = SHD_NOISE_FBM;
  tex->normalize = true;

  node->storage = tex;
}

static const char *gpu_shader_get_name(const int dimensions, const int type)
{
  BLI_assert(dimensions > 0 && dimensions < 5);
  BLI_assert(type >= 0 && type < 5);

  switch (type) {
    case SHD_NOISE_MULTIFRACTAL:
      return std::array{"node_noise_tex_multi_fractal_1d",
                        "node_noise_tex_multi_fractal_2d",
                        "node_noise_tex_multi_fractal_3d",
                        "node_noise_tex_multi_fractal_4d"}[dimensions - 1];
    case SHD_NOISE_FBM:
      return std::array{"node_noise_tex_fbm_1d",
                        "node_noise_tex_fbm_2d",
                        "node_noise_tex_fbm_3d",
                        "node_noise_tex_fbm_4d"}[dimensions - 1];
    case SHD_NOISE_HYBRID_MULTIFRACTAL:
      return std::array{"node_noise_tex_hybrid_multi_fractal_1d",
                        "node_noise_tex_hybrid_multi_fractal_2d",
                        "node_noise_tex_hybrid_multi_fractal_3d",
                        "node_noise_tex_hybrid_multi_fractal_4d"}[dimensions - 1];
    case SHD_NOISE_RIDGED_MULTIFRACTAL:
      return std::array{"node_noise_tex_ridged_multi_fractal_1d",
                        "node_noise_tex_ridged_multi_fractal_2d",
                        "node_noise_tex_ridged_multi_fractal_3d",
                        "node_noise_tex_ridged_multi_fractal_4d"}[dimensions - 1];
    case SHD_NOISE_HETERO_TERRAIN:
      return std::array{"node_noise_tex_hetero_terrain_1d",
                        "node_noise_tex_hetero_terrain_2d",
                        "node_noise_tex_hetero_terrain_3d",
                        "node_noise_tex_hetero_terrain_4d"}[dimensions - 1];
  }
  return nullptr;
}

static int node_shader_gpu_tex_noise(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  const NodeTexNoise &storage = node_storage(*node);
  float normalize = storage.normalize;

  const char *name = gpu_shader_get_name(storage.dimensions, storage.type);
  return GPU_stack_link(mat, node, name, in, out, GPU_constant(&normalize));
}

static void node_shader_update_tex_noise(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sockVector = bke::node_find_socket(*node, SOCK_IN, "Vector");
  bNodeSocket *sockW = bke::node_find_socket(*node, SOCK_IN, "W");
  bNodeSocket *inOffsetSock = bke::node_find_socket(*node, SOCK_IN, "Offset");
  bNodeSocket *inGainSock = bke::node_find_socket(*node, SOCK_IN, "Gain");

  const NodeTexNoise &storage = node_storage(*node);
  bke::node_set_socket_availability(*ntree, *sockVector, storage.dimensions != 1);
  bke::node_set_socket_availability(
      *ntree, *sockW, storage.dimensions == 1 || storage.dimensions == 4);
  bke::node_set_socket_availability(*ntree,
                                    *inOffsetSock,
                                    storage.type != SHD_NOISE_MULTIFRACTAL &&
                                        storage.type != SHD_NOISE_FBM);
  bke::node_set_socket_availability(*ntree,
                                    *inGainSock,
                                    storage.type == SHD_NOISE_HYBRID_MULTIFRACTAL ||
                                        storage.type == SHD_NOISE_RIDGED_MULTIFRACTAL);
}

class NoiseFunction : public mf::MultiFunction {
 private:
  int dimensions_;
  int type_;
  bool normalize_;

 public:
  NoiseFunction(int dimensions, int type, bool normalize)
      : dimensions_(dimensions), type_(type), normalize_(normalize)
  {
    BLI_assert(dimensions >= 1 && dimensions <= 4);
    BLI_assert(type >= 0 && type <= 4);
    static std::array<mf::Signature, 20> signatures{
        create_signature(1, SHD_NOISE_MULTIFRACTAL),
        create_signature(2, SHD_NOISE_MULTIFRACTAL),
        create_signature(3, SHD_NOISE_MULTIFRACTAL),
        create_signature(4, SHD_NOISE_MULTIFRACTAL),

        create_signature(1, SHD_NOISE_FBM),
        create_signature(2, SHD_NOISE_FBM),
        create_signature(3, SHD_NOISE_FBM),
        create_signature(4, SHD_NOISE_FBM),

        create_signature(1, SHD_NOISE_HYBRID_MULTIFRACTAL),
        create_signature(2, SHD_NOISE_HYBRID_MULTIFRACTAL),
        create_signature(3, SHD_NOISE_HYBRID_MULTIFRACTAL),
        create_signature(4, SHD_NOISE_HYBRID_MULTIFRACTAL),

        create_signature(1, SHD_NOISE_RIDGED_MULTIFRACTAL),
        create_signature(2, SHD_NOISE_RIDGED_MULTIFRACTAL),
        create_signature(3, SHD_NOISE_RIDGED_MULTIFRACTAL),
        create_signature(4, SHD_NOISE_RIDGED_MULTIFRACTAL),

        create_signature(1, SHD_NOISE_HETERO_TERRAIN),
        create_signature(2, SHD_NOISE_HETERO_TERRAIN),
        create_signature(3, SHD_NOISE_HETERO_TERRAIN),
        create_signature(4, SHD_NOISE_HETERO_TERRAIN),
    };
    this->set_signature(&signatures[dimensions + type * 4 - 1]);
  }

  static mf::Signature create_signature(int dimensions, int type)
  {
    mf::Signature signature;
    mf::SignatureBuilder builder{"Noise", signature};

    if (ELEM(dimensions, 2, 3, 4)) {
      builder.single_input<float3>("Vector");
    }
    if (ELEM(dimensions, 1, 4)) {
      builder.single_input<float>("W");
    }

    builder.single_input<float>("Scale");
    builder.single_input<float>("Detail");
    builder.single_input<float>("Roughness");
    builder.single_input<float>("Lacunarity");
    if (ELEM(type,
             SHD_NOISE_RIDGED_MULTIFRACTAL,
             SHD_NOISE_HYBRID_MULTIFRACTAL,
             SHD_NOISE_HETERO_TERRAIN))
    {
      builder.single_input<float>("Offset");
    }
    if (ELEM(type, SHD_NOISE_RIDGED_MULTIFRACTAL, SHD_NOISE_HYBRID_MULTIFRACTAL)) {
      builder.single_input<float>("Gain");
    }
    builder.single_input<float>("Distortion");

    builder.single_output<float>("Fac", mf::ParamFlag::SupportsUnusedOutput);
    builder.single_output<ColorGeometry4f>("Color", mf::ParamFlag::SupportsUnusedOutput);

    return signature;
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    int param = ELEM(dimensions_, 2, 3, 4) + ELEM(dimensions_, 1, 4);
    const VArray<float> &scale = params.readonly_single_input<float>(param++, "Scale");
    const VArray<float> &detail = params.readonly_single_input<float>(param++, "Detail");
    const VArray<float> &roughness = params.readonly_single_input<float>(param++, "Roughness");
    const VArray<float> &lacunarity = params.readonly_single_input<float>(param++, "Lacunarity");
    /* Initialize to any other variable when unused to avoid unnecessary conditionals. */
    const VArray<float> &offset = ELEM(type_,
                                       SHD_NOISE_RIDGED_MULTIFRACTAL,
                                       SHD_NOISE_HYBRID_MULTIFRACTAL,
                                       SHD_NOISE_HETERO_TERRAIN) ?
                                      params.readonly_single_input<float>(param++, "Offset") :
                                      scale;
    /* Initialize to any other variable when unused to avoid unnecessary conditionals. */
    const VArray<float> &gain = ELEM(type_,
                                     SHD_NOISE_RIDGED_MULTIFRACTAL,
                                     SHD_NOISE_HYBRID_MULTIFRACTAL) ?
                                    params.readonly_single_input<float>(param++, "Gain") :
                                    scale;
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
          mask.foreach_index([&](const int64_t i) {
            const float position = w[i] * scale[i];
            r_factor[i] = noise::perlin_fractal_distorted(position,
                                                          math::clamp(detail[i], 0.0f, 15.0f),
                                                          math::max(roughness[i], 0.0f),
                                                          lacunarity[i],
                                                          offset[i],
                                                          gain[i],
                                                          distortion[i],
                                                          type_,
                                                          normalize_);
          });
        }
        if (compute_color) {
          mask.foreach_index([&](const int64_t i) {
            const float position = w[i] * scale[i];
            const float3 c = noise::perlin_float3_fractal_distorted(
                position,
                math::clamp(detail[i], 0.0f, 15.0f),
                math::max(roughness[i], 0.0f),
                lacunarity[i],
                offset[i],
                gain[i],
                distortion[i],
                type_,
                normalize_);
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          });
        }
        break;
      }
      case 2: {
        const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
        if (compute_factor) {
          mask.foreach_index([&](const int64_t i) {
            const float2 position = float2(vector[i] * scale[i]);
            r_factor[i] = noise::perlin_fractal_distorted(position,
                                                          math::clamp(detail[i], 0.0f, 15.0f),
                                                          math::max(roughness[i], 0.0f),
                                                          lacunarity[i],
                                                          offset[i],
                                                          gain[i],
                                                          distortion[i],
                                                          type_,
                                                          normalize_);
          });
        }
        if (compute_color) {
          mask.foreach_index([&](const int64_t i) {
            const float2 position = float2(vector[i] * scale[i]);
            const float3 c = noise::perlin_float3_fractal_distorted(
                position,
                math::clamp(detail[i], 0.0f, 15.0f),
                math::max(roughness[i], 0.0f),
                lacunarity[i],
                offset[i],
                gain[i],
                distortion[i],
                type_,
                normalize_);
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          });
        }
        break;
      }
      case 3: {
        const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
        if (compute_factor) {
          mask.foreach_index([&](const int64_t i) {
            const float3 position = vector[i] * scale[i];
            r_factor[i] = noise::perlin_fractal_distorted(position,
                                                          math::clamp(detail[i], 0.0f, 15.0f),
                                                          math::max(roughness[i], 0.0f),
                                                          lacunarity[i],
                                                          offset[i],
                                                          gain[i],
                                                          distortion[i],
                                                          type_,
                                                          normalize_);
          });
        }
        if (compute_color) {
          mask.foreach_index([&](const int64_t i) {
            const float3 position = vector[i] * scale[i];
            const float3 c = noise::perlin_float3_fractal_distorted(
                position,
                math::clamp(detail[i], 0.0f, 15.0f),
                math::max(roughness[i], 0.0f),
                lacunarity[i],
                offset[i],
                gain[i],
                distortion[i],
                type_,
                normalize_);
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          });
        }
        break;
      }
      case 4: {
        const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
        const VArray<float> &w = params.readonly_single_input<float>(1, "W");
        if (compute_factor) {
          mask.foreach_index([&](const int64_t i) {
            const float3 position_vector = vector[i] * scale[i];
            const float position_w = w[i] * scale[i];
            const float4 position{
                position_vector[0], position_vector[1], position_vector[2], position_w};
            r_factor[i] = noise::perlin_fractal_distorted(position,
                                                          math::clamp(detail[i], 0.0f, 15.0f),
                                                          math::max(roughness[i], 0.0f),
                                                          lacunarity[i],
                                                          offset[i],
                                                          gain[i],
                                                          distortion[i],
                                                          type_,
                                                          normalize_);
          });
        }
        if (compute_color) {
          mask.foreach_index([&](const int64_t i) {
            const float3 position_vector = vector[i] * scale[i];
            const float position_w = w[i] * scale[i];
            const float4 position{
                position_vector[0], position_vector[1], position_vector[2], position_w};
            const float3 c = noise::perlin_float3_fractal_distorted(
                position,
                math::clamp(detail[i], 0.0f, 15.0f),
                math::max(roughness[i], 0.0f),
                lacunarity[i],
                offset[i],
                gain[i],
                distortion[i],
                type_,
                normalize_);
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          });
        }
        break;
      }
    }
  }

  ExecutionHints get_execution_hints() const override
  {
    ExecutionHints hints;
    hints.allocates_array = false;
    hints.min_grain_size = 100;
    return hints;
  }
};

static void sh_node_noise_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const NodeTexNoise &storage = node_storage(builder.node());
  builder.construct_and_set_matching_fn<NoiseFunction>(
      storage.dimensions, storage.type, storage.normalize);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* NOTE: Some inputs aren't supported by MaterialX. */
  NodeItem scale = get_input_value("Scale", NodeItem::Type::Float);
  NodeItem detail = get_input_default("Detail", NodeItem::Type::Float);
  NodeItem lacunarity = get_input_value("Lacunarity", NodeItem::Type::Float);
  /* Empirically, higher octaves lead to NaNs on e.g. Metal and NVIDIA. */
  const int octaves = int(math::clamp(detail.value->asA<float>(), 1.0f, 13.0f));

  NodeItem position = create_node("position", NodeItem::Type::Vector3);
  position = position * scale;

  return create_node(
      "fractal3d",
      STREQ(socket_out_->identifier, "Fac") ? NodeItem::Type::Float : NodeItem::Type::Color3,
      {{"position", position}, {"octaves", val(octaves)}, {"lacunarity", lacunarity}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_tex_noise_cc

void register_node_type_sh_tex_noise()
{
  namespace file_ns = blender::nodes::node_shader_tex_noise_cc;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeTexNoise", SH_NODE_TEX_NOISE);
  ntype.ui_name = "Noise Texture";
  ntype.ui_description = "Generate fractal Perlin noise";
  ntype.enum_name_legacy = "TEX_NOISE";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::sh_node_tex_noise_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_noise;
  ntype.initfunc = file_ns::node_shader_init_tex_noise;
  blender::bke::node_type_storage(
      ntype, "NodeTexNoise", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_noise;
  ntype.updatefunc = file_ns::node_shader_update_tex_noise;
  ntype.build_multi_function = file_ns::sh_node_noise_build_multi_function;
  ntype.materialx_fn = file_ns::node_shader_materialx;
  blender::bke::node_type_size(ntype, 145, 140, NODE_DEFAULT_MAX_WIDTH);

  blender::bke::node_register_type(ntype);
}
