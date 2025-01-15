/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_npr_image_sample_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Image>("Image").hide_value();
  b.add_input<decl::Vector>("Offset").hide_value();
  b.add_output<decl::Color>("Color");
}

static void node_shader_buts(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout,
          ptr,
          "offset_type",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND,
          std::nullopt,
          ICON_NONE);
}

static int node_shader_fn(GPUMaterial *mat,
                          bNode *node,
                          bNodeExecData * /*execdata*/,
                          GPUNodeStack *in,
                          GPUNodeStack *out)
{
  return GPU_stack_link(
      mat, node, node->custom1 ? "npr_image_sample_texel" : "npr_image_sample_view", in, out);
}

}  // namespace blender::nodes::node_shader_npr_image_sample_cc

void register_node_type_sh_npr_image_sample()
{
  namespace file_ns = blender::nodes::node_shader_npr_image_sample_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeNPR_ImageSample", SH_NODE_NPR_IMAGE_SAMPLE);
  ntype.enum_name_legacy = "NPR_IMAGE_SAMPLE";
  ntype.ui_name = "Image Sample";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts;
  ntype.add_ui_poll = npr_shader_nodes_poll;
  ntype.gpu_fn = file_ns::node_shader_fn;

  blender::bke::node_register_type(&ntype);
}
