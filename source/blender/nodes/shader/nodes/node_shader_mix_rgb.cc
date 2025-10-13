/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BKE_material.hh"

#include "BLI_math_vector.h"

#include "DNA_material_types.h"

#include "NOD_multi_function.hh"

namespace blender::nodes::node_shader_mix_rgb_cc {

static void sh_node_mix_rgb_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("Factor", "Fac")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Color>("Color1").default_value({0.5f, 0.5f, 0.5f, 1.0f});
  b.add_input<decl::Color>("Color2").default_value({0.5f, 0.5f, 0.5f, 1.0f});
  b.add_output<decl::Color>("Color");
}

static const char *gpu_shader_get_name(int mode)
{
  switch (mode) {
    case MA_RAMP_BLEND:
      return "mix_blend";
    case MA_RAMP_ADD:
      return "mix_add";
    case MA_RAMP_MULT:
      return "mix_mult";
    case MA_RAMP_SUB:
      return "mix_sub";
    case MA_RAMP_SCREEN:
      return "mix_screen";
    case MA_RAMP_DIV:
      return "mix_div_fallback";
    case MA_RAMP_DIFF:
      return "mix_diff";
    case MA_RAMP_EXCLUSION:
      return "mix_exclusion";
    case MA_RAMP_DARK:
      return "mix_dark";
    case MA_RAMP_LIGHT:
      return "mix_light";
    case MA_RAMP_OVERLAY:
      return "mix_overlay";
    case MA_RAMP_DODGE:
      return "mix_dodge";
    case MA_RAMP_BURN:
      return "mix_burn";
    case MA_RAMP_HUE:
      return "mix_hue";
    case MA_RAMP_SAT:
      return "mix_sat";
    case MA_RAMP_VAL:
      return "mix_val";
    case MA_RAMP_COLOR:
      return "mix_color";
    case MA_RAMP_SOFT:
      return "mix_soft";
    case MA_RAMP_LINEAR:
      return "mix_linear";
  }

  return nullptr;
}

static int gpu_shader_mix_rgb(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData * /*execdata*/,
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  const char *name = gpu_shader_get_name(node->custom1);

  if (name == nullptr) {
    return 0;
  }

  const float min = 0.0f;
  const float max = 1.0f;
  const GPUNodeLink *factor_link = in[0].link ? in[0].link : GPU_uniform(in[0].vec);
  GPU_link(mat, "clamp_value", factor_link, GPU_constant(&min), GPU_constant(&max), &in[0].link);

  int ret = GPU_stack_link(mat, node, name, in, out);

  if (ret && node->custom2 & SHD_MIXRGB_CLAMP) {
    const float min[3] = {0.0f, 0.0f, 0.0f};
    const float max[3] = {1.0f, 1.0f, 1.0f};
    GPU_link(mat, "clamp_color", out[0].link, GPU_constant(min), GPU_constant(max), &out[0].link);
  }
  return ret;
}

class MixRGBFunction : public mf::MultiFunction {
 private:
  bool clamp_;
  int type_;

 public:
  MixRGBFunction(bool clamp, int type) : clamp_(clamp), type_(type)
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"MixRGB", signature};
      builder.single_input<float>("Fac");
      builder.single_input<ColorGeometry4f>("Color1");
      builder.single_input<ColorGeometry4f>("Color2");
      builder.single_output<ColorGeometry4f>("Color");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float> &fac = params.readonly_single_input<float>(0, "Fac");
    const VArray<ColorGeometry4f> &col1 = params.readonly_single_input<ColorGeometry4f>(1,
                                                                                        "Color1");
    const VArray<ColorGeometry4f> &col2 = params.readonly_single_input<ColorGeometry4f>(2,
                                                                                        "Color2");
    MutableSpan<ColorGeometry4f> results = params.uninitialized_single_output<ColorGeometry4f>(
        3, "Color");

    mask.foreach_index([&](const int64_t i) {
      results[i] = col1[i];
      ramp_blend(type_, results[i], clamp_f(fac[i], 0.0f, 1.0f), col2[i]);
    });

    if (clamp_) {
      mask.foreach_index([&](const int64_t i) { clamp_v4(results[i], 0.0f, 1.0f); });
    }
  }
};

static void sh_node_mix_rgb_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &node = builder.node();
  bool clamp = node.custom2 & SHD_MIXRGB_CLAMP;
  int mix_type = node.custom1;
  builder.construct_and_set_matching_fn<MixRGBFunction>(clamp, mix_type);
}

}  // namespace blender::nodes::node_shader_mix_rgb_cc

void register_node_type_sh_mix_rgb()
{
  namespace file_ns = blender::nodes::node_shader_mix_rgb_cc;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeMixRGB", SH_NODE_MIX_RGB_LEGACY);
  ntype.ui_name = "Mix (Legacy)";
  ntype.ui_description = "Mix two input colors";
  ntype.enum_name_legacy = "MIX_RGB";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::sh_node_mix_rgb_declare;
  ntype.labelfunc = node_blend_label;
  ntype.gpu_fn = file_ns::gpu_shader_mix_rgb;
  ntype.build_multi_function = file_ns::sh_node_mix_rgb_build_multi_function;
  ntype.gather_link_search_ops = nullptr;
  blender::bke::node_register_type(ntype);
}
