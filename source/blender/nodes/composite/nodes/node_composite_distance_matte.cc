/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_distance_matte_cc {

static const EnumPropertyItem color_space_items[] = {
    {CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA, "RGB", 0, N_("RGB"), N_("RGB color space")},
    {CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_YCCA, "YCC", 0, N_("YCC"), N_("YCbCr color space")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_distance_matte_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f}).hide_value();
  b.add_output<decl::Color>("Image").align_with_previous();
  b.add_output<decl::Float>("Matte");

  b.add_input<decl::Color>("Key Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Menu>("Color Space")
      .default_value(CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA)
      .static_items(color_space_items)
      .expanded()
      .optional_label();
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
}

static void node_composit_init_distance_matte(bNodeTree * /*ntree*/, bNode *node)
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
  return GPU_stack_link(material, node, "node_composite_distance_matte", inputs, outputs);
}

static void distance_key(const float4 color,
                         const float4 key,
                         const CMPNodeDistanceMatteColorSpace color_space,
                         const float tolerance,
                         const float falloff,
                         float4 &result,
                         float &matte)
{
  float4 color_vector = color;
  float4 key_vector = key;
  switch (color_space) {
    case CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA:
      color_vector = color;
      key_vector = key;
      break;
    case CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_YCCA:
      rgb_to_ycc(color.x,
                 color.y,
                 color.z,
                 &color_vector.x,
                 &color_vector.y,
                 &color_vector.z,
                 BLI_YCC_ITU_BT709);
      color_vector /= 255.0f;
      rgb_to_ycc(
          key.x, key.y, key.z, &key_vector.x, &key_vector.y, &key_vector.z, BLI_YCC_ITU_BT709);
      key_vector /= 255.0f;
      break;
  }

  float difference = math::distance(color_vector.xyz(), key_vector.xyz());
  bool is_opaque = difference > tolerance + falloff;
  float alpha = is_opaque ? color.w :
                            math::safe_divide(math::max(0.0f, difference - tolerance), falloff);
  matte = math::min(alpha, color.w);
  result = color * matte;
}

using blender::compositor::Color;

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI5_SO2<Color, Color, MenuValue, float, float, Color, float>(
      "Distance Key",
      [=](const Color &color,
          const Color &key_color,
          const MenuValue &color_space,
          const float &tolerance,
          const float &falloff,
          Color &output_color,
          float &matte) -> void {
        float4 out_color;
        distance_key(float4(color),
                     float4(key_color),
                     CMPNodeDistanceMatteColorSpace(color_space.value),
                     tolerance,
                     falloff,
                     out_color,
                     matte);
        output_color = Color(out_color);
      },
      mf::build::exec_presets::SomeSpanOrSingle<0, 1>());
  builder.set_matching_fn(function);
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
