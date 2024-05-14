/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_bsdf_hair_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Float>("Offset").default_value(0.0f).min(-M_PI_2).max(M_PI_2).subtype(
      PROP_ANGLE);
  b.add_input<decl::Float>("RoughnessU")
      .default_value(0.1f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>("RoughnessV")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Vector>("Tangent").hide_value();
  b.add_input<decl::Float>("Weight").unavailable();
  b.add_output<decl::Shader>("BSDF");
}

static void node_shader_buts_hair(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "component", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static int node_shader_gpu_bsdf_hair(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  GPU_material_flag_set(mat, GPU_MATFLAG_DIFFUSE | GPU_MATFLAG_GLOSSY);

  return GPU_stack_link(mat, node, "node_bsdf_hair", in, out);
}

}  // namespace blender::nodes::node_shader_bsdf_hair_cc

/* node type definition */
void register_node_type_sh_bsdf_hair()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_hair_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BSDF_HAIR, "Hair BSDF", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_cycles_shader_nodes_poll;
  ntype.draw_buttons = file_ns::node_shader_buts_hair;
  blender::bke::node_type_size(&ntype, 150, 60, 200);
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_hair;

  blender::bke::nodeRegisterType(&ntype);
}
