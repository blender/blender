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

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.h"

namespace blender::nodes::node_shader_mix_rgb_cc {

static void sh_node_mix_rgb_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>(N_("Fac")).default_value(0.5f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Color>(N_("Color1")).default_value({0.5f, 0.5f, 0.5f, 1.0f});
  b.add_input<decl::Color>(N_("Color2")).default_value({0.5f, 0.5f, 0.5f, 1.0f});
  b.add_output<decl::Color>(N_("Color"));
};

static void node_shader_exec_mix_rgb(void *UNUSED(data),
                                     int UNUSED(thread),
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
                                     bNodeStack **in,
                                     bNodeStack **out)
{
  /* stack order in: fac, col1, col2 */
  /* stack order out: col */
  float col[3];
  float fac;
  float vec[3];

  nodestack_get_vec(&fac, SOCK_FLOAT, in[0]);
  CLAMP(fac, 0.0f, 1.0f);

  nodestack_get_vec(col, SOCK_VECTOR, in[1]);
  nodestack_get_vec(vec, SOCK_VECTOR, in[2]);

  ramp_blend(node->custom1, col, fac, vec);
  if (node->custom2 & SHD_MIXRGB_CLAMP) {
    CLAMP3(col, 0.0f, 1.0f);
  }
  copy_v3_v3(out[0]->vec, col);
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
      return "mix_div";
    case MA_RAMP_DIFF:
      return "mix_diff";
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
                              bNodeExecData *UNUSED(execdata),
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  const char *name = gpu_shader_get_name(node->custom1);

  if (name != nullptr) {
    int ret = GPU_stack_link(mat, node, name, in, out);
    if (ret && node->custom2 & SHD_MIXRGB_CLAMP) {
      const float min[3] = {0.0f, 0.0f, 0.0f};
      const float max[3] = {1.0f, 1.0f, 1.0f};
      GPU_link(
          mat, "clamp_color", out[0].link, GPU_constant(min), GPU_constant(max), &out[0].link);
    }
    return ret;
  }

  return 0;
}

class MixRGBFunction : public blender::fn::MultiFunction {
 private:
  bool clamp_;
  int type_;

 public:
  MixRGBFunction(bool clamp, int type) : clamp_(clamp), type_(type)
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"MixRGB"};
    signature.single_input<float>("Fac");
    signature.single_input<blender::ColorGeometry4f>("Color1");
    signature.single_input<blender::ColorGeometry4f>("Color2");
    signature.single_output<blender::ColorGeometry4f>("Color");
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<float> &fac = params.readonly_single_input<float>(0, "Fac");
    const blender::VArray<blender::ColorGeometry4f> &col1 =
        params.readonly_single_input<blender::ColorGeometry4f>(1, "Color1");
    const blender::VArray<blender::ColorGeometry4f> &col2 =
        params.readonly_single_input<blender::ColorGeometry4f>(2, "Color2");
    blender::MutableSpan<blender::ColorGeometry4f> results =
        params.uninitialized_single_output<blender::ColorGeometry4f>(3, "Color");

    for (int64_t i : mask) {
      results[i] = col1[i];
      ramp_blend(type_, results[i], clamp_f(fac[i], 0.0f, 1.0f), col2[i]);
    }

    if (clamp_) {
      for (int64_t i : mask) {
        clamp_v3(results[i], 0.0f, 1.0f);
      }
    }
  }
};

static void sh_node_mix_rgb_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  bNode &node = builder.node();
  bool clamp = node.custom2 & SHD_MIXRGB_CLAMP;
  int mix_type = node.custom1;
  builder.construct_and_set_matching_fn<MixRGBFunction>(clamp, mix_type);
}

}  // namespace blender::nodes::node_shader_mix_rgb_cc

void register_node_type_sh_mix_rgb()
{
  namespace file_ns = blender::nodes::node_shader_mix_rgb_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_MIX_RGB, "Mix", NODE_CLASS_OP_COLOR, 0);
  ntype.declare = file_ns::sh_node_mix_rgb_declare;
  ntype.labelfunc = node_blend_label;
  node_type_exec(&ntype, nullptr, nullptr, file_ns::node_shader_exec_mix_rgb);
  node_type_gpu(&ntype, file_ns::gpu_shader_mix_rgb);
  ntype.build_multi_function = file_ns::sh_node_mix_rgb_build_multi_function;

  nodeRegisterType(&ntype);
}
