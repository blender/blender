/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** INVERT ******************** */

namespace blender::nodes::node_composite_invert_cc {

static void cmp_node_invert_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(1);
  b.add_input<decl::Color>("Color")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Color");
}

static void node_composit_init_invert(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 |= CMP_CHAN_RGB;
}

static void node_composit_buts_invert(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "invert_rgb", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "invert_alpha", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class InvertShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float do_rgb = get_do_rgb();
    const float do_alpha = get_do_alpha();

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_invert",
                   inputs,
                   outputs,
                   GPU_constant(&do_rgb),
                   GPU_constant(&do_alpha));
  }

  bool get_do_rgb()
  {
    return bnode().custom1 & CMP_CHAN_RGB;
  }

  bool get_do_alpha()
  {
    return bnode().custom1 & CMP_CHAN_A;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new InvertShaderNode(node);
}

}  // namespace blender::nodes::node_composite_invert_cc

void register_node_type_cmp_invert()
{
  namespace file_ns = blender::nodes::node_composite_invert_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_INVERT, "Invert Color", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_invert_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_invert;
  ntype.initfunc = file_ns::node_composit_init_invert;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  blender::bke::nodeRegisterType(&ntype);
}
