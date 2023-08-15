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

/* **************** Premul and Key Alpha Convert ******************** */

namespace blender::nodes::node_composite_premulkey_cc {

static void cmp_node_premulkey_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void node_composit_buts_premulkey(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mapping", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::realtime_compositor;

class AlphaConvertShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    if (get_mode() == 0) {
      GPU_stack_link(material, &bnode(), "color_alpha_premultiply", inputs, outputs);
      return;
    }

    GPU_stack_link(material, &bnode(), "color_alpha_unpremultiply", inputs, outputs);
  }

  CMPNodeAlphaConvertMode get_mode()
  {
    return (CMPNodeAlphaConvertMode)bnode().custom1;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new AlphaConvertShaderNode(node);
}

}  // namespace blender::nodes::node_composite_premulkey_cc

void register_node_type_cmp_premulkey()
{
  namespace file_ns = blender::nodes::node_composite_premulkey_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_PREMULKEY, "Alpha Convert", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_premulkey_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_premulkey;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
}
