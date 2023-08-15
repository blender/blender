/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.h"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** Brightness and Contrast  ******************** */

namespace blender::nodes::node_composite_brightness_cc {

static void cmp_node_brightcontrast_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Bright").min(-100.0f).max(100.0f).compositor_domain_priority(1);
  b.add_input<decl::Float>("Contrast").min(-100.0f).max(100.0f).compositor_domain_priority(2);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_brightcontrast(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = 1;
}

static void node_composit_buts_brightcontrast(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_premultiply", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class BrightContrastShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float use_premultiply = get_use_premultiply();

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_bright_contrast",
                   inputs,
                   outputs,
                   GPU_constant(&use_premultiply));
  }

  bool get_use_premultiply()
  {
    return bnode().custom1;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new BrightContrastShaderNode(node);
}

}  // namespace blender::nodes::node_composite_brightness_cc

void register_node_type_cmp_brightcontrast()
{
  namespace file_ns = blender::nodes::node_composite_brightness_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_BRIGHTCONTRAST, "Brightness/Contrast", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_brightcontrast_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_brightcontrast;
  ntype.initfunc = file_ns::node_composit_init_brightcontrast;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
}
