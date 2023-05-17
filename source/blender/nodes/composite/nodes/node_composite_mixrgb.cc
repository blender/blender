/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"

#include "DNA_material_types.h"

#include "GPU_material.h"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** MIX RGB ******************** */

namespace blender::nodes::node_composite_mixrgb_cc {

static void cmp_node_mixrgb_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(2);
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Color>("Image", "Image_001")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
}

using namespace blender::realtime_compositor;

class MixRGBShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    if (get_use_alpha()) {
      GPU_link(material,
               "multiply_by_alpha",
               get_input_link("Fac"),
               get_input_link("Image_001"),
               &get_input("Fac").link);
    }

    GPU_stack_link(material, &bnode(), get_shader_function_name(), inputs, outputs);

    if (!get_should_clamp()) {
      return;
    }

    const float min[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float max[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    GPU_link(material,
             "clamp_color",
             get_output("Image").link,
             GPU_constant(min),
             GPU_constant(max),
             &get_output("Image").link);
  }

  int get_mode()
  {
    return bnode().custom1;
  }

  const char *get_shader_function_name()
  {
    switch (get_mode()) {
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

    BLI_assert_unreachable();
    return nullptr;
  }

  bool get_use_alpha()
  {
    return bnode().custom2 & SHD_MIXRGB_USE_ALPHA;
  }

  bool get_should_clamp()
  {
    return bnode().custom2 & SHD_MIXRGB_CLAMP;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new MixRGBShaderNode(node);
}

}  // namespace blender::nodes::node_composite_mixrgb_cc

void register_node_type_cmp_mix_rgb()
{
  namespace file_ns = blender::nodes::node_composite_mixrgb_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MIX_RGB, "Mix", NODE_CLASS_OP_COLOR);
  ntype.flag |= NODE_PREVIEW;
  ntype.declare = file_ns::cmp_node_mixrgb_declare;
  ntype.labelfunc = node_blend_label;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
}
