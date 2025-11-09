/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "DNA_color_types.h"

#include "BKE_colortools.hh"
#include "BKE_node.hh"

#include "GPU_material.hh"

#include "COM_node_operation.hh"
#include "COM_result.hh"
#include "COM_utilities_gpu_material.hh"

#include "node_composite_util.hh"

/* **************** CURVE Time  ******************** */

namespace blender::nodes::node_composite_time_curves_cc {

static void cmp_node_time_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Start Frame").default_value(1);
  b.add_input<decl::Int>("End Frame").default_value(250);

  b.add_output<decl::Float>("Factor", "Fac");
}

static void node_composit_init_curves_time(bNodeTree * /*ntree*/, bNode *node)
{
  node->storage = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

using namespace blender::compositor;

class TimeCurveOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &result = this->get_result("Fac");
    result.allocate_single_value();

    CurveMapping *curve_mapping = const_cast<CurveMapping *>(this->get_curve_mapping());
    BKE_curvemapping_init(curve_mapping);
    const float time = BKE_curvemapping_evaluateF(
        curve_mapping, 0, this->compute_normalized_time());
    result.set_single_value(math::clamp(time, 0.0f, 1.0f));
  }

  float compute_normalized_time()
  {
    const int frame_number = this->context().get_frame_number();
    if (frame_number < this->get_start_frame()) {
      return 0.0f;
    }
    if (frame_number > this->get_end_frame()) {
      return 1.0f;
    }
    if (this->get_start_frame() == this->get_end_frame()) {
      return 0.0f;
    }
    return float(frame_number - this->get_start_frame()) /
           float(this->get_end_frame() - this->get_start_frame());
  }

  int get_start_frame()
  {
    return this->get_input("Start Frame").get_single_value_default(1);
  }

  int get_end_frame()
  {
    return this->get_input("End Frame").get_single_value_default(250);
  }

  const CurveMapping *get_curve_mapping()
  {
    return static_cast<const CurveMapping *>(bnode().storage);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new TimeCurveOperation(context, node);
}

}  // namespace blender::nodes::node_composite_time_curves_cc

static void register_node_type_cmp_curve_time()
{
  namespace file_ns = blender::nodes::node_composite_time_curves_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeTime", CMP_NODE_TIME);
  ntype.ui_name = "Time Curve";
  ntype.ui_description =
      "Generate a factor value (from 0.0 to 1.0) between scene start and end time, using a curve "
      "mapping";
  ntype.enum_name_legacy = "TIME";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::cmp_node_time_declare;
  blender::bke::node_type_size(ntype, 200, 140, 320);
  ntype.initfunc = file_ns::node_composit_init_curves_time;
  blender::bke::node_type_storage(ntype, "CurveMapping", node_free_curves, node_copy_curves);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_curve_time)

/* **************** CURVE RGB  ******************** */

namespace blender::nodes::node_composite_rgb_curves_cc {

static void cmp_node_rgbcurves_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description("Image/Color input on which RGB color transformation will be applied");
  b.add_input<decl::Float>("Factor", "Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Amount of influence the node exerts on the image");
  b.add_input<decl::Color>("Black Level")
      .default_value({0.0f, 0.0f, 0.0f, 1.0f})
      .description("Input color that should be mapped to black");
  b.add_input<decl::Color>("White Level")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description("Input color that should be mapped to white");
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_curve_rgb(bNodeTree * /*ntree*/, bNode *node)
{
  node->storage = BKE_curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
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

  float start_slopes[CM_TOT];
  float end_slopes[CM_TOT];
  BKE_curvemapping_compute_slopes(curve_mapping, start_slopes, end_slopes);
  float range_minimums[CM_TOT];
  BKE_curvemapping_get_range_minimums(curve_mapping, range_minimums);
  float range_dividers[CM_TOT];
  BKE_curvemapping_compute_range_dividers(curve_mapping, range_dividers);

  if (curve_mapping->tone == CURVE_TONE_FILMLIKE) {
    return GPU_stack_link(material,
                          node,
                          "curves_film_like_compositor",
                          inputs,
                          outputs,
                          band_texture,
                          GPU_constant(&band_layer),
                          GPU_uniform(&range_minimums[3]),
                          GPU_uniform(&range_dividers[3]),
                          GPU_uniform(&start_slopes[3]),
                          GPU_uniform(&end_slopes[3]));
  }

  const float min = 0.0f;
  const float max = 1.0f;
  GPU_link(material,
           "clamp_value",
           get_shader_node_input_link(*node, inputs, "Fac"),
           GPU_constant(&min),
           GPU_constant(&max),
           &get_shader_node_input(*node, inputs, "Fac").link);

  /* If the RGB curves do nothing, use a function that skips RGB computations. */
  if (BKE_curvemapping_is_map_identity(curve_mapping, 0) &&
      BKE_curvemapping_is_map_identity(curve_mapping, 1) &&
      BKE_curvemapping_is_map_identity(curve_mapping, 2))
  {
    return GPU_stack_link(material,
                          node,
                          "curves_combined_only_compositor",
                          inputs,
                          outputs,
                          band_texture,
                          GPU_constant(&band_layer),
                          GPU_uniform(&range_minimums[3]),
                          GPU_uniform(&range_dividers[3]),
                          GPU_uniform(&start_slopes[3]),
                          GPU_uniform(&end_slopes[3]));
  }

  return GPU_stack_link(material,
                        node,
                        "curves_combined_rgb_compositor",
                        inputs,
                        outputs,
                        band_texture,
                        GPU_constant(&band_layer),
                        GPU_uniform(range_minimums),
                        GPU_uniform(range_dividers),
                        GPU_uniform(start_slopes),
                        GPU_uniform(end_slopes));
}

static float4 curves_rgba(const CurveMapping *curve_mapping,
                          const float4 &color,
                          const float factor,
                          const float4 &black,
                          const float4 &white)
{
  float3 black_white_scale;
  BKE_curvemapping_set_black_white_ex(black, white, black_white_scale);

  float3 result;
  BKE_curvemapping_evaluate_premulRGBF_ex(curve_mapping, result, color, black, black_white_scale);
  return float4(math::interpolate(color.xyz(), result, math::clamp(factor, 0.0f, 1.0f)), color.w);
}

using blender::compositor::Color;

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  CurveMapping *curve_mapping = get_curve_mapping(builder.node());
  BKE_curvemapping_init(curve_mapping);
  BKE_curvemapping_premultiply(curve_mapping, false);

  builder.construct_and_set_matching_fn_cb([=]() {
    return mf::build::SI4_SO<Color, float, Color, Color, Color>(
        "RGB Curves",
        [=](const Color &color, const float factor, const Color &black, const Color &white)
            -> Color {
          return Color(
              curves_rgba(curve_mapping, float4(color), factor, float4(black), float4(white)));
        },
        mf::build::exec_presets::SomeSpanOrSingle<0>());
  });
}

}  // namespace blender::nodes::node_composite_rgb_curves_cc

static void register_node_type_cmp_curve_rgb()
{
  namespace file_ns = blender::nodes::node_composite_rgb_curves_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeCurveRGB", CMP_NODE_CURVE_RGB);
  ntype.ui_name = "RGB Curves";
  ntype.ui_description = "Perform level adjustments on each color channel of an image";
  ntype.enum_name_legacy = "CURVE_RGB";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::cmp_node_rgbcurves_declare;
  blender::bke::node_type_size(ntype, 200, 140, 320);
  ntype.initfunc = file_ns::node_composit_init_curve_rgb;
  blender::bke::node_type_storage(ntype, "CurveMapping", node_free_curves, node_copy_curves);
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_curve_rgb)
