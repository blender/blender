/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <cmath>

#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

/* ******************* Chroma Key ********************************************************** */

namespace blender::nodes::node_composite_chroma_matte_cc {

static void cmp_node_chroma_matte_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f}).hide_value();
  b.add_output<decl::Color>("Image").align_with_previous();
  b.add_output<decl::Float>("Matte");

  b.add_input<decl::Color>("Key Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Minimum")
      .default_value(DEG2RADF(10.0f))
      .subtype(PROP_ANGLE)
      .description(
          "If the angle between the color and the key color in CrCb space is less than this "
          "minimum angle, it is keyed");
  b.add_input<decl::Float>("Maximum")
      .default_value(DEG2RADF(30.0f))
      .subtype(PROP_ANGLE)
      .description(
          "If the angle between the color and the key color in CrCb space is larger than this "
          "maximum angle, it is not keyed");
  b.add_input<decl::Float>("Falloff")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Controls the falloff between keyed and non-keyed values. 0 means completely sharp and "
          "1 means completely smooth");
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  return GPU_stack_link(material, node, "node_composite_chroma_matte", inputs, outputs);
}

/* Algorithm from the book Video Demystified. Chapter 7. Chroma Keying. */
static void chroma_matte(const float4 &color,
                         const float4 &key,
                         const float &minimum,
                         const float &maximum,
                         const float &falloff,
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

  /* Normalize the CrCb components into the [-1, 1] range. */
  float2 color_cc = color_ycca.yz() * 2.0f - 1.0f;
  float2 key_cc = math::normalize(key_ycca.yz() * 2.0f - 1.0f);

  /* Rotate the color onto the space of the key such that x axis of the color space passes
   * through the key color. */
  color_cc = math::from_direction(key_cc * float2(1.0f, -1.0f)) * color_cc;

  /* Compute foreground key. If positive, the value is in the [0, 1] range. */
  float foreground_key = color_cc.x - (math::abs(color_cc.y) / math::tan(maximum / 2.0f));

  /* Negative foreground key values retain the original alpha. Positive values are scaled by the
   * falloff, while colors that make an angle less than the minimum angle get a zero alpha. */
  float alpha = color.w;
  if (foreground_key > 0.0f) {
    alpha = 1.0f - (foreground_key / falloff);

    if (math::abs(math::atan2(color_cc.y, color_cc.x)) < (minimum / 2.0f)) {
      alpha = 0.0f;
    }
  }

  /* Compute output. */
  matte = math::min(alpha, color.w);
  result = color * matte;
}

using blender::compositor::Color;

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  builder.construct_and_set_matching_fn_cb([=]() {
    return mf::build::SI5_SO2<Color, Color, float, float, float, Color, float>(
        "Chroma Key",
        [=](const Color &color,
            const Color &key_color,
            const float &minimum,
            const float &maximum,
            const float &falloff,
            Color &output_color,
            float &matte) -> void {
          float4 out_color;
          chroma_matte(
              float4(color), float4(key_color), minimum, maximum, falloff, out_color, matte);
          output_color = Color(out_color);
        },
        mf::build::exec_presets::SomeSpanOrSingle<0, 1>());
  });
}

}  // namespace blender::nodes::node_composite_chroma_matte_cc

static void register_node_type_cmp_chroma_matte()
{
  namespace file_ns = blender::nodes::node_composite_chroma_matte_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeChromaMatte", CMP_NODE_CHROMA_MATTE);
  ntype.ui_name = "Chroma Key";
  ntype.ui_description = "Create matte based on chroma values";
  ntype.enum_name_legacy = "CHROMA_MATTE";
  ntype.nclass = NODE_CLASS_MATTE;
  ntype.declare = file_ns::cmp_node_chroma_matte_declare;
  ntype.flag |= NODE_PREVIEW;
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;
  blender::bke::node_type_size(ntype, 155, 140, NODE_DEFAULT_MAX_WIDTH);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_chroma_matte)
