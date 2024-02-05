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

/* ******************* channel Distance Matte ********************************* */

namespace blender::nodes::node_composite_distance_matte_cc {

NODE_STORAGE_FUNCS(NodeChroma)

static void cmp_node_distance_matte_declare(NodeDeclarationBuilder &b)
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

static void node_composit_init_distance_matte(bNodeTree * /*ntree*/, bNode *node)
{
  NodeChroma *c = MEM_cnew<NodeChroma>(__func__);
  node->storage = c;
  c->channel = CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA;
  c->t1 = 0.1f;
  c->t2 = 0.1f;
}

static void node_composit_buts_distance_matte(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col, *row;

  col = uiLayoutColumn(layout, true);

  uiItemL(layout, IFACE_("Color Space:"), ICON_NONE);
  row = uiLayoutRow(layout, false);
  uiItemR(row, ptr, "channel", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  uiItemR(
      col, ptr, "tolerance", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(col, ptr, "falloff", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class DistanceMatteShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float tolerance = get_tolerance();
    const float falloff = get_falloff();

    if (get_color_space() == CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA) {
      GPU_stack_link(material,
                     &bnode(),
                     "node_composite_distance_matte_rgba",
                     inputs,
                     outputs,
                     GPU_uniform(&tolerance),
                     GPU_uniform(&falloff));
      return;
    }

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_distance_matte_ycca",
                   inputs,
                   outputs,
                   GPU_uniform(&tolerance),
                   GPU_uniform(&falloff));
  }

  CMPNodeDistanceMatteColorSpace get_color_space()
  {
    return (CMPNodeDistanceMatteColorSpace)node_storage(bnode()).channel;
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
  return new DistanceMatteShaderNode(node);
}

}  // namespace blender::nodes::node_composite_distance_matte_cc

void register_node_type_cmp_distance_matte()
{
  namespace file_ns = blender::nodes::node_composite_distance_matte_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DIST_MATTE, "Distance Key", NODE_CLASS_MATTE);
  ntype.declare = file_ns::cmp_node_distance_matte_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_distance_matte;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_distance_matte;
  node_type_storage(&ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
}
