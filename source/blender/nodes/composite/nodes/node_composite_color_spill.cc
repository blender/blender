/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.h"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* ******************* Color Spill Suppression ********************************* */

namespace blender::nodes::node_composite_color_spill_cc {

NODE_STORAGE_FUNCS(NodeColorspill)

static void cmp_node_color_spill_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_color_spill(bNodeTree * /*ntree*/, bNode *node)
{
  NodeColorspill *ncs = MEM_cnew<NodeColorspill>(__func__);
  node->storage = ncs;
  node->custom2 = CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_SINGLE;
  node->custom1 = 2;    /* green channel */
  ncs->limchan = 0;     /* limit by red */
  ncs->limscale = 1.0f; /* limit scaling factor */
  ncs->unspill = 0;     /* do not use unspill */
}

static void node_composit_buts_color_spill(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *row, *col;

  uiItemL(layout, IFACE_("Despill Channel:"), ICON_NONE);
  row = uiLayoutRow(layout, false);
  uiItemR(row, ptr, "channel", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "limit_method", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  if (RNA_enum_get(ptr, "limit_method") == 0) {
    uiItemL(col, IFACE_("Limiting Channel:"), ICON_NONE);
    row = uiLayoutRow(col, false);
    uiItemR(row,
            ptr,
            "limit_channel",
            UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND,
            nullptr,
            ICON_NONE);
  }

  uiItemR(col, ptr, "ratio", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(col, ptr, "use_unspill", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  if (RNA_boolean_get(ptr, "use_unspill") == true) {
    uiItemR(col,
            ptr,
            "unspill_red",
            UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
            nullptr,
            ICON_NONE);
    uiItemR(col,
            ptr,
            "unspill_green",
            UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
            nullptr,
            ICON_NONE);
    uiItemR(col,
            ptr,
            "unspill_blue",
            UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
            nullptr,
            ICON_NONE);
  }
}

using namespace blender::realtime_compositor;

class ColorSpillShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float spill_channel = get_spill_channel();
    float spill_scale[3];
    get_spill_scale(spill_scale);
    float limit_channels[2];
    get_limit_channels(limit_channels);
    const float limit_scale = get_limit_scale();

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_color_spill",
                   inputs,
                   outputs,
                   GPU_constant(&spill_channel),
                   GPU_uniform(spill_scale),
                   GPU_constant(limit_channels),
                   GPU_uniform(&limit_scale));
  }

  /* Get the index of the channel used for spilling. */
  int get_spill_channel()
  {
    return bnode().custom1 - 1;
  }

  CMPNodeColorSpillLimitAlgorithm get_limit_algorithm()
  {
    return (CMPNodeColorSpillLimitAlgorithm)bnode().custom2;
  }

  void get_spill_scale(float spill_scale[3])
  {
    const NodeColorspill &node_color_spill = node_storage(bnode());
    if (node_color_spill.unspill) {
      spill_scale[0] = node_color_spill.uspillr;
      spill_scale[1] = node_color_spill.uspillg;
      spill_scale[2] = node_color_spill.uspillb;
      spill_scale[get_spill_channel()] *= -1.0f;
    }
    else {
      spill_scale[0] = 0.0f;
      spill_scale[1] = 0.0f;
      spill_scale[2] = 0.0f;
      spill_scale[get_spill_channel()] = -1.0f;
    }
  }

  /* Get the index of the channel used for limiting. */
  int get_limit_channel()
  {
    return node_storage(bnode()).limchan;
  }

  /* Get the indices of the channels used to compute the limit value. We always assume the limit
   * algorithm is Average, if it is a single limit channel, store it in both limit channels,
   * because the average of two identical values is the same value. */
  void get_limit_channels(float limit_channels[2])
  {
    if (get_limit_algorithm() == CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_AVERAGE) {
      /* If the algorithm is Average, store the indices of the other two channels other than the
       * spill channel. */
      limit_channels[0] = (get_spill_channel() + 1) % 3;
      limit_channels[1] = (get_spill_channel() + 2) % 3;
    }
    else {
      /* If the algorithm is Single, store the index of the limit channel in both channels. */
      limit_channels[0] = get_limit_channel();
      limit_channels[1] = get_limit_channel();
    }
  }

  float get_limit_scale()
  {
    return node_storage(bnode()).limscale;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new ColorSpillShaderNode(node);
}

}  // namespace blender::nodes::node_composite_color_spill_cc

void register_node_type_cmp_color_spill()
{
  namespace file_ns = blender::nodes::node_composite_color_spill_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_COLOR_SPILL, "Color Spill", NODE_CLASS_MATTE);
  ntype.declare = file_ns::cmp_node_color_spill_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_color_spill;
  ntype.initfunc = file_ns::node_composit_init_color_spill;
  node_type_storage(
      &ntype, "NodeColorspill", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
}
