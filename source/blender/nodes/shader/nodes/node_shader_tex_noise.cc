/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

#include "node_shader_util.hh"

#include "BLI_noise.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_tex_noise_cc {

NODE_STORAGE_FUNCS(NodeTexNoise)

static void sh_node_tex_noise_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").implicit_field(implicit_field_inputs::position);
  b.add_input<decl::Float>("W").min(-1000.0f).max(1000.0f).make_available([](bNode &node) {
    /* Default to 1 instead of 4, because it is much faster. */
    node_storage(node).dimensions = 1;
  });
  b.add_input<decl::Float>("Scale").min(-1000.0f).max(1000.0f).default_value(5.0f);
  b.add_input<decl::Float>("Detail").min(0.0f).max(15.0f).default_value(2.0f);
  b.add_input<decl::Float>("Roughness")
      .min(0.0f)
      .max(1.0f)
      .default_value(0.5f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Distortion").min(-1000.0f).max(1000.0f).default_value(0.0f);
  b.add_output<decl::Float>("Fac").no_muted_links();
  b.add_output<decl::Color>("Color").no_muted_links();
}

static void node_shader_buts_tex_noise(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "noise_dimensions", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_tex_noise(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexNoise *tex = MEM_cnew<NodeTexNoise>(__func__);
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
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  const NodeTexNoise &storage = node_storage(*node);
  const char *name = gpu_shader_get_name(storage.dimensions);
  return GPU_stack_link(mat, node, name, in, out);
}

static void node_shader_update_tex_noise(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sockVector = nodeFindSocket(node, SOCK_IN, "Vector");
  bNodeSocket *sockW = nodeFindSocket(node, SOCK_IN, "W");

  const NodeTexNoise &storage = node_storage(*node);
  bke::nodeSetSocketAvailability(ntree, sockVector, storage.dimensions != 1);
  bke::nodeSetSocketAvailability(ntree, sockW, storage.dimensions == 1 || storage.dimensions == 4);
}

class NoiseFunction : public mf::MultiFunction {
 private:
  int dimensions_;

 public:
  NoiseFunction(int dimensions) : dimensions_(dimensions)
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
            r_factor[i] = noise::perlin_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
          });
        }
        if (compute_color) {
          mask.foreach_index([&](const int64_t i) {
            const float position = w[i] * scale[i];
            const float3 c = noise::perlin_float3_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
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
            r_factor[i] = noise::perlin_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
          });
        }
        if (compute_color) {
          mask.foreach_index([&](const int64_t i) {
            const float2 position = float2(vector[i] * scale[i]);
            const float3 c = noise::perlin_float3_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
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
            r_factor[i] = noise::perlin_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
          });
        }
        if (compute_color) {
          mask.foreach_index([&](const int64_t i) {
            const float3 position = vector[i] * scale[i];
            const float3 c = noise::perlin_float3_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
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
            r_factor[i] = noise::perlin_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
          });
        }
        if (compute_color) {
          mask.foreach_index([&](const int64_t i) {
            const float3 position_vector = vector[i] * scale[i];
            const float position_w = w[i] * scale[i];
            const float4 position{
                position_vector[0], position_vector[1], position_vector[2], position_w};
            const float3 c = noise::perlin_float3_fractal_distorted(
                position, detail[i], roughness[i], distortion[i]);
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
  builder.construct_and_set_matching_fn<NoiseFunction>(storage.dimensions);
}

}  // namespace blender::nodes::node_shader_tex_noise_cc

void register_node_type_sh_tex_noise()
{
  namespace file_ns = blender::nodes::node_shader_tex_noise_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_TEX_NOISE, "Noise Texture", NODE_CLASS_TEXTURE);
  ntype.declare = file_ns::sh_node_tex_noise_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_noise;
  ntype.initfunc = file_ns::node_shader_init_tex_noise;
  node_type_storage(
      &ntype, "NodeTexNoise", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_noise;
  ntype.updatefunc = file_ns::node_shader_update_tex_noise;
  ntype.build_multi_function = file_ns::sh_node_noise_build_multi_function;

  nodeRegisterType(&ntype);
}
