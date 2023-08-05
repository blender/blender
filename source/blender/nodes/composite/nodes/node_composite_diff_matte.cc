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

/* ******************* channel Difference Matte ********************************* */

namespace blender::nodes::node_composite_diff_matte_cc {

NODE_STORAGE_FUNCS(NodeChroma)

static void cmp_node_diff_matte_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image 1")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Color>("Image 2")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
  b.add_output<decl::Float>("Matte");
}

static void node_composit_init_diff_matte(bNodeTree * /*ntree*/, bNode *node)
{
  NodeChroma *c = MEM_cnew<NodeChroma>(__func__);
  node->storage = c;
  c->t1 = 0.1f;
  c->t2 = 0.1f;
}

static void node_composit_buts_diff_matte(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(
      col, ptr, "tolerance", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(col, ptr, "falloff", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class DifferenceMatteShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float tolerance = get_tolerance();
    const float falloff = get_falloff();

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_difference_matte",
                   inputs,
                   outputs,
                   GPU_uniform(&tolerance),
                   GPU_uniform(&falloff));
  }

  float get_tolerance()
  {
    return node_storage(bnode()).t1;
  }

  float get_falloff()
  {
    return node_storage(bnode()).t2;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new DifferenceMatteShaderNode(node);
}

}  // namespace blender::nodes::node_composite_diff_matte_cc

void register_node_type_cmp_diff_matte()
{
  namespace file_ns = blender::nodes::node_composite_diff_matte_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DIFF_MATTE, "Difference Key", NODE_CLASS_MATTE);
  ntype.declare = file_ns::cmp_node_diff_matte_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_diff_matte;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_diff_matte;
  node_type_storage(&ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
}
