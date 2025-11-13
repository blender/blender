/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"
#include "NOD_socket_usage_inference.hh"

#include "BKE_node.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_channel_matte_cc {

static const EnumPropertyItem color_space_items[] = {
    {CMP_NODE_CHANNEL_MATTE_CS_RGB, "RGB", 0, N_("RGB"), N_("RGB (Red, Green, Blue) color space")},
    {CMP_NODE_CHANNEL_MATTE_CS_HSV,
     "HSV",
     0,
     N_("HSV"),
     N_("HSV (Hue, Saturation, Value) color space")},
    {CMP_NODE_CHANNEL_MATTE_CS_YUV,
     "YUV",
     0,
     N_("YUV"),
     N_("YUV (Y - luma, U V - chroma) color space")},
    {CMP_NODE_CHANNEL_MATTE_CS_YCC,
     "YCC",
     0,
     N_("YCbCr"),
     N_("YCbCr (Y - luma, Cb - blue-difference chroma, Cr - red-difference chroma) color space")},
    {0, nullptr, 0, nullptr, nullptr},
};

enum class RGBChannel : uint8_t {
  R = 0,
  G = 1,
  B = 2,
};

static const EnumPropertyItem rgb_channel_items[] = {
    {int(RGBChannel::R), "R", 0, "R", ""},
    {int(RGBChannel::G), "G", 0, "G", ""},
    {int(RGBChannel::B), "B", 0, "B", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

enum class HSVChannel : uint8_t {
  H = 0,
  S = 1,
  V = 2,
};

static const EnumPropertyItem hsv_channel_items[] = {
    {int(HSVChannel::H), "H", 0, "H", ""},
    {int(HSVChannel::S), "S", 0, "S", ""},
    {int(HSVChannel::V), "V", 0, "V", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

enum class YUVChannel : uint8_t {
  Y = 0,
  U = 1,
  V = 2,
};

static const EnumPropertyItem yuv_channel_items[] = {
    {int(YUVChannel::Y), "Y", 0, "Y", ""},
    {int(YUVChannel::U), "U", 0, "U", ""},
    {int(YUVChannel::V), "V", 0, "V", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

enum class YCbCrChannel : uint8_t {
  Y = 0,
  Cb = 1,
  Cr = 2,
};

static const EnumPropertyItem ycbcr_channel_items[] = {
    {int(YCbCrChannel::Y), "Y", 0, "Y", ""},
    {int(YCbCrChannel::Cb), "CB", 0, "Cb", ""},
    {int(YCbCrChannel::Cr), "CR", 0, "Cr", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem limit_method_items[] = {
    {CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_SINGLE,
     "SINGLE",
     0,
     "Single",
     "Limit by single channel"},
    {CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_MAX,
     "MAX",
     0,
     "Max",
     "Limit by maximum of other channels"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_channel_matte_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f}).hide_value();
  b.add_output<decl::Color>("Image").align_with_previous();
  b.add_output<decl::Float>("Matte");

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

  b.add_input<decl::Menu>("Color Space")
      .default_value(CMP_NODE_CHANNEL_MATTE_CS_RGB)
      .static_items(color_space_items)
      .expanded()
      .optional_label();
  b.add_input<decl::Menu>("RGB Key Channel")
      .default_value(RGBChannel::G)
      .static_items(rgb_channel_items)
      .expanded()
      .translation_context(BLT_I18NCONTEXT_COLOR)
      .usage_by_menu("Color Space", CMP_NODE_CHANNEL_MATTE_CS_RGB)
      .optional_label();
  b.add_input<decl::Menu>("HSV Key Channel")
      .default_value(HSVChannel::H)
      .static_items(hsv_channel_items)
      .expanded()
      .translation_context(BLT_I18NCONTEXT_COLOR)
      .usage_by_menu("Color Space", CMP_NODE_CHANNEL_MATTE_CS_HSV)
      .optional_label();
  b.add_input<decl::Menu>("YUV Key Channel")
      .default_value(YUVChannel::V)
      .static_items(yuv_channel_items)
      .expanded()
      .usage_by_menu("Color Space", CMP_NODE_CHANNEL_MATTE_CS_YUV)
      .optional_label();
  b.add_input<decl::Menu>("YCbCr Key Channel")
      .default_value(YCbCrChannel::Cr)
      .static_items(ycbcr_channel_items)
      .expanded()
      .usage_by_menu("Color Space", CMP_NODE_CHANNEL_MATTE_CS_YCC)
      .optional_label();

  b.add_input<decl::Menu>("Limit Method")
      .default_value(CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_MAX)
      .static_items(limit_method_items)
      .expanded()
      .optional_label();
  b.add_input<decl::Menu>("RGB Limit Channel")
      .default_value(RGBChannel::R)
      .static_items(rgb_channel_items)
      .expanded()
      .optional_label()
      .make_available([](bNode &node) {
        bNodeSocket &limit_method_socket = *blender::bke::node_find_socket(
            node, SOCK_IN, "Limit Method");
        limit_method_socket.default_value_typed<bNodeSocketValueMenu>()->value =
            CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_SINGLE;

        bNodeSocket &color_space_socket = *blender::bke::node_find_socket(
            node, SOCK_IN, "Color Space");
        color_space_socket.default_value_typed<bNodeSocketValueMenu>()->value =
            CMP_NODE_CHANNEL_MATTE_CS_RGB;
      })
      .usage_inference(
          [](const socket_usage_inference::SocketUsageParams &params) -> std::optional<bool> {
            return params.menu_input_may_be("Limit Method",
                                            CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_SINGLE) &&
                   params.menu_input_may_be("Color Space", CMP_NODE_CHANNEL_MATTE_CS_RGB);
          });
  b.add_input<decl::Menu>("HSV Limit Channel")
      .default_value(HSVChannel::S)
      .static_items(hsv_channel_items)
      .expanded()
      .optional_label()
      .make_available([](bNode &node) {
        bNodeSocket &limit_method_socket = *blender::bke::node_find_socket(
            node, SOCK_IN, "Limit Method");
        limit_method_socket.default_value_typed<bNodeSocketValueMenu>()->value =
            CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_SINGLE;

        bNodeSocket &color_space_socket = *blender::bke::node_find_socket(
            node, SOCK_IN, "Color Space");
        color_space_socket.default_value_typed<bNodeSocketValueMenu>()->value =
            CMP_NODE_CHANNEL_MATTE_CS_HSV;
      })
      .usage_inference(
          [](const socket_usage_inference::SocketUsageParams &params) -> std::optional<bool> {
            return params.menu_input_may_be("Limit Method",
                                            CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_SINGLE) &&
                   params.menu_input_may_be("Color Space", CMP_NODE_CHANNEL_MATTE_CS_HSV);
          });
  b.add_input<decl::Menu>("YUV Limit Channel")
      .default_value(YUVChannel::U)
      .static_items(yuv_channel_items)
      .expanded()
      .optional_label()
      .make_available([](bNode &node) {
        bNodeSocket &limit_method_socket = *blender::bke::node_find_socket(
            node, SOCK_IN, "Limit Method");
        limit_method_socket.default_value_typed<bNodeSocketValueMenu>()->value =
            CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_SINGLE;

        bNodeSocket &color_space_socket = *blender::bke::node_find_socket(
            node, SOCK_IN, "Color Space");
        color_space_socket.default_value_typed<bNodeSocketValueMenu>()->value =
            CMP_NODE_CHANNEL_MATTE_CS_YUV;
      })
      .usage_inference(
          [](const socket_usage_inference::SocketUsageParams &params) -> std::optional<bool> {
            return params.menu_input_may_be("Limit Method",
                                            CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_SINGLE) &&
                   params.menu_input_may_be("Color Space", CMP_NODE_CHANNEL_MATTE_CS_YUV);
          });
  b.add_input<decl::Menu>("YCbCr Limit Channel")
      .default_value(YCbCrChannel::Cb)
      .static_items(ycbcr_channel_items)
      .expanded()
      .optional_label()
      .make_available([](bNode &node) {
        bNodeSocket &limit_method_socket = *blender::bke::node_find_socket(
            node, SOCK_IN, "Limit Method");
        limit_method_socket.default_value_typed<bNodeSocketValueMenu>()->value =
            CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_SINGLE;

        bNodeSocket &color_space_socket = *blender::bke::node_find_socket(
            node, SOCK_IN, "Color Space");
        color_space_socket.default_value_typed<bNodeSocketValueMenu>()->value =
            CMP_NODE_CHANNEL_MATTE_CS_YCC;
      })
      .usage_inference(
          [](const socket_usage_inference::SocketUsageParams &params) -> std::optional<bool> {
            return params.menu_input_may_be("Limit Method",
                                            CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_SINGLE) &&
                   params.menu_input_may_be("Color Space", CMP_NODE_CHANNEL_MATTE_CS_YCC);
          });
}

static void node_composit_init_channel_matte(bNodeTree * /*ntree*/, bNode *node)
{
  /* Unused, but allocated for forward compatibility. */
  node->storage = MEM_callocN<NodeChroma>(__func__);
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  return GPU_stack_link(material, node, "node_composite_channel_matte", inputs, outputs);
}

static float3 compute_channels(const float4 color, const CMPNodeChannelMatteColorSpace color_space)
{
  switch (color_space) {
    case CMP_NODE_CHANNEL_MATTE_CS_RGB: {
      return color.xyz();
    }
    case CMP_NODE_CHANNEL_MATTE_CS_HSV: {
      float4 hsv;
      rgb_to_hsv_v(color, hsv);
      return hsv.xyz();
    }
    case CMP_NODE_CHANNEL_MATTE_CS_YUV: {
      float4 yuv;
      rgb_to_yuv(color.x, color.y, color.z, &yuv.x, &yuv.y, &yuv.z, BLI_YUV_ITU_BT709);
      return yuv.xyz();
    }
    case CMP_NODE_CHANNEL_MATTE_CS_YCC: {
      float4 ycc;
      rgb_to_ycc(color.x, color.y, color.z, &ycc.x, &ycc.y, &ycc.z, BLI_YCC_ITU_BT709);
      ycc /= 255.0f;
      return ycc.xyz();
    }
  }

  return color.xyz();
}

static int get_channel_index(const CMPNodeChannelMatteColorSpace color_space,
                             const int rgb_channel,
                             const int hsv_channel,
                             const int yuv_channel,
                             const int ycc_channel)
{
  switch (color_space) {
    case CMP_NODE_CHANNEL_MATTE_CS_RGB:
      return rgb_channel;
    case CMP_NODE_CHANNEL_MATTE_CS_HSV:
      return hsv_channel;
    case CMP_NODE_CHANNEL_MATTE_CS_YUV:
      return yuv_channel;
    case CMP_NODE_CHANNEL_MATTE_CS_YCC:
      return ycc_channel;
  }

  return 0;
}

/* Compute the indices of the channels used to compute the limit value. We always assume the limit
 * algorithm is Max, if it is a single limit channel, store it in both limit channels, because
 * the maximum of two identical values is the same value. */
static int2 compute_limit_channels(const CMPNodeChannelMatteLimitAlgorithm limit_method,
                                   const int matte_channel,
                                   const int limit_channel)
{
  /* If the algorithm is Max, store the indices of the other two channels other than the matte
   * channel. */
  if (limit_method == CMP_NODE_CHANNEL_MATTE_LIMIT_ALGORITHM_MAX) {
    return int2((matte_channel + 1) % 3, (matte_channel + 2) % 3);
  }

  /* If the algorithm is Single, store the index of the limit channel in both channels. */
  return int2(limit_channel);
}

static void channel_key(const float4 color,
                        const float minimum,
                        const float maximum,
                        const CMPNodeChannelMatteColorSpace color_space,
                        const int rgb_key_channel,
                        const int hsv_key_channel,
                        const int yuv_key_channel,
                        const int ycc_key_channel,
                        const CMPNodeChannelMatteLimitAlgorithm limit_method,
                        const int rgb_limit_channel,
                        const int hsv_limit_channel,
                        const int yuv_limit_channel,
                        const int ycc_limit_channel,
                        float4 &output_color,
                        float &matte)
{
  const float3 channels = compute_channels(color, color_space);
  const int matte_channel = get_channel_index(
      color_space, rgb_key_channel, hsv_key_channel, yuv_key_channel, ycc_key_channel);
  const int limit_channel = get_channel_index(
      color_space, rgb_limit_channel, hsv_limit_channel, yuv_limit_channel, ycc_limit_channel);
  const int2 limit_channels = compute_limit_channels(limit_method, matte_channel, limit_channel);

  float matte_value = channels[matte_channel];
  float limit_value = math::max(channels[limit_channels.x], channels[limit_channels.y]);

  float alpha = 1.0f - (matte_value - limit_value);
  if (alpha > maximum) {
    alpha = color.w;
  }
  else if (alpha < minimum) {
    alpha = 0.0f;
  }
  else {
    alpha = (alpha - minimum) / (maximum - minimum);
  }

  matte = math::min(alpha, color.w);
  output_color = color * matte;
}

using blender::compositor::Color;

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function =
      mf::build::detail::build_multi_function_with_n_inputs_two_outputs<Color, float>(
          "Channel Key",
          [=](const Color &color,
              const float &minimum,
              const float &maximum,
              const MenuValue &color_space,
              const MenuValue &rgb_key_channel,
              const MenuValue &hsv_key_channel,
              const MenuValue &yuv_key_channel,
              const MenuValue &ycc_key_channel,
              const MenuValue &limit_method,
              const MenuValue &rgb_limit_channel,
              const MenuValue &hsv_limit_channel,
              const MenuValue &yuv_limit_channel,
              const MenuValue &ycc_limit_channel,
              Color &output_color,
              float &matte) -> void {
            float4 out_color;
            channel_key(float4(color),
                        minimum,
                        maximum,
                        CMPNodeChannelMatteColorSpace(color_space.value),
                        rgb_key_channel.value,
                        hsv_key_channel.value,
                        yuv_key_channel.value,
                        ycc_key_channel.value,
                        CMPNodeChannelMatteLimitAlgorithm(limit_method.value),
                        rgb_limit_channel.value,
                        hsv_limit_channel.value,
                        yuv_limit_channel.value,
                        ycc_limit_channel.value,
                        out_color,
                        matte);
            output_color = Color(out_color);
          },
          mf::build::exec_presets::SomeSpanOrSingle<0>(),
          TypeSequence<Color,
                       float,
                       float,
                       MenuValue,
                       MenuValue,
                       MenuValue,
                       MenuValue,
                       MenuValue,
                       MenuValue,
                       MenuValue,
                       MenuValue,
                       MenuValue,
                       MenuValue>());

  builder.set_matching_fn(function);
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
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_channel_matte;
  blender::bke::node_type_storage(
      ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_channel_matte)
