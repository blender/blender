/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "IMB_colormanagement.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* ******************* Luma Matte Node ********************************* */

namespace blender::nodes::node_composite_luma_matte_cc {

NODE_STORAGE_FUNCS(NodeChroma)

static void cmp_node_luma_matte_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
  b.add_output<decl::Float>("Matte");
}

static void node_composit_init_luma_matte(bNodeTree * /*ntree*/, bNode *node)
{
  NodeChroma *c = MEM_cnew<NodeChroma>(__func__);
  node->storage = c;
  c->t1 = 1.0f;
  c->t2 = 0.0f;
}

static void node_composit_buts_luma_matte(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(
      col, ptr, "limit_max", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(
      col, ptr, "limit_min", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class LuminanceMatteShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float high = get_high();
    const float low = get_low();
    float luminance_coefficients[3];
    IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_luminance_matte",
                   inputs,
                   outputs,
                   GPU_uniform(&high),
                   GPU_uniform(&low),
                   GPU_constant(luminance_coefficients));
  }

  float get_high()
  {
    return node_storage(bnode()).t1;
  }

  float get_low()
  {
    return node_storage(bnode()).t2;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new LuminanceMatteShaderNode(node);
}

}  // namespace blender::nodes::node_composite_luma_matte_cc

void register_node_type_cmp_luma_matte()
{
  namespace file_ns = blender::nodes::node_composite_luma_matte_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_LUMA_MATTE, "Luminance Key", NODE_CLASS_MATTE);
  ntype.declare = file_ns::cmp_node_luma_matte_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_luma_matte;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_luma_matte;
  node_type_storage(&ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
}
