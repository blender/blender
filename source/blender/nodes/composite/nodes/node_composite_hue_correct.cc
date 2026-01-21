/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "BKE_colortools.hh"
#include "BKE_node.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_hue_correct_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Factor", "Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_output<decl::Color>("Image");
}

static void node_init(bNodeTree * /*ntree*/, bNode *node)
{
  node->storage = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);

  CurveMapping *cumapping = static_cast<CurveMapping *>(node->storage);

  cumapping->preset = CURVE_PRESET_MID8;

  for (int c = 0; c < 3; c++) {
    CurveMap *cuma = &cumapping->cm[c];
    BKE_curvemap_reset(cuma, &cumapping->clipr, cumapping->preset, CurveMapSlopeType::Positive);
  }
  /* use wrapping for all hue correct nodes */
  cumapping->flag |= CUMA_USE_WRAPPING;
  /* default to showing Saturation */
  cumapping->cur = 1;
}

using namespace blender::compositor;

static CurveMapping *get_curve_mapping(const bNode &node)
{
  return static_cast<CurveMapping *>(node.storage);
}

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  CurveMapping *curve_mapping = get_curve_mapping(*node);

  BKE_curvemapping_init(curve_mapping);
  float *band_values;
  int band_size;
  BKE_curvemapping_table_RGBA(curve_mapping, &band_values, &band_size);
  float band_layer;
  GPUNodeLink *band_texture = GPU_color_band(material, band_size, band_values, &band_layer);

  float range_minimums[CM_TOT];
  BKE_curvemapping_get_range_minimums(curve_mapping, range_minimums);
  float range_dividers[CM_TOT];
  BKE_curvemapping_compute_range_dividers(curve_mapping, range_dividers);

  return GPU_stack_link(material,
                        node,
                        "node_composite_hue_correct",
                        inputs,
                        outputs,
                        band_texture,
                        GPU_constant(&band_layer),
                        GPU_uniform(range_minimums),
                        GPU_uniform(range_dividers));
}

static float4 hue_correct(const float4 &color, const float factor, const CurveMapping *curve_map)
{
  float3 hsv;
  rgb_to_hsv_v(color, hsv);

  /* We parameterize the curve using the hue value. */
  float parameter = hsv.x;

  /* Adjust each of the Hue, Saturation, and Values accordingly to the following rules. A curve map
   * value of 0.5 means no change in hue, so adjust the value to get an identity at 0.5. Since the
   * identity of addition is 0, we subtract 0.5 (0.5 - 0.5 = 0). A curve map value of 0.5 means no
   * change in saturation or value, so adjust the value to get an identity at 0.5. Since the
   * identity of multiplication is 1, we multiply by 2 (0.5 * 2 = 1). */
  hsv.x += BKE_curvemapping_evaluateF(curve_map, 0, parameter) - 0.5f;
  hsv.y *= BKE_curvemapping_evaluateF(curve_map, 1, parameter) * 2.0f;
  hsv.z *= BKE_curvemapping_evaluateF(curve_map, 2, parameter) * 2.0f;

  /* Sanitize the new hue and saturation values. */
  hsv.x = math::fract(hsv.x);
  hsv.y = math::clamp(hsv.y, 0.0f, 1.0f);

  float3 rgb_result;
  hsv_to_rgb_v(hsv, rgb_result);
  float4 result = float4(math::max(rgb_result, float3(0.0f)), color.w);

  return math::interpolate(color, result, factor);
}

using compositor::Color;

static void node_build_multi_function(nodes::NodeMultiFunctionBuilder &builder)
{
  CurveMapping *curve_mapping = get_curve_mapping(builder.node());
  BKE_curvemapping_init(curve_mapping);

  builder.construct_and_set_matching_fn_cb([&]() {
    return mf::build::SI2_SO<Color, float, Color>(
        "Hue Correct",
        /* Take ownership of the tree because it contains the curve mapping. */
        [curve_mapping, tree = builder.shared_tree()](const Color &color, const float factor)
            -> Color { return Color(hue_correct(float4(color), factor, curve_mapping)); },
        mf::build::exec_presets::SomeSpanOrSingle<0>());
  });
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeHueCorrect", CMP_NODE_HUECORRECT);
  ntype.ui_name = "Hue Correct";
  ntype.ui_description = "Adjust hue, saturation, and value with a curve";
  ntype.enum_name_legacy = "HUECORRECT";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = node_declare;
  bke::node_type_size(ntype, 320, 140, 500);
  ntype.initfunc = node_init;
  bke::node_type_storage(ntype, "CurveMapping", node_free_curves, node_copy_curves);
  ntype.gpu_fn = node_gpu_material;
  ntype.build_multi_function = node_build_multi_function;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_hue_correct_cc
