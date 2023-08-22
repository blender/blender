/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_bsdf_sheen_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Float>("Roughness")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Vector>("Normal").hide_value();
  b.add_input<decl::Float>("Weight").unavailable();
  b.add_output<decl::Shader>("BSDF");
}

static void node_shader_buts_sheen(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distribution", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_sheen(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_SHEEN_MICROFIBER;
}

static int node_shader_gpu_bsdf_sheen(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  if (!in[2].link) {
    GPU_link(mat, "world_normals_get", &in[2].link);
  }

  GPU_material_flag_set(mat, GPU_MATFLAG_DIFFUSE);

  return GPU_stack_link(mat, node, "node_bsdf_sheen", in, out);
}

}  // namespace blender::nodes::node_shader_bsdf_sheen_cc

/* node type definition */
void register_node_type_sh_bsdf_sheen()
{
  namespace file_ns = blender::nodes::node_shader_bsdf_sheen_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BSDF_SHEEN, "Sheen BSDF", NODE_CLASS_SHADER);
  ntype.add_ui_poll = object_cycles_shader_nodes_poll;
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_shader_init_sheen;
  ntype.gpu_fn = file_ns::node_shader_gpu_bsdf_sheen;
  ntype.draw_buttons = file_ns::node_shader_buts_sheen;

  nodeRegisterType(&ntype);
}
