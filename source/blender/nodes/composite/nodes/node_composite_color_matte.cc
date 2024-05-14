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

/* ******************* Color Matte ********************************************************** */

namespace blender::nodes::node_composite_color_matte_cc {

NODE_STORAGE_FUNCS(NodeChroma)

static void cmp_node_color_matte_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Color>("Key Color")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
  b.add_output<decl::Float>("Matte");
}

static void node_composit_init_color_matte(bNodeTree * /*ntree*/, bNode *node)
{
  NodeChroma *c = MEM_cnew<NodeChroma>(__func__);
  node->storage = c;
  c->t1 = 0.01f;
  c->t2 = 0.1f;
  c->t3 = 0.1f;
  c->fsize = 0.0f;
  c->fstrength = 1.0f;
}

static void node_composit_buts_color_matte(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(
      col, ptr, "color_hue", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(col,
          ptr,
          "color_saturation",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          nullptr,
          ICON_NONE);
  uiItemR(
      col, ptr, "color_value", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class ColorMatteShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float hue_epsilon = get_hue_epsilon();
    const float saturation_epsilon = get_saturation_epsilon();
    const float value_epsilon = get_value_epsilon();

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_color_matte",
                   inputs,
                   outputs,
                   GPU_uniform(&hue_epsilon),
                   GPU_uniform(&saturation_epsilon),
                   GPU_uniform(&value_epsilon));
  }

  float get_hue_epsilon()
  {
    /* Divide by 2 because the hue wraps around. */
    return node_storage(bnode()).t1 / 2.0f;
  }

  float get_saturation_epsilon()
  {
    return node_storage(bnode()).t2;
  }

  float get_value_epsilon()
  {
    return node_storage(bnode()).t3;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new ColorMatteShaderNode(node);
}

}  // namespace blender::nodes::node_composite_color_matte_cc

void register_node_type_cmp_color_matte()
{
  namespace file_ns = blender::nodes::node_composite_color_matte_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_COLOR_MATTE, "Color Key", NODE_CLASS_MATTE);
  ntype.declare = file_ns::cmp_node_color_matte_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_color_matte;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_color_matte;
  blender::bke::node_type_storage(
      &ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  blender::bke::nodeRegisterType(&ntype);
}
