/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2018 Blender Foundation */

#include "node_shader_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_bsdf_hair_principled_cc {

/* Color, melanin and absorption coefficient default to approximately same brownish hair. */
static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({0.017513f, 0.005763f, 0.002059f, 1.0f});
  b.add_input<decl::Float>("Melanin").default_value(0.8f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Melanin Redness")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Color>("Tint").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Vector>("Absorption Coefficient")
      .default_value({0.245531f, 0.52f, 1.365f})
      .min(0.0f)
      .max(1000.0f);
  b.add_input<decl::Float>("Roughness")
      .default_value(0.3f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Radial Roughness")
      .default_value(0.3f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Coat").default_value(0.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Float>("IOR").default_value(1.55f).min(0.0f).max(1000.0f);
  b.add_input<decl::Float>("Offset")
      .default_value(2.0f * float(M_PI) / 180.0f)
      .min(-M_PI_2)
      .max(M_PI_2)
      .subtype(PROP_ANGLE);
  b.add_input<decl::Float>("Random Color")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Random Roughness")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Random").hide_value();
  b.add_input<decl::Float>("Weight").unavailable();
  b.add_output<decl::Shader>("BSDF");
}

static void node_shader_buts_principled_hair(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "parametrization", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

/* Initialize the custom Parametrization property to Color. */
static void node_shader_init_hair_principled(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_PRINCIPLED_HAIR_REFLECTANCE;
}

/* Triggers (in)visibility of some sockets when changing Parametrization. */
static void node_shader_update_hair_principled(bNodeTree *ntree, bNode *node)
{
  int parametrization = node->custom1;

  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (STREQ(sock->name, "Color")) {
      bke::nodeSetSocketAvailability(
          ntree, sock, parametrization == SHD_PRINCIPLED_HAIR_REFLECTANCE);
    }
    else if (STREQ(sock->name, "Melanin")) {
      bke::nodeSetSocketAvailability(
          ntree, sock, parametrization == SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION);
    }
    else if (STREQ(sock->name, "Melanin Redness")) {
      bke::nodeSetSocketAvailability(
          ntree, sock, parametrization == SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION);
    }
    else if (STREQ(sock->name, "Tint")) {
      bke::nodeSetSocketAvailability(
          ntree, sock, parametrization == SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION);
    }
    else if (STREQ(sock->name, "Absorption Coefficient")) {
      bke::nodeSetSocketAvailability(
          ntree, sock, parametrization == SHD_PRINCIPLED_HAIR_DIRECT_ABSORPTION);
    }
    else if (STREQ(sock->name, "Random Color")) {
      bke::nodeSetSocketAvailability(
          ntree, sock, parametrization == SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION);
    }
  }
}

static int node_shader_gpu_hair_principled(GPUMaterial *mat,
                                           bNode *node,
                                           bNodeExecData * /*execdata*/,
                                           GPUNodeStack *in,
                                           GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_bsdf_hair_principled", in, out);
}

}  // namespace blender::nodes::node_shader_bsdf_hair_principled_cc

/* node type definition */
void register_node_type_sh_bsdf_hair_principled()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_hair_principled_cc;

  static bNodeType ntype;

  sh_node_type_base(
      &ntype, SH_NODE_BSDF_HAIR_PRINCIPLED, "Principled Hair BSDF", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_cycles_shader_nodes_poll;
  ntype.draw_buttons = file_ns::node_shader_buts_principled_hair;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::LARGE);
  ntype.initfunc = file_ns::node_shader_init_hair_principled;
  ntype.updatefunc = file_ns::node_shader_update_hair_principled;
  ntype.gpu_fn = file_ns::node_shader_gpu_hair_principled;

  nodeRegisterType(&ntype);
}
