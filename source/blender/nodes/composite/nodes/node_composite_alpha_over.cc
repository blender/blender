/* SPDX-FileCopyrightText: 2006 Blender Foundation
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

/* **************** ALPHAOVER ******************** */

namespace blender::nodes::node_composite_alpha_over_cc {

NODE_STORAGE_FUNCS(NodeTwoFloats)

static void cmp_node_alphaover_declare(NodeDeclarationBuilder &b)
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

static void node_alphaover_init(bNodeTree * /*ntree*/, bNode *node)
{
  node->storage = MEM_cnew<NodeTwoFloats>(__func__);
}

static void node_composit_buts_alphaover(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_premultiply", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "premul", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class AlphaOverShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float premultiply_factor = get_premultiply_factor();
    if (premultiply_factor != 0.0f) {
      GPU_stack_link(material,
                     &bnode(),
                     "node_composite_alpha_over_mixed",
                     inputs,
                     outputs,
                     GPU_uniform(&premultiply_factor));
      return;
    }

    if (get_use_premultiply()) {
      GPU_stack_link(material, &bnode(), "node_composite_alpha_over_key", inputs, outputs);
      return;
    }

    GPU_stack_link(material, &bnode(), "node_composite_alpha_over_premultiply", inputs, outputs);
  }

  bool get_use_premultiply()
  {
    return bnode().custom1;
  }

  float get_premultiply_factor()
  {
    return node_storage(bnode()).x;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new AlphaOverShaderNode(node);
}

}  // namespace blender::nodes::node_composite_alpha_over_cc

void register_node_type_cmp_alphaover()
{
  namespace file_ns = blender::nodes::node_composite_alpha_over_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_ALPHAOVER, "Alpha Over", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_alphaover_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_alphaover;
  ntype.initfunc = file_ns::node_alphaover_init;
  node_type_storage(
      &ntype, "NodeTwoFloats", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
}
