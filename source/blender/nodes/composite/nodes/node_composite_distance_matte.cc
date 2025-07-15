/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "node_composite_util.hh"

/* ******************* channel Distance Matte ********************************* */

namespace blender::nodes::node_composite_distance_matte_cc {

NODE_STORAGE_FUNCS(NodeChroma)

static void cmp_node_distance_matte_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Color>("Key Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Tolerance")
      .default_value(0.1f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description(
          "If the distance between the color and the key color in the given color space is less "
          "than this threshold, it is keyed");
  b.add_input<decl::Float>("Falloff")
      .default_value(0.1f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description(
          "If the distance between the color and the key color in the given color space is less "
          "than this threshold, it is partially keyed, otherwise, it is not keyed");

  b.add_output<decl::Color>("Image");
  b.add_output<decl::Float>("Matte");
}

static void node_composit_init_distance_matte(bNodeTree * /*ntree*/, bNode *node)
{
  NodeChroma *c = MEM_callocN<NodeChroma>(__func__);
  node->storage = c;
  c->channel = CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA;
}

static void node_composit_buts_distance_matte(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->label(IFACE_("Color Space:"), ICON_NONE);
  uiLayout *row = &layout->row(false);
  row->prop(
      ptr, "channel", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

static CMPNodeDistanceMatteColorSpace get_color_space(const bNode &node)
{
  return static_cast<CMPNodeDistanceMatteColorSpace>(node_storage(node).channel);
}

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  switch (get_color_space(*node)) {
    case CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA:
      return GPU_stack_link(material, node, "node_composite_distance_matte_rgba", inputs, outputs);
    case CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_YCCA:
      return GPU_stack_link(material, node, "node_composite_distance_matte_ycca", inputs, outputs);
  }

  return false;
}

static void distance_key_rgba(const float4 &color,
                              const float4 &key,
                              const float tolerance,
                              const float falloff,
                              float4 &result,
                              float &matte)
{
  float difference = math::distance(color.xyz(), key.xyz());
  bool is_opaque = difference > tolerance + falloff;
  float alpha = is_opaque ? color.w : math::max(0.0f, difference - tolerance) / falloff;
  matte = math::min(alpha, color.w);
  result = color * matte;
}

static void distance_key_ycca(const float4 &color,
                              const float4 &key,
                              const float tolerance,
                              const float falloff,
                              float4 &result,
                              float &matte)
{
  float3 color_ycca;
  rgb_to_ycc(
      color.x, color.y, color.z, &color_ycca.x, &color_ycca.y, &color_ycca.z, BLI_YCC_ITU_BT709);
  color_ycca /= 255.0f;
  float3 key_ycca;
  rgb_to_ycc(key.x, key.y, key.z, &key_ycca.x, &key_ycca.y, &key_ycca.z, BLI_YCC_ITU_BT709);
  key_ycca /= 255.0f;

  float difference = math::distance(color_ycca.yz(), key_ycca.yz());
  bool is_opaque = difference > tolerance + falloff;
  float alpha = is_opaque ? color.w : math::max(0.0f, difference - tolerance) / falloff;
  matte = math::min(alpha, color.w);
  result = color * matte;
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  const CMPNodeDistanceMatteColorSpace color_space = get_color_space(builder.node());

  switch (color_space) {
    case CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_YCCA:
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI4_SO2<float4, float4, float, float, float4, float>(
            "Distance Key YCCA",
            [=](const float4 &color,
                const float4 &key_color,
                const float &tolerance,
                const float &falloff,
                float4 &output_color,
                float &matte) -> void {
              distance_key_ycca(color, key_color, tolerance, falloff, output_color, matte);
            },
            mf::build::exec_presets::SomeSpanOrSingle<0, 1>());
      });
      break;
    case CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA:
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI4_SO2<float4, float4, float, float, float4, float>(
            "Distance Key RGBA",
            [=](const float4 &color,
                const float4 &key_color,
                const float &tolerance,
                const float &falloff,
                float4 &output_color,
                float &matte) -> void {
              distance_key_rgba(color, key_color, tolerance, falloff, output_color, matte);
            },
            mf::build::exec_presets::SomeSpanOrSingle<0, 1>());
      });
      break;
  }
}

}  // namespace blender::nodes::node_composite_distance_matte_cc

static void register_node_type_cmp_distance_matte()
{
  namespace file_ns = blender::nodes::node_composite_distance_matte_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeDistanceMatte", CMP_NODE_DIST_MATTE);
  ntype.ui_name = "Distance Key";
  ntype.ui_description = "Create matte based on 3D distance between colors";
  ntype.enum_name_legacy = "DISTANCE_MATTE";
  ntype.nclass = NODE_CLASS_MATTE;
  ntype.declare = file_ns::cmp_node_distance_matte_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_distance_matte;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_distance_matte;
  blender::bke::node_type_storage(
      ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;
  blender::bke::node_type_size(ntype, 155, 140, NODE_DEFAULT_MAX_WIDTH);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_distance_matte)
