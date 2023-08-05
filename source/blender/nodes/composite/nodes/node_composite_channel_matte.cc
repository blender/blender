/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "RNA_access.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.h"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* ******************* Channel Matte Node ********************************* */

namespace blender::nodes::node_composite_channel_matte_cc {

NODE_STORAGE_FUNCS(NodeChroma)

static void cmp_node_channel_matte_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
  b.add_output<decl::Float>("Matte");
}

static void node_composit_init_channel_matte(bNodeTree * /*ntree*/, bNode *node)
{
  NodeChroma *c = MEM_cnew<NodeChroma>(__func__);
  node->storage = c;
  c->t1 = 1.0f;
  c->t2 = 0.0f;
  c->t3 = 0.0f;
  c->fsize = 0.0f;
  c->fstrength = 0.0f;
  c->algorithm = 1;  /* Max channel limiting. */
  c->channel = 1;    /* Limit by red. */
  node->custom1 = 1; /* RGB channel. */
  node->custom2 = 2; /* Green Channel. */
}

static void node_composit_buts_channel_matte(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col, *row;

  uiItemL(layout, IFACE_("Color Space:"), ICON_NONE);
  row = uiLayoutRow(layout, false);
  uiItemR(
      row, ptr, "color_space", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiItemL(col, IFACE_("Key Channel:"), ICON_NONE);
  row = uiLayoutRow(col, false);
  uiItemR(row,
          ptr,
          "matte_channel",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND,
          nullptr,
          ICON_NONE);

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

  uiItemR(
      col, ptr, "limit_max", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(
      col, ptr, "limit_min", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class ChannelMatteShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float color_space = get_color_space();
    const float matte_channel = get_matte_channel();
    float limit_channels[2];
    get_limit_channels(limit_channels);
    const float max_limit = get_max_limit();
    const float min_limit = get_min_limit();

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_channel_matte",
                   inputs,
                   outputs,
                   GPU_constant(&color_space),
                   GPU_constant(&matte_channel),
                   GPU_constant(limit_channels),
                   GPU_uniform(&max_limit),
                   GPU_uniform(&min_limit));
  }

  /* 1 -> CMP_NODE_CHANNEL_MATTE_CS_RGB
   * 2 -> CMP_NODE_CHANNEL_MATTE_CS_HSV
   * 3 -> CMP_NODE_CHANNEL_MATTE_CS_YUV
   * 4 -> CMP_NODE_CHANNEL_MATTE_CS_YCC */
  int get_color_space()
  {
    return bnode().custom1;
  }

  /* Get the index of the channel used to generate the matte. */
  int get_matte_channel()
  {
    return bnode().custom2 - 1;
  }

  /* Get the index of the channel used to compute the limit value. */
  int get_limit_channel()
  {
    return node_storage(bnode()).channel - 1;
  }

  /* Get the indices of the channels used to compute the limit value. We always assume the limit
   * algorithm is Max, if it is a single limit channel, store it in both limit channels, because
   * the maximum of two identical values is the same value. */
  void get_limit_channels(float limit_channels[2])
  {
    if (node_storage(bnode()).algorithm == CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_MAX) {
      /* If the algorithm is Max, store the indices of the other two channels other than the matte
       * channel. */
      limit_channels[0] = (get_matte_channel() + 1) % 3;
      limit_channels[1] = (get_matte_channel() + 2) % 3;
    }
    else {
      /* If the algorithm is Single, store the index of the limit channel in both channels. */
      limit_channels[0] = get_limit_channel();
      limit_channels[1] = get_limit_channel();
    }
  }

  float get_max_limit()
  {
    return node_storage(bnode()).t1;
  }

  float get_min_limit()
  {
    return node_storage(bnode()).t2;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new ChannelMatteShaderNode(node);
}

}  // namespace blender::nodes::node_composite_channel_matte_cc

void register_node_type_cmp_channel_matte()
{
  namespace file_ns = blender::nodes::node_composite_channel_matte_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CHANNEL_MATTE, "Channel Key", NODE_CLASS_MATTE);
  ntype.declare = file_ns::cmp_node_channel_matte_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_channel_matte;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_channel_matte;
  node_type_storage(&ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
}
