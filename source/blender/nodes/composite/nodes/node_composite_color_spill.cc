/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_color_spill_cc {

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

static const EnumPropertyItem limit_method_items[] = {
    {CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_SINGLE,
     "SINGLE",
     0,
     N_("Single"),
     N_("Limit by a single channel")},
    {CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_AVERAGE,
     "AVERAGE",
     0,
     N_("Average"),
     N_("Limit by the average of the other two channels")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_color_spill_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.use_custom_socket_order();

  b.add_output<decl::Color>("Image");

  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Factor", "Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Menu>("Spill Channel")
      .default_value(RGBChannel::G)
      .static_items(rgb_channel_items)
      .expanded()
      .translation_context(BLT_I18NCONTEXT_COLOR)
      .optional_label();
  b.add_input<decl::Menu>("Limit Method")
      .default_value(CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_SINGLE)
      .static_items(limit_method_items)
      .expanded()
      .optional_label();
  b.add_input<decl::Menu>("Limit Channel")
      .default_value(RGBChannel::R)
      .static_items(rgb_channel_items)
      .expanded()
      .translation_context(BLT_I18NCONTEXT_COLOR)
      .optional_label()
      .usage_by_menu("Limit Method", CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_SINGLE);
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
  /* Unused, but allocated for forward compatibility. */
  node->storage = MEM_callocN<NodeColorspill>(__func__);
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  return GPU_stack_link(material, node, "node_composite_color_spill", inputs, outputs);
}

/* Compute the indices of the channels used to compute the limit value. We always assume the limit
 * algorithm is Average, if it is a single limit channel, store it in both limit channels, because
 * the average of two identical values is the same value. */
static int2 compute_limit_channels(const CMPNodeColorSpillLimitAlgorithm limit_method,
                                   const int spill_channel,
                                   const int limit_channel)
{
  /* If the algorithm is Average, store the indices of the other two channels other than the spill
   * channel. */
  if (limit_method == CMP_NODE_COLOR_SPILL_LIMIT_ALGORITHM_AVERAGE) {
    return int2((spill_channel + 1) % 3, (spill_channel + 2) % 3);
  }

  /* If the algorithm is Single, store the index of the limit channel in both channels. */
  return int2(limit_channel);
}

static float3 compute_spill_scale(const bool use_spill_strength,
                                  const float4 spill_strength,
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

static float4 color_spill(const float4 color,
                          const float factor,
                          const int spill_channel,
                          const CMPNodeColorSpillLimitAlgorithm limit_method,
                          const int limit_channel,
                          const float limit_scale,
                          const bool use_spill_strength,
                          const float4 spill_strength)
{
  const int2 limit_channels = compute_limit_channels(limit_method, spill_channel, limit_channel);
  const float average_limit = (color[limit_channels.x] + color[limit_channels.y]) / 2.0f;
  const float map = factor * color[spill_channel] - limit_scale * average_limit;
  const float3 spill_scale = compute_spill_scale(
      use_spill_strength, spill_strength, spill_channel);
  return float4(map > 0.0f ? color.xyz() + spill_scale * map : color.xyz(), color.w);
}

using blender::compositor::Color;

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function =
      mf::build::SI8_SO<Color, float, MenuValue, MenuValue, MenuValue, float, bool, Color, Color>(
          "Color Spill",
          [=](const Color &color,
              const float &factor,
              const MenuValue spill_channel,
              const MenuValue limit_method,
              const MenuValue limit_channel,
              const float &limit_scale,
              const bool &use_spill_strength,
              const Color &spill_strength) -> Color {
            return Color(color_spill(float4(color),
                                     factor,
                                     spill_channel.value,
                                     CMPNodeColorSpillLimitAlgorithm(limit_method.value),
                                     limit_channel.value,
                                     limit_scale,
                                     use_spill_strength,
                                     float4(spill_strength)));
          },
          mf::build::exec_presets::SomeSpanOrSingle<0>());
  builder.set_matching_fn(function);
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
