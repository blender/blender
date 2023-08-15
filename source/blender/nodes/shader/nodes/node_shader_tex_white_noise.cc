/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BLI_noise.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_tex_white_noise_cc {

static void sh_node_tex_white_noise_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").min(-10000.0f).max(10000.0f).implicit_field(
      implicit_field_inputs::position);
  b.add_input<decl::Float>("W").min(-10000.0f).max(10000.0f).make_available([](bNode &node) {
    /* Default to 1 instead of 4, because it is faster. */
    node.custom1 = 1;
  });
  b.add_output<decl::Float>("Value");
  b.add_output<decl::Color>("Color");
}

static void node_shader_buts_white_noise(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "noise_dimensions", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_tex_white_noise(bNodeTree * /*ntree*/, bNode *node)
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
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  const char *name = gpu_shader_get_name(node->custom1);
  return GPU_stack_link(mat, node, name, in, out);
}

static void node_shader_update_tex_white_noise(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sockVector = nodeFindSocket(node, SOCK_IN, "Vector");
  bNodeSocket *sockW = nodeFindSocket(node, SOCK_IN, "W");

  bke::nodeSetSocketAvailability(ntree, sockVector, node->custom1 != 1);
  bke::nodeSetSocketAvailability(ntree, sockW, node->custom1 == 1 || node->custom1 == 4);
}

class WhiteNoiseFunction : public mf::MultiFunction {
 private:
  int dimensions_;

 public:
  WhiteNoiseFunction(int dimensions) : dimensions_(dimensions)
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
    mf::SignatureBuilder builder{"WhiteNoise", signature};

    if (ELEM(dimensions, 2, 3, 4)) {
      builder.single_input<float3>("Vector");
    }
    if (ELEM(dimensions, 1, 4)) {
      builder.single_input<float>("W");
    }

    builder.single_output<float>("Value", mf::ParamFlag::SupportsUnusedOutput);
    builder.single_output<ColorGeometry4f>("Color", mf::ParamFlag::SupportsUnusedOutput);

    return signature;
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
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
          mask.foreach_index([&](const int64_t i) {
            const float3 c = noise::hash_float_to_float3(w[i]);
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          });
        }
        if (compute_value) {
          mask.foreach_index(
              [&](const int64_t i) { r_value[i] = noise::hash_float_to_float(w[i]); });
        }
        break;
      }
      case 2: {
        const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
        if (compute_color) {
          mask.foreach_index([&](const int64_t i) {
            const float3 c = noise::hash_float_to_float3(float2(vector[i].x, vector[i].y));
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          });
        }
        if (compute_value) {
          mask.foreach_index([&](const int64_t i) {
            r_value[i] = noise::hash_float_to_float(float2(vector[i].x, vector[i].y));
          });
        }
        break;
      }
      case 3: {
        const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
        if (compute_color) {
          mask.foreach_index([&](const int64_t i) {
            const float3 c = noise::hash_float_to_float3(vector[i]);
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          });
        }
        if (compute_value) {
          mask.foreach_index(
              [&](const int64_t i) { r_value[i] = noise::hash_float_to_float(vector[i]); });
        }
        break;
      }
      case 4: {
        const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
        const VArray<float> &w = params.readonly_single_input<float>(1, "W");
        if (compute_color) {
          mask.foreach_index([&](const int64_t i) {
            const float3 c = noise::hash_float_to_float3(
                float4(vector[i].x, vector[i].y, vector[i].z, w[i]));
            r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
          });
        }
        if (compute_value) {
          mask.foreach_index([&](const int64_t i) {
            r_value[i] = noise::hash_float_to_float(
                float4(vector[i].x, vector[i].y, vector[i].z, w[i]));
          });
        }
        break;
      }
    }
  }
};

static void sh_node_noise_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &node = builder.node();
  builder.construct_and_set_matching_fn<WhiteNoiseFunction>(int(node.custom1));
}

}  // namespace blender::nodes::node_shader_tex_white_noise_cc

void register_node_type_sh_tex_white_noise()
{
  namespace file_ns = blender::nodes::node_shader_tex_white_noise_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_TEX_WHITE_NOISE, "White Noise Texture", NODE_CLASS_TEXTURE);
  ntype.declare = file_ns::sh_node_tex_white_noise_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_white_noise;
  ntype.initfunc = file_ns::node_shader_init_tex_white_noise;
  ntype.gpu_fn = file_ns::gpu_shader_tex_white_noise;
  ntype.updatefunc = file_ns::node_shader_update_tex_white_noise;
  ntype.build_multi_function = file_ns::sh_node_noise_build_multi_function;

  nodeRegisterType(&ntype);
}
