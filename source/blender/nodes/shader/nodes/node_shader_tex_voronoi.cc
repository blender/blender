/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BKE_texture.h"

#include "BLI_noise.hh"

#include "NOD_multi_function.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_tex_voronoi_cc {

NODE_STORAGE_FUNCS(NodeTexVoronoi)

static void sh_node_tex_voronoi_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").hide_value().implicit_field(implicit_field_inputs::position);
  b.add_input<decl::Float>("W").min(-1000.0f).max(1000.0f).make_available([](bNode &node) {
    /* Default to 1 instead of 4, because it is much faster. */
    node_storage(node).dimensions = 1;
  });
  b.add_input<decl::Float>("Scale").min(-1000.0f).max(1000.0f).default_value(5.0f);
  b.add_input<decl::Float>("Detail")
      .min(0.0f)
      .max(15.0f)
      .default_value(0.0f)
      .make_available([](bNode &node) { node_storage(node).feature = SHD_VORONOI_F1; })
      .description("The number of Voronoi layers to sum");
  b.add_input<decl::Float>("Roughness")
      .min(0.0f)
      .max(1.0f)
      .default_value(0.5f)
      .subtype(PROP_FACTOR)
      .make_available([](bNode &node) { node_storage(node).feature = SHD_VORONOI_F1; })
      .description("The influence of a Voronoi layer relative to that of the previous layer");
  b.add_input<decl::Float>("Lacunarity")
      .min(0.0f)
      .max(1000.0f)
      .default_value(2.0f)
      .make_available([](bNode &node) { node_storage(node).feature = SHD_VORONOI_F1; })
      .description("The scale of a Voronoi layer relative to that of the previous layer");
  b.add_input<decl::Float>("Smoothness")
      .min(0.0f)
      .max(1.0f)
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .make_available([](bNode &node) { node_storage(node).feature = SHD_VORONOI_SMOOTH_F1; });
  b.add_input<decl::Float>("Exponent")
      .min(0.0f)
      .max(32.0f)
      .default_value(0.5f)
      .make_available([](bNode &node) { node_storage(node).distance = SHD_VORONOI_MINKOWSKI; });
  b.add_input<decl::Float>("Randomness")
      .min(0.0f)
      .max(1.0f)
      .default_value(1.0f)
      .subtype(PROP_FACTOR);
  b.add_output<decl::Float>("Distance").no_muted_links();
  b.add_output<decl::Color>("Color").no_muted_links();
  b.add_output<decl::Vector>("Position").no_muted_links();
  b.add_output<decl::Float>("W").no_muted_links().make_available([](bNode &node) {
    /* Default to 1 instead of 4, because it is much faster. */
    node_storage(node).dimensions = 1;
  });
  b.add_output<decl::Float>("Radius").no_muted_links().make_available(
      [](bNode &node) { node_storage(node).feature = SHD_VORONOI_N_SPHERE_RADIUS; });
}

static void node_shader_buts_tex_voronoi(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "voronoi_dimensions", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(layout, ptr, "feature", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  int feature = RNA_enum_get(ptr, "feature");
  if (!ELEM(feature, SHD_VORONOI_DISTANCE_TO_EDGE, SHD_VORONOI_N_SPHERE_RADIUS) &&
      RNA_enum_get(ptr, "voronoi_dimensions") != 1)
  {
    uiItemR(layout, ptr, "distance", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  }
  if (!ELEM(feature, SHD_VORONOI_N_SPHERE_RADIUS)) {
    uiItemR(layout, ptr, "normalize", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
}

static void node_shader_init_tex_voronoi(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexVoronoi *tex = MEM_cnew<NodeTexVoronoi>(__func__);
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->dimensions = 3;
  tex->distance = SHD_VORONOI_EUCLIDEAN;
  tex->feature = SHD_VORONOI_F1;
  tex->normalize = false;

  node->storage = tex;
}

static const char *gpu_shader_get_name(const int feature, const int dimensions)
{
  BLI_assert(feature >= 0 && feature < 5);
  BLI_assert(dimensions > 0 && dimensions < 5);

  switch (feature) {
    case SHD_VORONOI_F1:
      return std::array{
          "node_tex_voronoi_f1_1d",
          "node_tex_voronoi_f1_2d",
          "node_tex_voronoi_f1_3d",
          "node_tex_voronoi_f1_4d",
      }[dimensions - 1];
    case SHD_VORONOI_F2:
      return std::array{
          "node_tex_voronoi_f2_1d",
          "node_tex_voronoi_f2_2d",
          "node_tex_voronoi_f2_3d",
          "node_tex_voronoi_f2_4d",
      }[dimensions - 1];
    case SHD_VORONOI_SMOOTH_F1:
      return std::array{
          "node_tex_voronoi_smooth_f1_1d",
          "node_tex_voronoi_smooth_f1_2d",
          "node_tex_voronoi_smooth_f1_3d",
          "node_tex_voronoi_smooth_f1_4d",
      }[dimensions - 1];
    case SHD_VORONOI_DISTANCE_TO_EDGE:
      return std::array{
          "node_tex_voronoi_distance_to_edge_1d",
          "node_tex_voronoi_distance_to_edge_2d",
          "node_tex_voronoi_distance_to_edge_3d",
          "node_tex_voronoi_distance_to_edge_4d",
      }[dimensions - 1];
    case SHD_VORONOI_N_SPHERE_RADIUS:
      return std::array{
          "node_tex_voronoi_n_sphere_radius_1d",
          "node_tex_voronoi_n_sphere_radius_2d",
          "node_tex_voronoi_n_sphere_radius_3d",
          "node_tex_voronoi_n_sphere_radius_4d",
      }[dimensions - 1];
  }
  return nullptr;
}

static int node_shader_gpu_tex_voronoi(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData * /*execdata*/,
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  NodeTexVoronoi *tex = (NodeTexVoronoi *)node->storage;
  float metric = tex->distance;
  float normalize = tex->normalize;

  const char *name = gpu_shader_get_name(tex->feature, tex->dimensions);

  return GPU_stack_link(mat, node, name, in, out, GPU_constant(&metric), GPU_constant(&normalize));
}

static void node_shader_update_tex_voronoi(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *inVectorSock = nodeFindSocket(node, SOCK_IN, "Vector");
  bNodeSocket *inWSock = nodeFindSocket(node, SOCK_IN, "W");
  bNodeSocket *inDetailSock = nodeFindSocket(node, SOCK_IN, "Detail");
  bNodeSocket *inRoughnessSock = nodeFindSocket(node, SOCK_IN, "Roughness");
  bNodeSocket *inLacunaritySock = nodeFindSocket(node, SOCK_IN, "Lacunarity");
  bNodeSocket *inSmoothnessSock = nodeFindSocket(node, SOCK_IN, "Smoothness");
  bNodeSocket *inExponentSock = nodeFindSocket(node, SOCK_IN, "Exponent");

  bNodeSocket *outDistanceSock = nodeFindSocket(node, SOCK_OUT, "Distance");
  bNodeSocket *outColorSock = nodeFindSocket(node, SOCK_OUT, "Color");
  bNodeSocket *outPositionSock = nodeFindSocket(node, SOCK_OUT, "Position");
  bNodeSocket *outWSock = nodeFindSocket(node, SOCK_OUT, "W");
  bNodeSocket *outRadiusSock = nodeFindSocket(node, SOCK_OUT, "Radius");

  const NodeTexVoronoi &storage = node_storage(*node);

  bke::nodeSetSocketAvailability(
      ntree, inWSock, storage.dimensions == 1 || storage.dimensions == 4);
  bke::nodeSetSocketAvailability(ntree, inVectorSock, storage.dimensions != 1);
  bke::nodeSetSocketAvailability(
      ntree,
      inExponentSock,
      storage.distance == SHD_VORONOI_MINKOWSKI && storage.dimensions != 1 &&
          !ELEM(storage.feature, SHD_VORONOI_DISTANCE_TO_EDGE, SHD_VORONOI_N_SPHERE_RADIUS));
  bke::nodeSetSocketAvailability(
      ntree, inDetailSock, storage.feature != SHD_VORONOI_N_SPHERE_RADIUS);
  bke::nodeSetSocketAvailability(
      ntree, inRoughnessSock, storage.feature != SHD_VORONOI_N_SPHERE_RADIUS);
  bke::nodeSetSocketAvailability(
      ntree, inLacunaritySock, storage.feature != SHD_VORONOI_N_SPHERE_RADIUS);
  bke::nodeSetSocketAvailability(
      ntree, inSmoothnessSock, storage.feature == SHD_VORONOI_SMOOTH_F1);

  bke::nodeSetSocketAvailability(
      ntree, outDistanceSock, storage.feature != SHD_VORONOI_N_SPHERE_RADIUS);
  bke::nodeSetSocketAvailability(ntree,
                                 outColorSock,
                                 storage.feature != SHD_VORONOI_DISTANCE_TO_EDGE &&
                                     storage.feature != SHD_VORONOI_N_SPHERE_RADIUS);
  bke::nodeSetSocketAvailability(ntree,
                                 outPositionSock,
                                 storage.feature != SHD_VORONOI_DISTANCE_TO_EDGE &&
                                     storage.feature != SHD_VORONOI_N_SPHERE_RADIUS &&
                                     storage.dimensions != 1);
  bke::nodeSetSocketAvailability(ntree,
                                 outWSock,
                                 storage.feature != SHD_VORONOI_DISTANCE_TO_EDGE &&
                                     storage.feature != SHD_VORONOI_N_SPHERE_RADIUS &&
                                     ELEM(storage.dimensions, 1, 4));
  bke::nodeSetSocketAvailability(
      ntree, outRadiusSock, storage.feature == SHD_VORONOI_N_SPHERE_RADIUS);
}

static mf::MultiFunction::ExecutionHints voronoi_execution_hints{50, false};

class VoronoiMetricFunction : public mf::MultiFunction {
 private:
  int dimensions_;
  int feature_;
  int metric_;
  bool normalize_;

 public:
  VoronoiMetricFunction(int dimensions, int feature, int metric, bool normalize)
      : dimensions_(dimensions), feature_(feature), metric_(metric), normalize_(normalize)
  {
    BLI_assert(dimensions >= 1 && dimensions <= 4);
    BLI_assert(feature >= 0 && feature <= 4);
    if ELEM (metric_, SHD_VORONOI_MINKOWSKI) {
      static std::array<mf::Signature, 12> signatures{
          create_signature(1, SHD_VORONOI_F1, SHD_VORONOI_MINKOWSKI),
          create_signature(2, SHD_VORONOI_F1, SHD_VORONOI_MINKOWSKI),
          create_signature(3, SHD_VORONOI_F1, SHD_VORONOI_MINKOWSKI),
          create_signature(4, SHD_VORONOI_F1, SHD_VORONOI_MINKOWSKI),

          create_signature(1, SHD_VORONOI_F2, SHD_VORONOI_MINKOWSKI),
          create_signature(2, SHD_VORONOI_F2, SHD_VORONOI_MINKOWSKI),
          create_signature(3, SHD_VORONOI_F2, SHD_VORONOI_MINKOWSKI),
          create_signature(4, SHD_VORONOI_F2, SHD_VORONOI_MINKOWSKI),

          create_signature(1, SHD_VORONOI_SMOOTH_F1, SHD_VORONOI_MINKOWSKI),
          create_signature(2, SHD_VORONOI_SMOOTH_F1, SHD_VORONOI_MINKOWSKI),
          create_signature(3, SHD_VORONOI_SMOOTH_F1, SHD_VORONOI_MINKOWSKI),
          create_signature(4, SHD_VORONOI_SMOOTH_F1, SHD_VORONOI_MINKOWSKI),
      };
      this->set_signature(&signatures[dimensions + feature * 4 - 1]);
    }
    else {
      static std::array<mf::Signature, 12> signatures{
          create_signature(1, SHD_VORONOI_F1, SHD_VORONOI_EUCLIDEAN),
          create_signature(2, SHD_VORONOI_F1, SHD_VORONOI_EUCLIDEAN),
          create_signature(3, SHD_VORONOI_F1, SHD_VORONOI_EUCLIDEAN),
          create_signature(4, SHD_VORONOI_F1, SHD_VORONOI_EUCLIDEAN),

          create_signature(1, SHD_VORONOI_F2, SHD_VORONOI_EUCLIDEAN),
          create_signature(2, SHD_VORONOI_F2, SHD_VORONOI_EUCLIDEAN),
          create_signature(3, SHD_VORONOI_F2, SHD_VORONOI_EUCLIDEAN),
          create_signature(4, SHD_VORONOI_F2, SHD_VORONOI_EUCLIDEAN),

          create_signature(1, SHD_VORONOI_SMOOTH_F1, SHD_VORONOI_EUCLIDEAN),
          create_signature(2, SHD_VORONOI_SMOOTH_F1, SHD_VORONOI_EUCLIDEAN),
          create_signature(3, SHD_VORONOI_SMOOTH_F1, SHD_VORONOI_EUCLIDEAN),
          create_signature(4, SHD_VORONOI_SMOOTH_F1, SHD_VORONOI_EUCLIDEAN),
      };
      this->set_signature(&signatures[dimensions + feature * 4 - 1]);
    }
  }

  static mf::Signature create_signature(int dimensions, int feature, int metric)
  {
    mf::Signature signature;
    mf::SignatureBuilder builder{"voronoi_metric", signature};

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
    if (feature == SHD_VORONOI_SMOOTH_F1) {
      builder.single_input<float>("Smoothness");
    }
    if ((dimensions != 1) && (metric == SHD_VORONOI_MINKOWSKI)) {
      builder.single_input<float>("Exponent");
    }
    builder.single_input<float>("Randomness");

    builder.single_output<float>("Distance", mf::ParamFlag::SupportsUnusedOutput);
    builder.single_output<ColorGeometry4f>("Color", mf::ParamFlag::SupportsUnusedOutput);
    if (dimensions != 1) {
      builder.single_output<float3>("Position", mf::ParamFlag::SupportsUnusedOutput);
    }
    if (ELEM(dimensions, 1, 4)) {
      builder.single_output<float>("W", mf::ParamFlag::SupportsUnusedOutput);
    }

    return signature;
  }

  void call(const IndexMask &mask, mf::Params mf_params, mf::Context /*context*/) const override
  {
    auto get_vector = [&](int param_index) -> VArray<float3> {
      return mf_params.readonly_single_input<float3>(param_index, "Vector");
    };
    auto get_w = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "W");
    };
    auto get_scale = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "Scale");
    };
    auto get_detail = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "Detail");
    };
    auto get_roughness = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "Roughness");
    };
    auto get_lacunarity = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "Lacunarity");
    };
    auto get_smoothness = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "Smoothness");
    };
    auto get_exponent = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "Exponent");
    };
    auto get_randomness = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "Randomness");
    };
    auto get_r_distance = [&](int param_index) -> MutableSpan<float> {
      return mf_params.uninitialized_single_output_if_required<float>(param_index, "Distance");
    };
    auto get_r_color = [&](int param_index) -> MutableSpan<ColorGeometry4f> {
      return mf_params.uninitialized_single_output_if_required<ColorGeometry4f>(param_index,
                                                                                "Color");
    };
    auto get_r_position = [&](int param_index) -> MutableSpan<float3> {
      return mf_params.uninitialized_single_output_if_required<float3>(param_index, "Position");
    };
    auto get_r_w = [&](int param_index) -> MutableSpan<float> {
      return mf_params.uninitialized_single_output_if_required<float>(param_index, "W");
    };

    int param = 0;

    const VArray<float3> &vector = !ELEM(dimensions_, 1) ? get_vector(param++) : VArray<float3>{};
    const VArray<float> &w = ELEM(dimensions_, 1, 4) ? get_w(param++) : VArray<float>{};
    const VArray<float> &scale = get_scale(param++);
    const VArray<float> &detail = get_detail(param++);
    const VArray<float> &roughness = get_roughness(param++);
    const VArray<float> &lacunarity = get_lacunarity(param++);
    const VArray<float> &smoothness = ELEM(feature_, SHD_VORONOI_SMOOTH_F1) ?
                                          get_smoothness(param++) :
                                          VArray<float>{};
    const VArray<float> &exponent = ELEM(metric_, SHD_VORONOI_MINKOWSKI) && !ELEM(dimensions_, 1) ?
                                        get_exponent(param++) :
                                        VArray<float>{};
    const VArray<float> &randomness = get_randomness(param++);
    MutableSpan<float> r_distance = get_r_distance(param++);
    MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
    MutableSpan<float3> r_position = !ELEM(dimensions_, 1) ? get_r_position(param++) :
                                                             MutableSpan<float3>{};
    MutableSpan<float> r_w = ELEM(dimensions_, 1, 4) ? get_r_w(param++) : MutableSpan<float>{};
    const bool calc_distance = !r_distance.is_empty();
    const bool calc_color = !r_color.is_empty();
    const bool calc_position = !r_position.is_empty();
    const bool calc_w = !r_w.is_empty();

    noise::VoronoiParams params;
    params.feature = feature_;
    params.metric = metric_;
    params.normalize = normalize_;

    noise::VoronoiOutput output;
    switch (dimensions_) {
      case 1: {
        mask.foreach_index([&](const int64_t i) {
          params.scale = scale[i];
          params.detail = detail[i];
          params.roughness = roughness[i];
          params.lacunarity = lacunarity[i];
          params.smoothness = ELEM(feature_, SHD_VORONOI_SMOOTH_F1) ?
                                  std::min(std::max(smoothness[i] / 2.0f, 0.0f), 0.5f) :
                                  0.0f;
          params.exponent = 0.0f;
          params.randomness = std::min(std::max(randomness[i], 0.0f), 1.0f);
          params.max_distance = (0.5f + 0.5f * params.randomness) *
                                ((params.feature == SHD_VORONOI_F2) ? 2.0f : 1.0f);

          output = noise::fractal_voronoi_x_fx<float>(params, w[i] * params.scale, calc_color);
          if (calc_distance) {
            r_distance[i] = output.distance;
          }
          if (calc_color) {
            r_color[i] = ColorGeometry4f(output.color.x, output.color.y, output.color.z, 1.0f);
          }
          if (calc_w) {
            r_w[i] = output.position.w;
          }
        });
        break;
      }
      case 2: {
        mask.foreach_index([&](const int64_t i) {
          params.scale = scale[i];
          params.detail = detail[i];
          params.roughness = roughness[i];
          params.lacunarity = lacunarity[i];
          params.smoothness = ELEM(feature_, SHD_VORONOI_SMOOTH_F1) ?
                                  std::min(std::max(smoothness[i] / 2.0f, 0.0f), 0.5f) :
                                  0.0f;
          params.exponent = ELEM(metric_, SHD_VORONOI_MINKOWSKI) && !ELEM(dimensions_, 1) ?
                                exponent[i] :
                                0.0f;
          params.randomness = std::min(std::max(randomness[i], 0.0f), 1.0f);
          params.max_distance = noise::voronoi_distance(float2{0.0f, 0.0f},
                                                        float2(0.5f + 0.5f * params.randomness,
                                                               0.5f + 0.5f * params.randomness),
                                                        params) *
                                ((params.feature == SHD_VORONOI_F2) ? 2.0f : 1.0f);

          output = noise::fractal_voronoi_x_fx<float2>(
              params, float2{vector[i].x, vector[i].y} * params.scale, calc_color);
          if (calc_distance) {
            r_distance[i] = output.distance;
          }
          if (calc_color) {
            r_color[i] = ColorGeometry4f(output.color.x, output.color.y, output.color.z, 1.0f);
          }
          if (calc_position) {
            r_position[i] = float3{output.position.x, output.position.y, 0.0f};
          }
        });
        break;
      }
      case 3: {
        mask.foreach_index([&](const int64_t i) {
          params.scale = scale[i];
          params.detail = detail[i];
          params.roughness = roughness[i];
          params.lacunarity = lacunarity[i];
          params.smoothness = ELEM(feature_, SHD_VORONOI_SMOOTH_F1) ?
                                  std::min(std::max(smoothness[i] / 2.0f, 0.0f), 0.5f) :
                                  0.0f;
          params.exponent = ELEM(metric_, SHD_VORONOI_MINKOWSKI) && !ELEM(dimensions_, 1) ?
                                exponent[i] :
                                0.0f;
          params.randomness = std::min(std::max(randomness[i], 0.0f), 1.0f);
          params.max_distance = noise::voronoi_distance(float3{0.0f, 0.0f, 0.0f},
                                                        float3(0.5f + 0.5f * params.randomness,
                                                               0.5f + 0.5f * params.randomness,
                                                               0.5f + 0.5f * params.randomness),
                                                        params) *
                                ((params.feature == SHD_VORONOI_F2) ? 2.0f : 1.0f);

          output = noise::fractal_voronoi_x_fx<float3>(
              params, vector[i] * params.scale, calc_color);
          if (calc_distance) {
            r_distance[i] = output.distance;
          }
          if (calc_color) {
            r_color[i] = ColorGeometry4f(output.color.x, output.color.y, output.color.z, 1.0f);
          }
          if (calc_position) {
            r_position[i] = float3{output.position.x, output.position.y, output.position.z};
          }
        });
        break;
      }
      case 4: {
        mask.foreach_index([&](const int64_t i) {
          params.scale = scale[i];
          params.detail = detail[i];
          params.roughness = roughness[i];
          params.lacunarity = lacunarity[i];
          params.smoothness = ELEM(feature_, SHD_VORONOI_SMOOTH_F1) ?
                                  std::min(std::max(smoothness[i] / 2.0f, 0.0f), 0.5f) :
                                  0.0f;
          params.exponent = ELEM(metric_, SHD_VORONOI_MINKOWSKI) && !ELEM(dimensions_, 1) ?
                                exponent[i] :
                                0.0f;
          params.randomness = std::min(std::max(randomness[i], 0.0f), 1.0f);
          params.max_distance = noise::voronoi_distance(float4{0.0f, 0.0f, 0.0f, 0.0f},
                                                        float4(0.5f + 0.5f * params.randomness,
                                                               0.5f + 0.5f * params.randomness,
                                                               0.5f + 0.5f * params.randomness,
                                                               0.5f + 0.5f * params.randomness),
                                                        params) *
                                ((params.feature == SHD_VORONOI_F2) ? 2.0f : 1.0f);

          output = noise::fractal_voronoi_x_fx<float4>(
              params,
              float4{vector[i].x, vector[i].y, vector[i].z, w[i]} * params.scale,
              calc_color);
          if (calc_distance) {
            r_distance[i] = output.distance;
          }
          if (calc_color) {
            r_color[i] = ColorGeometry4f(output.color.x, output.color.y, output.color.z, 1.0f);
          }
          if (calc_position) {
            r_position[i] = float3{output.position.x, output.position.y, output.position.z};
          }
          if (calc_w) {
            r_w[i] = output.position.w;
          }
        });
        break;
      }
    }
  }

  ExecutionHints get_execution_hints() const override
  {
    return voronoi_execution_hints;
  }
};

class VoronoiDistToEdgeFunction : public mf::MultiFunction {
 private:
  int dimensions_;
  bool normalize_;

 public:
  VoronoiDistToEdgeFunction(int dimensions, bool normalize)
      : dimensions_(dimensions), normalize_(normalize)
  {
    BLI_assert(dimensions >= 1 && dimensions <= 4);
    static std::array<mf::Signature, 4> signatures{
        create_signature(1),
        create_signature(2),
        create_signature(3),
        create_signature(4),
    };
    this->set_signature(&signatures[dimensions - 1]);
  }

  static mf::Signature create_signature(int dimensions)
  {
    mf::Signature signature;
    mf::SignatureBuilder builder{"voronoi_dist_to_edge", signature};

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
    builder.single_input<float>("Randomness");

    builder.single_output<float>("Distance");

    return signature;
  }

  void call(const IndexMask &mask, mf::Params mf_params, mf::Context /*context*/) const override
  {
    auto get_vector = [&](int param_index) -> VArray<float3> {
      return mf_params.readonly_single_input<float3>(param_index, "Vector");
    };
    auto get_w = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "W");
    };
    auto get_scale = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "Scale");
    };
    auto get_detail = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "Detail");
    };
    auto get_roughness = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "Roughness");
    };
    auto get_lacunarity = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "Lacunarity");
    };
    auto get_randomness = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "Randomness");
    };
    auto get_r_distance = [&](int param_index) -> MutableSpan<float> {
      return mf_params.uninitialized_single_output<float>(param_index, "Distance");
    };

    int param = 0;

    const VArray<float3> &vector = !ELEM(dimensions_, 1) ? get_vector(param++) : VArray<float3>{};
    const VArray<float> &w = ELEM(dimensions_, 1, 4) ? get_w(param++) : VArray<float>{};
    const VArray<float> &scale = get_scale(param++);
    const VArray<float> &detail = get_detail(param++);
    const VArray<float> &roughness = get_roughness(param++);
    const VArray<float> &lacunarity = get_lacunarity(param++);
    const VArray<float> &randomness = get_randomness(param++);
    MutableSpan<float> r_distance = get_r_distance(param++);

    noise::VoronoiParams params;
    params.normalize = normalize_;

    switch (dimensions_) {
      case 1: {
        mask.foreach_index([&](const int64_t i) {
          params.scale = scale[i];
          params.detail = detail[i];
          params.roughness = roughness[i];
          params.lacunarity = lacunarity[i];
          params.randomness = std::min(std::max(randomness[i], 0.0f), 1.0f);
          params.max_distance = 0.5f + 0.5f * params.randomness;

          r_distance[i] = noise::fractal_voronoi_distance_to_edge<float>(params,
                                                                         w[i] * params.scale);
        });
        break;
      }
      case 2: {
        mask.foreach_index([&](const int64_t i) {
          params.scale = scale[i];
          params.detail = detail[i];
          params.roughness = roughness[i];
          params.lacunarity = lacunarity[i];
          params.randomness = std::min(std::max(randomness[i], 0.0f), 1.0f);
          params.max_distance = 0.5f + 0.5f * params.randomness;

          r_distance[i] = noise::fractal_voronoi_distance_to_edge<float2>(
              params, float2{vector[i].x, vector[i].y} * params.scale);
        });
        break;
      }
      case 3: {
        mask.foreach_index([&](const int64_t i) {
          params.scale = scale[i];
          params.detail = detail[i];
          params.roughness = roughness[i];
          params.lacunarity = lacunarity[i];
          params.randomness = std::min(std::max(randomness[i], 0.0f), 1.0f);
          params.max_distance = 0.5f + 0.5f * params.randomness;

          r_distance[i] = noise::fractal_voronoi_distance_to_edge<float3>(
              params, vector[i] * params.scale);
        });
        break;
      }
      case 4: {
        mask.foreach_index([&](const int64_t i) {
          params.scale = scale[i];
          params.detail = detail[i];
          params.roughness = roughness[i];
          params.lacunarity = lacunarity[i];
          params.randomness = std::min(std::max(randomness[i], 0.0f), 1.0f);
          params.max_distance = 0.5f + 0.5f * params.randomness;

          r_distance[i] = noise::fractal_voronoi_distance_to_edge<float4>(
              params, float4{vector[i].x, vector[i].y, vector[i].z, w[i]} * params.scale);
        });
        break;
      }
    }
  }

  ExecutionHints get_execution_hints() const override
  {
    return voronoi_execution_hints;
  }
};

class VoronoiNSphereFunction : public mf::MultiFunction {
 private:
  int dimensions_;

 public:
  VoronoiNSphereFunction(int dimensions) : dimensions_(dimensions)
  {
    BLI_assert(dimensions >= 1 && dimensions <= 4);
    static std::array<mf::Signature, 4> signatures{
        create_signature(1),
        create_signature(2),
        create_signature(3),
        create_signature(4),
    };
    this->set_signature(&signatures[dimensions - 1]);
  }

  static mf::Signature create_signature(int dimensions)
  {
    mf::Signature signature;
    mf::SignatureBuilder builder{"voronoi_n_sphere", signature};

    if (ELEM(dimensions, 2, 3, 4)) {
      builder.single_input<float3>("Vector");
    }
    if (ELEM(dimensions, 1, 4)) {
      builder.single_input<float>("W");
    }
    builder.single_input<float>("Scale");
    builder.single_input<float>("Randomness");

    builder.single_output<float>("Radius");

    return signature;
  }

  void call(const IndexMask &mask, mf::Params mf_params, mf::Context /*context*/) const override
  {
    auto get_vector = [&](int param_index) -> VArray<float3> {
      return mf_params.readonly_single_input<float3>(param_index, "Vector");
    };
    auto get_w = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "W");
    };
    auto get_scale = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "Scale");
    };
    auto get_randomness = [&](int param_index) -> VArray<float> {
      return mf_params.readonly_single_input<float>(param_index, "Randomness");
    };
    auto get_r_radius = [&](int param_index) -> MutableSpan<float> {
      return mf_params.uninitialized_single_output<float>(param_index, "Radius");
    };

    int param = 0;

    const VArray<float3> &vector = !ELEM(dimensions_, 1) ? get_vector(param++) : VArray<float3>{};
    const VArray<float> &w = ELEM(dimensions_, 1, 4) ? get_w(param++) : VArray<float>{};
    const VArray<float> &scale = get_scale(param++);
    const VArray<float> &randomness = get_randomness(param++);
    MutableSpan<float> r_radius = get_r_radius(param++);

    noise::VoronoiParams params;

    switch (dimensions_) {
      case 1: {
        mask.foreach_index([&](const int64_t i) {
          params.scale = scale[i];
          params.randomness = std::min(std::max(randomness[i], 0.0f), 1.0f);

          r_radius[i] = noise::voronoi_n_sphere_radius(params, w[i] * params.scale);
        });
        break;
      }
      case 2: {
        mask.foreach_index([&](const int64_t i) {
          params.scale = scale[i];
          params.randomness = std::min(std::max(randomness[i], 0.0f), 1.0f);

          r_radius[i] = noise::voronoi_n_sphere_radius(
              params, float2{vector[i].x, vector[i].y} * params.scale);
        });
        break;
      }
      case 3: {
        mask.foreach_index([&](const int64_t i) {
          params.scale = scale[i];
          params.randomness = std::min(std::max(randomness[i], 0.0f), 1.0f);

          r_radius[i] = noise::voronoi_n_sphere_radius(params, vector[i] * params.scale);
        });
        break;
      }
      case 4: {
        mask.foreach_index([&](const int64_t i) {
          params.scale = scale[i];
          params.randomness = std::min(std::max(randomness[i], 0.0f), 1.0f);

          r_radius[i] = noise::voronoi_n_sphere_radius(
              params, float4{vector[i].x, vector[i].y, vector[i].z, w[i]} * params.scale);
        });
        break;
      }
    }
  }

  ExecutionHints get_execution_hints() const override
  {
    return voronoi_execution_hints;
  }
};

static void sh_node_voronoi_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const NodeTexVoronoi &storage = node_storage(builder.node());
  switch (storage.feature) {
    case SHD_VORONOI_DISTANCE_TO_EDGE: {
      builder.construct_and_set_matching_fn<VoronoiDistToEdgeFunction>(storage.dimensions,
                                                                       storage.normalize);
      break;
    }
    case SHD_VORONOI_N_SPHERE_RADIUS: {
      builder.construct_and_set_matching_fn<VoronoiNSphereFunction>(storage.dimensions);
      break;
    }
    default: {
      builder.construct_and_set_matching_fn<VoronoiMetricFunction>(
          storage.dimensions, storage.feature, storage.distance, storage.normalize);
      break;
    }
  }
}

}  // namespace blender::nodes::node_shader_tex_voronoi_cc

void register_node_type_sh_tex_voronoi()
{
  namespace file_ns = blender::nodes::node_shader_tex_voronoi_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_TEX_VORONOI, "Voronoi Texture", NODE_CLASS_TEXTURE);
  ntype.declare = file_ns::sh_node_tex_voronoi_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_voronoi;
  ntype.initfunc = file_ns::node_shader_init_tex_voronoi;
  node_type_storage(
      &ntype, "NodeTexVoronoi", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_voronoi;
  ntype.updatefunc = file_ns::node_shader_update_tex_voronoi;
  ntype.build_multi_function = file_ns::sh_node_voronoi_build_multi_function;

  nodeRegisterType(&ntype);
}
