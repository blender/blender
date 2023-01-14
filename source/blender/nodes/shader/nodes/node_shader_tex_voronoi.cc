/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

#include "node_shader_util.hh"

#include "BLI_noise.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_tex_voronoi_cc {

NODE_STORAGE_FUNCS(NodeTexVoronoi)

static void sh_node_tex_voronoi_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>(N_("Vector"))
      .hide_value()
      .implicit_field(implicit_field_inputs::position);
  b.add_input<decl::Float>(N_("W")).min(-1000.0f).max(1000.0f).make_available([](bNode &node) {
    /* Default to 1 instead of 4, because it is much faster. */
    node_storage(node).dimensions = 1;
  });
  b.add_input<decl::Float>(N_("Scale")).min(-1000.0f).max(1000.0f).default_value(5.0f);
  b.add_input<decl::Float>(N_("Smoothness"))
      .min(0.0f)
      .max(1.0f)
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .make_available([](bNode &node) { node_storage(node).feature = SHD_VORONOI_SMOOTH_F1; });
  b.add_input<decl::Float>(N_("Exponent"))
      .min(0.0f)
      .max(32.0f)
      .default_value(0.5f)
      .make_available([](bNode &node) { node_storage(node).distance = SHD_VORONOI_MINKOWSKI; });
  b.add_input<decl::Float>(N_("Randomness"))
      .min(0.0f)
      .max(1.0f)
      .default_value(1.0f)
      .subtype(PROP_FACTOR);
  b.add_output<decl::Float>(N_("Distance")).no_muted_links();
  b.add_output<decl::Color>(N_("Color")).no_muted_links();
  b.add_output<decl::Vector>(N_("Position")).no_muted_links();
  b.add_output<decl::Float>(N_("W")).no_muted_links().make_available([](bNode &node) {
    /* Default to 1 instead of 4, because it is much faster. */
    node_storage(node).dimensions = 1;
  });
  b.add_output<decl::Float>(N_("Radius")).no_muted_links().make_available([](bNode &node) {
    node_storage(node).feature = SHD_VORONOI_N_SPHERE_RADIUS;
  });
}

static void node_shader_buts_tex_voronoi(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "voronoi_dimensions", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(layout, ptr, "feature", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  int feature = RNA_enum_get(ptr, "feature");
  if (!ELEM(feature, SHD_VORONOI_DISTANCE_TO_EDGE, SHD_VORONOI_N_SPHERE_RADIUS) &&
      RNA_enum_get(ptr, "voronoi_dimensions") != 1) {
    uiItemR(layout, ptr, "distance", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
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

  const char *name = gpu_shader_get_name(tex->feature, tex->dimensions);

  return GPU_stack_link(mat, node, name, in, out, GPU_constant(&metric));
}

static void node_shader_update_tex_voronoi(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *inVectorSock = nodeFindSocket(node, SOCK_IN, "Vector");
  bNodeSocket *inWSock = nodeFindSocket(node, SOCK_IN, "W");
  bNodeSocket *inSmoothnessSock = nodeFindSocket(node, SOCK_IN, "Smoothness");
  bNodeSocket *inExponentSock = nodeFindSocket(node, SOCK_IN, "Exponent");

  bNodeSocket *outDistanceSock = nodeFindSocket(node, SOCK_OUT, "Distance");
  bNodeSocket *outColorSock = nodeFindSocket(node, SOCK_OUT, "Color");
  bNodeSocket *outPositionSock = nodeFindSocket(node, SOCK_OUT, "Position");
  bNodeSocket *outWSock = nodeFindSocket(node, SOCK_OUT, "W");
  bNodeSocket *outRadiusSock = nodeFindSocket(node, SOCK_OUT, "Radius");

  const NodeTexVoronoi &storage = node_storage(*node);

  nodeSetSocketAvailability(ntree, inWSock, storage.dimensions == 1 || storage.dimensions == 4);
  nodeSetSocketAvailability(ntree, inVectorSock, storage.dimensions != 1);
  nodeSetSocketAvailability(
      ntree,
      inExponentSock,
      storage.distance == SHD_VORONOI_MINKOWSKI && storage.dimensions != 1 &&
          !ELEM(storage.feature, SHD_VORONOI_DISTANCE_TO_EDGE, SHD_VORONOI_N_SPHERE_RADIUS));
  nodeSetSocketAvailability(ntree, inSmoothnessSock, storage.feature == SHD_VORONOI_SMOOTH_F1);

  nodeSetSocketAvailability(
      ntree, outDistanceSock, storage.feature != SHD_VORONOI_N_SPHERE_RADIUS);
  nodeSetSocketAvailability(ntree,
                            outColorSock,
                            storage.feature != SHD_VORONOI_DISTANCE_TO_EDGE &&
                                storage.feature != SHD_VORONOI_N_SPHERE_RADIUS);
  nodeSetSocketAvailability(ntree,
                            outPositionSock,
                            storage.feature != SHD_VORONOI_DISTANCE_TO_EDGE &&
                                storage.feature != SHD_VORONOI_N_SPHERE_RADIUS &&
                                storage.dimensions != 1);
  nodeSetSocketAvailability(ntree,
                            outWSock,
                            storage.feature != SHD_VORONOI_DISTANCE_TO_EDGE &&
                                storage.feature != SHD_VORONOI_N_SPHERE_RADIUS &&
                                ELEM(storage.dimensions, 1, 4));
  nodeSetSocketAvailability(ntree, outRadiusSock, storage.feature == SHD_VORONOI_N_SPHERE_RADIUS);
}

static mf::MultiFunction::ExecutionHints voronoi_execution_hints{50, false};

class VoronoiMinowskiFunction : public mf::MultiFunction {
 private:
  int dimensions_;
  int feature_;

 public:
  VoronoiMinowskiFunction(int dimensions, int feature) : dimensions_(dimensions), feature_(feature)
  {
    BLI_assert(dimensions >= 2 && dimensions <= 4);
    BLI_assert(feature >= 0 && feature <= 2);
    static std::array<mf::Signature, 9> signatures{
        create_signature(2, SHD_VORONOI_F1),
        create_signature(3, SHD_VORONOI_F1),
        create_signature(4, SHD_VORONOI_F1),

        create_signature(2, SHD_VORONOI_F2),
        create_signature(3, SHD_VORONOI_F2),
        create_signature(4, SHD_VORONOI_F2),

        create_signature(2, SHD_VORONOI_SMOOTH_F1),
        create_signature(3, SHD_VORONOI_SMOOTH_F1),
        create_signature(4, SHD_VORONOI_SMOOTH_F1),
    };
    this->set_signature(&signatures[(dimensions - 1) + feature * 3 - 1]);
  }

  static mf::Signature create_signature(int dimensions, int feature)
  {
    mf::Signature signature;
    mf::SignatureBuilder builder{"voronoi_minowski", signature};

    if (ELEM(dimensions, 2, 3, 4)) {
      builder.single_input<float3>("Vector");
    }
    if (ELEM(dimensions, 1, 4)) {
      builder.single_input<float>("W");
    }
    builder.single_input<float>("Scale");
    if (feature == SHD_VORONOI_SMOOTH_F1) {
      builder.single_input<float>("Smoothness");
    }
    builder.single_input<float>("Exponent");
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

  void call(IndexMask mask, mf::Params params, mf::Context /*context*/) const override
  {
    auto get_vector = [&](int param_index) -> VArray<float3> {
      return params.readonly_single_input<float3>(param_index, "Vector");
    };
    auto get_w = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "W");
    };
    auto get_scale = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Scale");
    };
    auto get_smoothness = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Smoothness");
    };
    auto get_exponent = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Exponent");
    };
    auto get_randomness = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Randomness");
    };
    auto get_r_distance = [&](int param_index) -> MutableSpan<float> {
      return params.uninitialized_single_output_if_required<float>(param_index, "Distance");
    };
    auto get_r_color = [&](int param_index) -> MutableSpan<ColorGeometry4f> {
      return params.uninitialized_single_output_if_required<ColorGeometry4f>(param_index, "Color");
    };
    auto get_r_position = [&](int param_index) -> MutableSpan<float3> {
      return params.uninitialized_single_output_if_required<float3>(param_index, "Position");
    };
    auto get_r_w = [&](int param_index) -> MutableSpan<float> {
      return params.uninitialized_single_output_if_required<float>(param_index, "W");
    };

    int param = 0;
    switch (dimensions_) {
      case 2: {
        switch (feature_) {
          case SHD_VORONOI_F1: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &exponent = get_exponent(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              float3 col;
              float2 pos;
              noise::voronoi_f1(float2(vector[i].x, vector[i].y) * scale[i],
                                exponent[i],
                                rand,
                                SHD_VORONOI_MINKOWSKI,
                                calc_distance ? &r_distance[i] : nullptr,
                                calc_color ? &col : nullptr,
                                calc_position ? &pos : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position) {
                pos = math::safe_divide(pos, scale[i]);
                r_position[i] = float3(pos.x, pos.y, 0.0f);
              }
            }
            break;
          }
          case SHD_VORONOI_F2: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &exponent = get_exponent(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              float3 col;
              float2 pos;
              noise::voronoi_f2(float2(vector[i].x, vector[i].y) * scale[i],
                                exponent[i],
                                rand,
                                SHD_VORONOI_MINKOWSKI,
                                calc_distance ? &r_distance[i] : nullptr,
                                calc_color ? &col : nullptr,
                                calc_position ? &pos : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position) {
                pos = math::safe_divide(pos, scale[i]);
                r_position[i] = float3(pos.x, pos.y, 0.0f);
              }
            }
            break;
          }
          case SHD_VORONOI_SMOOTH_F1: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &smoothness = get_smoothness(param++);
            const VArray<float> &exponent = get_exponent(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            for (int64_t i : mask) {
              const float smth = std::min(std::max(smoothness[i] / 2.0f, 0.0f), 0.5f);
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              float3 col;
              float2 pos;
              noise::voronoi_smooth_f1(float2(vector[i].x, vector[i].y) * scale[i],
                                       smth,
                                       exponent[i],
                                       rand,
                                       SHD_VORONOI_MINKOWSKI,
                                       calc_distance ? &r_distance[i] : nullptr,
                                       calc_color ? &col : nullptr,
                                       calc_position ? &pos : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position) {
                pos = math::safe_divide(pos, scale[i]);
                r_position[i] = float3(pos.x, pos.y, 0.0f);
              }
            }
            break;
          }
        }
        break;
      }
      case 3: {
        switch (feature_) {
          case SHD_VORONOI_F1: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &exponent = get_exponent(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              float3 col;
              noise::voronoi_f1(vector[i] * scale[i],
                                exponent[i],
                                rand,
                                SHD_VORONOI_MINKOWSKI,
                                calc_distance ? &r_distance[i] : nullptr,
                                calc_color ? &col : nullptr,
                                calc_position ? &r_position[i] : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position) {
                r_position[i] = math::safe_divide(r_position[i], scale[i]);
              }
            }
            break;
          }
          case SHD_VORONOI_F2: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &exponent = get_exponent(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              float3 col;
              noise::voronoi_f2(vector[i] * scale[i],
                                exponent[i],
                                rand,
                                SHD_VORONOI_MINKOWSKI,
                                calc_distance ? &r_distance[i] : nullptr,
                                calc_color ? &col : nullptr,
                                calc_position ? &r_position[i] : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position) {
                r_position[i] = math::safe_divide(r_position[i], scale[i]);
              }
            }
            break;
          }
          case SHD_VORONOI_SMOOTH_F1: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &smoothness = get_smoothness(param++);
            const VArray<float> &exponent = get_exponent(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            for (int64_t i : mask) {
              const float smth = std::min(std::max(smoothness[i] / 2.0f, 0.0f), 0.5f);
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              float3 col;
              noise::voronoi_smooth_f1(vector[i] * scale[i],
                                       smth,
                                       exponent[i],
                                       rand,
                                       SHD_VORONOI_MINKOWSKI,
                                       calc_distance ? &r_distance[i] : nullptr,
                                       calc_color ? &col : nullptr,
                                       calc_position ? &r_position[i] : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position) {
                r_position[i] = math::safe_divide(r_position[i], scale[i]);
              }
            }
            break;
          }
        }
        break;
      }
      case 4: {
        switch (feature_) {
          case SHD_VORONOI_F1: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &w = get_w(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &exponent = get_exponent(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            MutableSpan<float> r_w = get_r_w(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            const bool calc_w = !r_w.is_empty();
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              const float4 p = float4(vector[i].x, vector[i].y, vector[i].z, w[i]) * scale[i];
              float3 col;
              float4 pos;
              noise::voronoi_f1(p,
                                exponent[i],
                                rand,
                                SHD_VORONOI_F1,
                                calc_distance ? &r_distance[i] : nullptr,
                                calc_color ? &col : nullptr,
                                calc_position || calc_w ? &pos : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position || calc_w) {
                pos = math::safe_divide(pos, scale[i]);
                if (calc_position) {
                  r_position[i] = float3(pos.x, pos.y, pos.z);
                }
                if (calc_w) {
                  r_w[i] = pos.w;
                }
              }
            }
            break;
          }
          case SHD_VORONOI_F2: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &w = get_w(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &exponent = get_exponent(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            MutableSpan<float> r_w = get_r_w(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            const bool calc_w = !r_w.is_empty();
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              const float4 p = float4(vector[i].x, vector[i].y, vector[i].z, w[i]) * scale[i];
              float3 col;
              float4 pos;
              noise::voronoi_f2(p,
                                exponent[i],
                                rand,
                                SHD_VORONOI_MINKOWSKI,
                                calc_distance ? &r_distance[i] : nullptr,
                                calc_color ? &col : nullptr,
                                calc_position || calc_w ? &pos : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position || calc_w) {
                pos = math::safe_divide(pos, scale[i]);
                if (calc_position) {
                  r_position[i] = float3(pos.x, pos.y, pos.z);
                }
                if (calc_w) {
                  r_w[i] = pos.w;
                }
              }
            }
            break;
          }
          case SHD_VORONOI_SMOOTH_F1: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &w = get_w(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &smoothness = get_smoothness(param++);
            const VArray<float> &exponent = get_exponent(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            MutableSpan<float> r_w = get_r_w(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            const bool calc_w = !r_w.is_empty();
            for (int64_t i : mask) {
              const float smth = std::min(std::max(smoothness[i] / 2.0f, 0.0f), 0.5f);
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              const float4 p = float4(vector[i].x, vector[i].y, vector[i].z, w[i]) * scale[i];
              float3 col;
              float4 pos;
              noise::voronoi_smooth_f1(p,
                                       smth,
                                       exponent[i],
                                       rand,
                                       SHD_VORONOI_MINKOWSKI,
                                       calc_distance ? &r_distance[i] : nullptr,
                                       calc_color ? &col : nullptr,
                                       calc_position || calc_w ? &pos : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position || calc_w) {
                pos = math::safe_divide(pos, scale[i]);
                if (calc_position) {
                  r_position[i] = float3(pos.x, pos.y, pos.z);
                }
                if (calc_w) {
                  r_w[i] = pos.w;
                }
              }
            }
            break;
          }
        }
        break;
      }
    }
  }

  ExecutionHints get_execution_hints() const override
  {
    return voronoi_execution_hints;
  }
};

class VoronoiMetricFunction : public mf::MultiFunction {
 private:
  int dimensions_;
  int feature_;
  int metric_;

 public:
  VoronoiMetricFunction(int dimensions, int feature, int metric)
      : dimensions_(dimensions), feature_(feature), metric_(metric)
  {
    BLI_assert(dimensions >= 1 && dimensions <= 4);
    BLI_assert(feature >= 0 && feature <= 4);
    static std::array<mf::Signature, 12> signatures{
        create_signature(1, SHD_VORONOI_F1),
        create_signature(2, SHD_VORONOI_F1),
        create_signature(3, SHD_VORONOI_F1),
        create_signature(4, SHD_VORONOI_F1),

        create_signature(1, SHD_VORONOI_F2),
        create_signature(2, SHD_VORONOI_F2),
        create_signature(3, SHD_VORONOI_F2),
        create_signature(4, SHD_VORONOI_F2),

        create_signature(1, SHD_VORONOI_SMOOTH_F1),
        create_signature(2, SHD_VORONOI_SMOOTH_F1),
        create_signature(3, SHD_VORONOI_SMOOTH_F1),
        create_signature(4, SHD_VORONOI_SMOOTH_F1),
    };
    this->set_signature(&signatures[dimensions + feature * 4 - 1]);
  }

  static mf::Signature create_signature(int dimensions, int feature)
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
    if (feature == SHD_VORONOI_SMOOTH_F1) {
      builder.single_input<float>("Smoothness");
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

  void call(IndexMask mask, mf::Params params, mf::Context /*context*/) const override
  {
    auto get_vector = [&](int param_index) -> VArray<float3> {
      return params.readonly_single_input<float3>(param_index, "Vector");
    };
    auto get_w = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "W");
    };
    auto get_scale = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Scale");
    };
    auto get_smoothness = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Smoothness");
    };
    auto get_randomness = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Randomness");
    };
    auto get_r_distance = [&](int param_index) -> MutableSpan<float> {
      return params.uninitialized_single_output_if_required<float>(param_index, "Distance");
    };
    auto get_r_color = [&](int param_index) -> MutableSpan<ColorGeometry4f> {
      return params.uninitialized_single_output_if_required<ColorGeometry4f>(param_index, "Color");
    };
    auto get_r_position = [&](int param_index) -> MutableSpan<float3> {
      return params.uninitialized_single_output_if_required<float3>(param_index, "Position");
    };
    auto get_r_w = [&](int param_index) -> MutableSpan<float> {
      return params.uninitialized_single_output_if_required<float>(param_index, "W");
    };

    int param = 0;
    switch (dimensions_) {
      case 1: {
        switch (feature_) {
          case SHD_VORONOI_F1: {
            const VArray<float> &w = get_w(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float> r_w = get_r_w(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_w = !r_w.is_empty();
            for (int64_t i : mask) {
              const float p = w[i] * scale[i];
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              float3 col;
              noise::voronoi_f1(p,
                                rand,
                                calc_distance ? &r_distance[i] : nullptr,
                                calc_color ? &col : nullptr,
                                calc_w ? &r_w[i] : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_w) {
                r_w[i] = safe_divide(r_w[i], scale[i]);
              }
            }
            break;
          }
          case SHD_VORONOI_F2: {
            const VArray<float> &w = get_w(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float> r_w = get_r_w(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_w = !r_w.is_empty();
            for (int64_t i : mask) {
              const float p = w[i] * scale[i];
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              float3 col;
              noise::voronoi_f2(p,
                                rand,
                                calc_distance ? &r_distance[i] : nullptr,
                                calc_color ? &col : nullptr,
                                calc_w ? &r_w[i] : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_w) {
                r_w[i] = safe_divide(r_w[i], scale[i]);
              }
            }
            break;
          }
          case SHD_VORONOI_SMOOTH_F1: {
            const VArray<float> &w = get_w(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &smoothness = get_smoothness(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float> r_w = get_r_w(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_w = !r_w.is_empty();
            for (int64_t i : mask) {
              const float p = w[i] * scale[i];
              const float smth = std::min(std::max(smoothness[i] / 2.0f, 0.0f), 0.5f);
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              float3 col;
              noise::voronoi_smooth_f1(p,
                                       smth,
                                       rand,
                                       calc_distance ? &r_distance[i] : nullptr,
                                       calc_color ? &col : nullptr,
                                       calc_w ? &r_w[i] : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_w) {
                r_w[i] = safe_divide(r_w[i], scale[i]);
              }
            }
            break;
          }
        }
        break;
      }
      case 2: {
        switch (feature_) {
          case SHD_VORONOI_F1: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              float3 col;
              float2 pos;
              noise::voronoi_f1(float2(vector[i].x, vector[i].y) * scale[i],
                                0.0f,
                                rand,
                                metric_,
                                calc_distance ? &r_distance[i] : nullptr,
                                calc_color ? &col : nullptr,
                                calc_position ? &pos : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position) {
                pos = math::safe_divide(pos, scale[i]);
                r_position[i] = float3(pos.x, pos.y, 0.0f);
              }
            }
            break;
          }
          case SHD_VORONOI_F2: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              float3 col;
              float2 pos;
              noise::voronoi_f2(float2(vector[i].x, vector[i].y) * scale[i],
                                0.0f,
                                rand,
                                metric_,
                                calc_distance ? &r_distance[i] : nullptr,
                                calc_color ? &col : nullptr,
                                calc_position ? &pos : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position) {
                pos = math::safe_divide(pos, scale[i]);
                r_position[i] = float3(pos.x, pos.y, 0.0f);
              }
            }
            break;
          }
          case SHD_VORONOI_SMOOTH_F1: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &smoothness = get_smoothness(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            for (int64_t i : mask) {
              const float smth = std::min(std::max(smoothness[i] / 2.0f, 0.0f), 0.5f);
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              float3 col;
              float2 pos;
              noise::voronoi_smooth_f1(float2(vector[i].x, vector[i].y) * scale[i],
                                       smth,
                                       0.0f,
                                       rand,
                                       metric_,
                                       calc_distance ? &r_distance[i] : nullptr,
                                       calc_color ? &col : nullptr,
                                       calc_position ? &pos : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position) {
                pos = math::safe_divide(pos, scale[i]);
                r_position[i] = float3(pos.x, pos.y, 0.0f);
              }
            }
            break;
          }
        }
        break;
      }
      case 3: {
        switch (feature_) {
          case SHD_VORONOI_F1: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              float3 col;
              noise::voronoi_f1(vector[i] * scale[i],
                                0.0f,
                                rand,
                                metric_,
                                calc_distance ? &r_distance[i] : nullptr,
                                calc_color ? &col : nullptr,
                                calc_position ? &r_position[i] : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position) {
                r_position[i] = math::safe_divide(r_position[i], scale[i]);
              }
            }
            break;
          }
          case SHD_VORONOI_F2: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              float3 col;
              noise::voronoi_f2(vector[i] * scale[i],
                                0.0f,
                                rand,
                                metric_,
                                calc_distance ? &r_distance[i] : nullptr,
                                calc_color ? &col : nullptr,
                                calc_position ? &r_position[i] : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position) {
                r_position[i] = math::safe_divide(r_position[i], scale[i]);
              }
            }
            break;
          }
          case SHD_VORONOI_SMOOTH_F1: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &smoothness = get_smoothness(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            {
              for (int64_t i : mask) {
                const float smth = std::min(std::max(smoothness[i] / 2.0f, 0.0f), 0.5f);
                const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
                float3 col;
                noise::voronoi_smooth_f1(vector[i] * scale[i],
                                         smth,
                                         0.0f,
                                         rand,
                                         metric_,
                                         calc_distance ? &r_distance[i] : nullptr,
                                         calc_color ? &col : nullptr,
                                         calc_position ? &r_position[i] : nullptr);
                if (calc_color) {
                  r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
                }
                if (calc_position) {
                  r_position[i] = math::safe_divide(r_position[i], scale[i]);
                }
              }
            }

            break;
          }
        }
        break;
      }
      case 4: {
        switch (feature_) {
          case SHD_VORONOI_F1: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &w = get_w(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            MutableSpan<float> r_w = get_r_w(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            const bool calc_w = !r_w.is_empty();
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              const float4 p = float4(vector[i].x, vector[i].y, vector[i].z, w[i]) * scale[i];
              float3 col;
              float4 pos;
              noise::voronoi_f1(p,
                                0.0f,
                                rand,
                                metric_,
                                calc_distance ? &r_distance[i] : nullptr,
                                calc_color ? &col : nullptr,
                                calc_position || calc_w ? &pos : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position || calc_w) {
                pos = math::safe_divide(pos, scale[i]);
                if (calc_position) {
                  r_position[i] = float3(pos.x, pos.y, pos.z);
                }
                if (calc_w) {
                  r_w[i] = pos.w;
                }
              }
            }
            break;
          }
          case SHD_VORONOI_F2: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &w = get_w(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            MutableSpan<float> r_w = get_r_w(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            const bool calc_w = !r_w.is_empty();
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              const float4 p = float4(vector[i].x, vector[i].y, vector[i].z, w[i]) * scale[i];
              float3 col;
              float4 pos;
              noise::voronoi_f2(p,
                                0.0f,
                                rand,
                                metric_,
                                calc_distance ? &r_distance[i] : nullptr,
                                calc_color ? &col : nullptr,
                                calc_position || calc_w ? &pos : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position || calc_w) {
                pos = math::safe_divide(pos, scale[i]);
                if (calc_position) {
                  r_position[i] = float3(pos.x, pos.y, pos.z);
                }
                if (calc_w) {
                  r_w[i] = pos.w;
                }
              }
            }
            break;
          }
          case SHD_VORONOI_SMOOTH_F1: {
            const VArray<float3> &vector = get_vector(param++);
            const VArray<float> &w = get_w(param++);
            const VArray<float> &scale = get_scale(param++);
            const VArray<float> &smoothness = get_smoothness(param++);
            const VArray<float> &randomness = get_randomness(param++);
            MutableSpan<float> r_distance = get_r_distance(param++);
            MutableSpan<ColorGeometry4f> r_color = get_r_color(param++);
            MutableSpan<float3> r_position = get_r_position(param++);
            MutableSpan<float> r_w = get_r_w(param++);
            const bool calc_distance = !r_distance.is_empty();
            const bool calc_color = !r_color.is_empty();
            const bool calc_position = !r_position.is_empty();
            const bool calc_w = !r_w.is_empty();
            for (int64_t i : mask) {
              const float smth = std::min(std::max(smoothness[i] / 2.0f, 0.0f), 0.5f);
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              const float4 p = float4(vector[i].x, vector[i].y, vector[i].z, w[i]) * scale[i];
              float3 col;
              float4 pos;
              noise::voronoi_smooth_f1(p,
                                       smth,
                                       0.0f,
                                       rand,
                                       metric_,
                                       calc_distance ? &r_distance[i] : nullptr,
                                       calc_color ? &col : nullptr,
                                       calc_position || calc_w ? &pos : nullptr);
              if (calc_color) {
                r_color[i] = ColorGeometry4f(col[0], col[1], col[2], 1.0f);
              }
              if (calc_position || calc_w) {
                pos = math::safe_divide(pos, scale[i]);
                if (calc_position) {
                  r_position[i] = float3(pos.x, pos.y, pos.z);
                }
                if (calc_w) {
                  r_w[i] = pos.w;
                }
              }
            }
            break;
          }
        }
        break;
      }
    }
  }

  ExecutionHints get_execution_hints() const override
  {
    return voronoi_execution_hints;
  }
};

class VoronoiEdgeFunction : public mf::MultiFunction {
 private:
  int dimensions_;
  int feature_;

 public:
  VoronoiEdgeFunction(int dimensions, int feature) : dimensions_(dimensions), feature_(feature)
  {
    BLI_assert(dimensions >= 1 && dimensions <= 4);
    BLI_assert(feature >= 3 && feature <= 4);
    static std::array<mf::Signature, 8> signatures{
        create_signature(1, SHD_VORONOI_DISTANCE_TO_EDGE),
        create_signature(2, SHD_VORONOI_DISTANCE_TO_EDGE),
        create_signature(3, SHD_VORONOI_DISTANCE_TO_EDGE),
        create_signature(4, SHD_VORONOI_DISTANCE_TO_EDGE),

        create_signature(1, SHD_VORONOI_N_SPHERE_RADIUS),
        create_signature(2, SHD_VORONOI_N_SPHERE_RADIUS),
        create_signature(3, SHD_VORONOI_N_SPHERE_RADIUS),
        create_signature(4, SHD_VORONOI_N_SPHERE_RADIUS),
    };
    this->set_signature(&signatures[dimensions + (feature - 3) * 4 - 1]);
  }

  static mf::Signature create_signature(int dimensions, int feature)
  {
    mf::Signature signature;
    mf::SignatureBuilder builder{"voronoi_edge", signature};

    if (ELEM(dimensions, 2, 3, 4)) {
      builder.single_input<float3>("Vector");
    }
    if (ELEM(dimensions, 1, 4)) {
      builder.single_input<float>("W");
    }
    builder.single_input<float>("Scale");
    builder.single_input<float>("Randomness");

    if (feature == SHD_VORONOI_DISTANCE_TO_EDGE) {
      builder.single_output<float>("Distance");
    }
    if (feature == SHD_VORONOI_N_SPHERE_RADIUS) {
      builder.single_output<float>("Radius");
    }

    return signature;
  }

  void call(IndexMask mask, mf::Params params, mf::Context /*context*/) const override
  {
    auto get_vector = [&](int param_index) -> VArray<float3> {
      return params.readonly_single_input<float3>(param_index, "Vector");
    };
    auto get_w = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "W");
    };
    auto get_scale = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Scale");
    };
    auto get_randomness = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Randomness");
    };
    auto get_r_distance = [&](int param_index) -> MutableSpan<float> {
      return params.uninitialized_single_output<float>(param_index, "Distance");
    };
    auto get_r_radius = [&](int param_index) -> MutableSpan<float> {
      return params.uninitialized_single_output<float>(param_index, "Radius");
    };

    int param = 0;
    switch (dimensions_) {
      case 1: {
        const VArray<float> &w = get_w(param++);
        const VArray<float> &scale = get_scale(param++);
        const VArray<float> &randomness = get_randomness(param++);
        switch (feature_) {
          case SHD_VORONOI_DISTANCE_TO_EDGE: {
            MutableSpan<float> r_distance = get_r_distance(param++);
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              const float p = w[i] * scale[i];
              noise::voronoi_distance_to_edge(p, rand, &r_distance[i]);
            }
            break;
          }
          case SHD_VORONOI_N_SPHERE_RADIUS: {
            MutableSpan<float> r_radius = get_r_radius(param++);
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              const float p = w[i] * scale[i];
              noise::voronoi_n_sphere_radius(p, rand, &r_radius[i]);
            }
            break;
          }
        }
        break;
      }
      case 2: {
        const VArray<float3> &vector = get_vector(param++);
        const VArray<float> &scale = get_scale(param++);
        const VArray<float> &randomness = get_randomness(param++);
        switch (feature_) {
          case SHD_VORONOI_DISTANCE_TO_EDGE: {
            MutableSpan<float> r_distance = get_r_distance(param++);
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              const float2 p = float2(vector[i].x, vector[i].y) * scale[i];
              noise::voronoi_distance_to_edge(p, rand, &r_distance[i]);
            }
            break;
          }
          case SHD_VORONOI_N_SPHERE_RADIUS: {
            MutableSpan<float> r_radius = get_r_radius(param++);
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              const float2 p = float2(vector[i].x, vector[i].y) * scale[i];
              noise::voronoi_n_sphere_radius(p, rand, &r_radius[i]);
            }
            break;
          }
        }
        break;
      }
      case 3: {
        const VArray<float3> &vector = get_vector(param++);
        const VArray<float> &scale = get_scale(param++);
        const VArray<float> &randomness = get_randomness(param++);
        switch (feature_) {
          case SHD_VORONOI_DISTANCE_TO_EDGE: {
            MutableSpan<float> r_distance = get_r_distance(param++);
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              noise::voronoi_distance_to_edge(vector[i] * scale[i], rand, &r_distance[i]);
            }
            break;
          }
          case SHD_VORONOI_N_SPHERE_RADIUS: {
            MutableSpan<float> r_radius = get_r_radius(param++);
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              noise::voronoi_n_sphere_radius(vector[i] * scale[i], rand, &r_radius[i]);
            }
            break;
          }
        }
        break;
      }
      case 4: {
        const VArray<float3> &vector = get_vector(param++);
        const VArray<float> &w = get_w(param++);
        const VArray<float> &scale = get_scale(param++);
        const VArray<float> &randomness = get_randomness(param++);
        switch (feature_) {
          case SHD_VORONOI_DISTANCE_TO_EDGE: {
            MutableSpan<float> r_distance = get_r_distance(param++);
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              const float4 p = float4(vector[i].x, vector[i].y, vector[i].z, w[i]) * scale[i];
              noise::voronoi_distance_to_edge(p, rand, &r_distance[i]);
            }
            break;
          }
          case SHD_VORONOI_N_SPHERE_RADIUS: {
            MutableSpan<float> r_radius = get_r_radius(param++);
            for (int64_t i : mask) {
              const float rand = std::min(std::max(randomness[i], 0.0f), 1.0f);
              const float4 p = float4(vector[i].x, vector[i].y, vector[i].z, w[i]) * scale[i];
              noise::voronoi_n_sphere_radius(p, rand, &r_radius[i]);
            }
            break;
          }
        }
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
  bool minowski =
      (storage.distance == SHD_VORONOI_MINKOWSKI && storage.dimensions != 1 &&
       !ELEM(storage.feature, SHD_VORONOI_DISTANCE_TO_EDGE, SHD_VORONOI_N_SPHERE_RADIUS));
  bool dist_radius = ELEM(
      storage.feature, SHD_VORONOI_DISTANCE_TO_EDGE, SHD_VORONOI_N_SPHERE_RADIUS);
  if (dist_radius) {
    builder.construct_and_set_matching_fn<VoronoiEdgeFunction>(storage.dimensions,
                                                               storage.feature);
  }
  else if (minowski) {
    builder.construct_and_set_matching_fn<VoronoiMinowskiFunction>(storage.dimensions,
                                                                   storage.feature);
  }
  else {
    builder.construct_and_set_matching_fn<VoronoiMetricFunction>(
        storage.dimensions, storage.feature, storage.distance);
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
