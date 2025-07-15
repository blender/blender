/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "node_composite_util.hh"

/* ******************* Color Spill Suppression ********************************* */

namespace blender::nodes::node_composite_color_spill_cc {

NODE_STORAGE_FUNCS(NodeColorspill)

static void cmp_node_color_spill_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.use_custom_socket_order();

  b.add_output<decl::Color>("Image");

  b.add_layout([](uiLayout *layout, bContext * /*C*/, PointerRNA *ptr) {
    layout->label(IFACE_("Despill Channel:"), ICON_NONE);
    uiLayout *row = &layout->row(false);
    row->prop(
        ptr, "channel", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

    uiLayout *col = &layout->column(false);
    col->prop(ptr, "limit_method", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);

    if (RNA_enum_get(ptr, "limit_method") == 0) {
      col->label(IFACE_("Limiting Channel:"), ICON_NONE);
      row = &col->row(false);
      row->prop(ptr,
                "limit_channel",
                UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND,
                std::nullopt,
                ICON_NONE);
    }
  });

  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Fac").default_value(1.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Float>("Limit Strength")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(2.0f)
      .description("Specifies the limiting strength of the limit channel");

  PanelDeclarationBuilder &use_spill_strength_panel =
      b.add_panel("Spill Strength").default_closed(true);
  use_spill_strength_panel.add_input<decl::Bool>("Use Spill Strength")
      .default_value(false)
      .panel_toggle()
      .description(
          "If enabled, the spill strength for each color channel can be specified. If disabled, "
          "the spill channel will have a unit scale, while other channels will be zero");
  use_spill_strength_panel.add_input<decl::Color>("Strength", "Spill Strength")
      .default_value({0.0f, 1.0f, 0.0f, 1.0f})
      .description("Specifies the spilling strength of each color channel");
}

static void node_composit_init_color_spill(bNodeTree * /*ntree*/, bNode *node)
{
  NodeColorspill *ncs = MEM_callocN<NodeColorspill>(__func__);
  node->storage = ncs;
  node->custom2 = CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_SINGLE;
  node->custom1 = 2; /* green channel */
  ncs->limchan = 0;  /* limit by red */
}

using namespace blender::compositor;

/* Get the index of the channel used for spilling. */
static int get_spill_channel(const bNode &node)
{
  return node.custom1 - 1;
}

static CMPNodeColorSpillLimitAlgorithm get_limit_algorithm(const bNode &node)
{
  return static_cast<CMPNodeColorSpillLimitAlgorithm>(node.custom2);
}

/* Get the index of the channel used for limiting. */
static int get_limit_channel(const bNode &node)
{
  return node_storage(node).limchan;
}

/* Get the indices of the channels used to compute the limit value. We always assume the limit
 * algorithm is Average, if it is a single limit channel, store it in both limit channels,
 * because the average of two identical values is the same value. */
static int2 get_limit_channels(const bNode &node)
{
  int2 limit_channels;
  if (get_limit_algorithm(node) == CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_AVERAGE) {
    /* If the algorithm is Average, store the indices of the other two channels other than the
     * spill channel. */
    limit_channels[0] = (get_spill_channel(node) + 1) % 3;
    limit_channels[1] = (get_spill_channel(node) + 2) % 3;
  }
  else {
    /* If the algorithm is Single, store the index of the limit channel in both channels. */
    limit_channels[0] = get_limit_channel(node);
    limit_channels[1] = get_limit_channel(node);
  }

  return limit_channels;
}

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  const float spill_channel = get_spill_channel(*node);
  const float2 limit_channels = float2(get_limit_channels(*node));

  return GPU_stack_link(material,
                        node,
                        "node_composite_color_spill",
                        inputs,
                        outputs,
                        GPU_constant(&spill_channel),
                        GPU_constant(limit_channels));
}

static float3 compute_spill_scale(const bool &use_spill_strength,
                                  const float4 &spill_strength,
                                  const int spill_channel)
{
  if (use_spill_strength) {
    float3 scale = spill_strength.xyz();
    scale[spill_channel] *= -1.0f;
    return scale;
  }

  float3 scale = float3(0.0f);
  scale[spill_channel] = -1.0f;
  return scale;
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  const int spill_channel = get_spill_channel(builder.node());
  const float2 limit_channels = float2(get_limit_channels(builder.node()));

  builder.construct_and_set_matching_fn_cb([=]() {
    return mf::build::SI5_SO<float4, float, float, bool, float4, float4>(
        "Color Spill",
        [=](const float4 &color,
            const float &factor,
            const float &limit_scale,
            const bool &use_spill_strength,
            const float4 &spill_strength) -> float4 {
          float average_limit = (color[limit_channels.x] + color[limit_channels.y]) / 2.0f;
          float map = factor * color[spill_channel] - limit_scale * average_limit;
          float3 spill_scale = compute_spill_scale(
              use_spill_strength, spill_strength, spill_channel);
          return float4(map > 0.0f ? color.xyz() + spill_scale * map : color.xyz(), color.w);
        },
        mf::build::exec_presets::SomeSpanOrSingle<0>());
  });
}

}  // namespace blender::nodes::node_composite_color_spill_cc

static void register_node_type_cmp_color_spill()
{
  namespace file_ns = blender::nodes::node_composite_color_spill_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeColorSpill", CMP_NODE_COLOR_SPILL);
  ntype.ui_name = "Color Spill";
  ntype.ui_description =
      "Remove colors from a blue or green screen, by reducing one RGB channel compared to the "
      "others";
  ntype.enum_name_legacy = "COLOR_SPILL";
  ntype.nclass = NODE_CLASS_MATTE;
  ntype.declare = file_ns::cmp_node_color_spill_declare;
  ntype.initfunc = file_ns::node_composit_init_color_spill;
  blender::bke::node_type_storage(
      ntype, "NodeColorspill", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;
  blender::bke::node_type_size(ntype, 160, 140, NODE_DEFAULT_MAX_WIDTH);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_color_spill)
