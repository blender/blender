/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BKE_node.hh"
#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "node_composite_util.hh"

/* ******************* Channel Matte Node ********************************* */

namespace blender::nodes::node_composite_channel_matte_cc {

NODE_STORAGE_FUNCS(NodeChroma)

static void cmp_node_channel_matte_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Minimum")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description("Channel values lower than this minimum are keyed");
  b.add_input<decl::Float>("Maximum")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description("Channel values higher than this maximum are not keyed");

  b.add_output<decl::Color>("Image");
  b.add_output<decl::Float>("Matte");
}

static void node_composit_init_channel_matte(bNodeTree * /*ntree*/, bNode *node)
{
  NodeChroma *c = MEM_callocN<NodeChroma>(__func__);
  node->storage = c;
  c->algorithm = 1;  /* Max channel limiting. */
  c->channel = 1;    /* Limit by red. */
  node->custom1 = 1; /* RGB channel. */
  node->custom2 = 2; /* Green Channel. */
}

static void node_composit_buts_channel_matte(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col, *row;

  layout->label(IFACE_("Color Space:"), ICON_NONE);
  row = &layout->row(false);
  row->prop(
      ptr, "color_space", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  col = &layout->column(false);
  col->label(IFACE_("Key Channel:"), ICON_NONE);
  row = &col->row(false);
  row->prop(ptr,
            "matte_channel",
            UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND,
            std::nullopt,
            ICON_NONE);

  col = &layout->column(false);

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
}

using namespace blender::compositor;

static CMPNodeChannelMatteColorSpace get_color_space(const bNode &node)
{
  return static_cast<CMPNodeChannelMatteColorSpace>(node.custom1);
}

/* Get the index of the channel used to generate the matte. */
static int get_matte_channel(const bNode &node)
{
  return node.custom2 - 1;
}

/* Get the index of the channel used to compute the limit value. */
static int get_limit_channel(const bNode &node)
{
  return node_storage(node).channel - 1;
}

/* Get the indices of the channels used to compute the limit value. We always assume the limit
 * algorithm is Max, if it is a single limit channel, store it in both limit channels, because
 * the maximum of two identical values is the same value. */
static int2 get_limit_channels(const bNode &node)
{
  int2 limit_channels;
  if (node_storage(node).algorithm == CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_MAX) {
    /* If the algorithm is Max, store the indices of the other two channels other than the matte
     * channel. */
    limit_channels[0] = (get_matte_channel(node) + 1) % 3;
    limit_channels[1] = (get_matte_channel(node) + 2) % 3;
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
  const float color_space = int(get_color_space(*node));
  const float matte_channel = get_matte_channel(*node);
  const float2 limit_channels = float2(get_limit_channels(*node));

  return GPU_stack_link(material,
                        node,
                        "node_composite_channel_matte",
                        inputs,
                        outputs,
                        GPU_constant(&color_space),
                        GPU_constant(&matte_channel),
                        GPU_constant(limit_channels));
}

template<CMPNodeChannelMatteColorSpace ColorSpace>
static void channel_key(const float4 &color,
                        const float min_limit,
                        const float max_limit,
                        const int matte_channel,
                        const int2 limit_channels,
                        float4 &result,
                        float &matte)
{
  float3 channels;
  if constexpr (ColorSpace == CMP_NODE_CHANNEL_MATTE_CS_HSV) {
    rgb_to_hsv_v(color, channels);
  }
  else if (ColorSpace == CMP_NODE_CHANNEL_MATTE_CS_YUV) {
    rgb_to_yuv(
        color.x, color.y, color.z, &channels.x, &channels.y, &channels.z, BLI_YUV_ITU_BT709);
  }
  else if (ColorSpace == CMP_NODE_CHANNEL_MATTE_CS_YCC) {
    rgb_to_ycc(
        color.x, color.y, color.z, &channels.x, &channels.y, &channels.z, BLI_YCC_ITU_BT709);
    channels /= 255.0f;
  }
  else {
    channels = color.xyz();
  }

  float matte_value = channels[matte_channel];
  float limit_value = math::max(channels[limit_channels.x], channels[limit_channels.y]);

  float alpha = 1.0f - (matte_value - limit_value);
  if (alpha > max_limit) {
    alpha = color.w;
  }
  else if (alpha < min_limit) {
    alpha = 0.0f;
  }
  else {
    alpha = (alpha - min_limit) / (max_limit - min_limit);
  }

  matte = math::min(alpha, color.w);
  result = color * matte;
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  const CMPNodeChannelMatteColorSpace color_space = get_color_space(builder.node());
  const int matte_channel = get_matte_channel(builder.node());
  const int2 limit_channels = get_limit_channels(builder.node());

  switch (color_space) {
    case CMP_NODE_CHANNEL_MATTE_CS_RGB:
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI3_SO2<float4, float, float, float4, float>(
            "Channel Key RGB",
            [=](const float4 &color,
                const float &minimum,
                const float &maximum,
                float4 &output_color,
                float &matte) -> void {
              channel_key<CMP_NODE_CHANNEL_MATTE_CS_RGB>(
                  color, minimum, maximum, matte_channel, limit_channels, output_color, matte);
            },
            mf::build::exec_presets::SomeSpanOrSingle<0>());
      });
      break;
    case CMP_NODE_CHANNEL_MATTE_CS_HSV:
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI3_SO2<float4, float, float, float4, float>(
            "Channel Key HSV",
            [=](const float4 &color,
                const float &minimum,
                const float &maximum,
                float4 &output_color,
                float &matte) -> void {
              channel_key<CMP_NODE_CHANNEL_MATTE_CS_HSV>(
                  color, minimum, maximum, matte_channel, limit_channels, output_color, matte);
            },
            mf::build::exec_presets::SomeSpanOrSingle<0>());
      });
      break;
    case CMP_NODE_CHANNEL_MATTE_CS_YUV:
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI3_SO2<float4, float, float, float4, float>(
            "Channel Key YUV",
            [=](const float4 &color,
                const float &minimum,
                const float &maximum,
                float4 &output_color,
                float &matte) -> void {
              channel_key<CMP_NODE_CHANNEL_MATTE_CS_YUV>(
                  color, minimum, maximum, matte_channel, limit_channels, output_color, matte);
            },
            mf::build::exec_presets::SomeSpanOrSingle<0>());
      });
      break;
    case CMP_NODE_CHANNEL_MATTE_CS_YCC:
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI3_SO2<float4, float, float, float4, float>(
            "Channel Key YCC",
            [=](const float4 &color,
                const float &minimum,
                const float &maximum,
                float4 &output_color,
                float &matte) -> void {
              channel_key<CMP_NODE_CHANNEL_MATTE_CS_YCC>(
                  color, minimum, maximum, matte_channel, limit_channels, output_color, matte);
            },
            mf::build::exec_presets::SomeSpanOrSingle<0>());
      });
      break;
  }
}

}  // namespace blender::nodes::node_composite_channel_matte_cc

static void register_node_type_cmp_channel_matte()
{
  namespace file_ns = blender::nodes::node_composite_channel_matte_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeChannelMatte", CMP_NODE_CHANNEL_MATTE);
  ntype.ui_name = "Channel Key";
  ntype.ui_description = "Create matte based on differences in color channels";
  ntype.enum_name_legacy = "CHANNEL_MATTE";
  ntype.nclass = NODE_CLASS_MATTE;
  ntype.declare = file_ns::cmp_node_channel_matte_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_channel_matte;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_channel_matte;
  blender::bke::node_type_storage(
      ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_channel_matte)
