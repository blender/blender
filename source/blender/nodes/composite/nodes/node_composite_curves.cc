/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "DNA_color_types.h"

#include "BKE_colortools.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_node_operation.hh"
#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** CURVE Time  ******************** */

namespace blender::nodes::node_composite_time_curves_cc {

static void cmp_node_time_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Fac");
}

/* custom1 = start_frame, custom2 = end_frame */
static void node_composit_init_curves_time(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = 1;
  node->custom2 = 250;
  node->storage = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

using namespace blender::realtime_compositor;

class TimeCurveOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &result = get_result("Fac");
    result.allocate_single_value();

    CurveMapping *curve_mapping = const_cast<CurveMapping *>(get_curve_mapping());
    BKE_curvemapping_init(curve_mapping);
    const float time = BKE_curvemapping_evaluateF(curve_mapping, 0, compute_normalized_time());
    result.set_float_value(clamp_f(time, 0.0f, 1.0f));
  }

  const CurveMapping *get_curve_mapping()
  {
    return static_cast<const CurveMapping *>(bnode().storage);
  }

  int get_start_time()
  {
    return bnode().custom1;
  }

  int get_end_time()
  {
    return bnode().custom2;
  }

  float compute_normalized_time()
  {
    const int frame_number = context().get_frame_number();
    if (frame_number < get_start_time()) {
      return 0.0f;
    }
    if (frame_number > get_end_time()) {
      return 1.0f;
    }
    if (get_start_time() == get_end_time()) {
      return 0.0f;
    }
    return float(frame_number - get_start_time()) / float(get_end_time() - get_start_time());
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new TimeCurveOperation(context, node);
}

}  // namespace blender::nodes::node_composite_time_curves_cc

void register_node_type_cmp_curve_time()
{
  namespace file_ns = blender::nodes::node_composite_time_curves_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_TIME, "Time Curve", NODE_CLASS_INPUT);
  ntype.declare = file_ns::cmp_node_time_declare;
  blender::bke::node_type_size(&ntype, 200, 140, 320);
  ntype.initfunc = file_ns::node_composit_init_curves_time;
  blender::bke::node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}

/* **************** CURVE VEC  ******************** */

namespace blender::nodes::node_composite_vector_curves_cc {

static void cmp_node_curve_vec_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector")
      .default_value({0.0f, 0.0f, 0.0f})
      .min(-1.0f)
      .max(1.0f)
      .compositor_domain_priority(0);
  b.add_output<decl::Vector>("Vector");
}

static void node_composit_init_curve_vec(bNodeTree * /*ntree*/, bNode *node)
{
  node->storage = BKE_curvemapping_add(3, -1.0f, -1.0f, 1.0f, 1.0f);
}

static void node_buts_curvevec(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiTemplateCurveMapping(layout, ptr, "mapping", 'v', false, false, false, false);
}

using namespace blender::realtime_compositor;

static CurveMapping *get_curve_mapping(const bNode &node)
{
  return static_cast<CurveMapping *>(node.storage);
}

class VectorCurvesShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    CurveMapping *curve_mapping = get_curve_mapping(bnode());

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

    GPU_stack_link(material,
                   &bnode(),
                   "curves_vector",
                   inputs,
                   outputs,
                   band_texture,
                   GPU_constant(&band_layer),
                   GPU_uniform(range_minimums),
                   GPU_uniform(range_dividers),
                   GPU_uniform(start_slopes),
                   GPU_uniform(end_slopes));
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new VectorCurvesShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  CurveMapping *curve_mapping = get_curve_mapping(builder.node());
  BKE_curvemapping_init(curve_mapping);

  builder.construct_and_set_matching_fn_cb([=]() {
    return mf::build::SI1_SO<float4, float4>(
        "Vector Curves",
        [=](const float4 &vector) -> float4 {
          float4 output_vector = float4(0.0f);
          BKE_curvemapping_evaluate3F(curve_mapping, output_vector, vector);
          return output_vector;
        },
        mf::build::exec_presets::AllSpanOrSingle());
  });
}

}  // namespace blender::nodes::node_composite_vector_curves_cc

void register_node_type_cmp_curve_vec()
{
  namespace file_ns = blender::nodes::node_composite_vector_curves_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CURVE_VEC, "Vector Curves", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::cmp_node_curve_vec_declare;
  ntype.draw_buttons = file_ns::node_buts_curvevec;
  blender::bke::node_type_size(&ntype, 200, 140, 320);
  ntype.initfunc = file_ns::node_composit_init_curve_vec;
  blender::bke::node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}

/* **************** CURVE RGB  ******************** */

namespace blender::nodes::node_composite_rgb_curves_cc {

static void cmp_node_rgbcurves_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(1)
      .description("Amount of influence the node exerts on the image");
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0)
      .description("Image/Color input on which RGB color transformation will be applied");
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

using namespace blender::realtime_compositor;

static CurveMapping *get_curve_mapping(const bNode &node)
{
  return static_cast<CurveMapping *>(node.storage);
}

class RGBCurvesShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    CurveMapping *curve_mapping = get_curve_mapping(bnode());

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
      GPU_stack_link(material,
                     &bnode(),
                     "curves_film_like",
                     inputs,
                     outputs,
                     band_texture,
                     GPU_constant(&band_layer),
                     GPU_uniform(&range_minimums[3]),
                     GPU_uniform(&range_dividers[3]),
                     GPU_uniform(&start_slopes[3]),
                     GPU_uniform(&end_slopes[3]));
      return;
    }

    const float min = 0.0f;
    const float max = 1.0f;
    GPU_link(material,
             "clamp_value",
             get_input_link("Fac"),
             GPU_constant(&min),
             GPU_constant(&max),
             &get_input("Fac").link);

    /* If the RGB curves do nothing, use a function that skips RGB computations. */
    if (BKE_curvemapping_is_map_identity(curve_mapping, 0) &&
        BKE_curvemapping_is_map_identity(curve_mapping, 1) &&
        BKE_curvemapping_is_map_identity(curve_mapping, 2))
    {
      GPU_stack_link(material,
                     &bnode(),
                     "curves_combined_only",
                     inputs,
                     outputs,
                     band_texture,
                     GPU_constant(&band_layer),
                     GPU_uniform(&range_minimums[3]),
                     GPU_uniform(&range_dividers[3]),
                     GPU_uniform(&start_slopes[3]),
                     GPU_uniform(&end_slopes[3]));
      return;
    }

    GPU_stack_link(material,
                   &bnode(),
                   "curves_combined_rgb",
                   inputs,
                   outputs,
                   band_texture,
                   GPU_constant(&band_layer),
                   GPU_uniform(range_minimums),
                   GPU_uniform(range_dividers),
                   GPU_uniform(start_slopes),
                   GPU_uniform(end_slopes));
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new RGBCurvesShaderNode(node);
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  CurveMapping *curve_mapping = get_curve_mapping(builder.node());
  BKE_curvemapping_init(curve_mapping);
  BKE_curvemapping_premultiply(curve_mapping, false);

  builder.construct_and_set_matching_fn_cb([=]() {
    return mf::build::SI4_SO<float, float4, float4, float4, float4>(
        "RGB Curves",
        [=](const float factor, const float4 &color, const float4 &black, const float4 &white)
            -> float4 {
          float3 black_white_scale;
          BKE_curvemapping_set_black_white_ex(black, white, black_white_scale);

          float3 result;
          BKE_curvemapping_evaluate_premulRGBF_ex(
              curve_mapping, result, color, black, black_white_scale);
          return float4(math::interpolate(color.xyz(), result, math::clamp(factor, 0.0f, 1.0f)),
                        color.w);
        },
        mf::build::exec_presets::SomeSpanOrSingle<1>());
  });
}

}  // namespace blender::nodes::node_composite_rgb_curves_cc

void register_node_type_cmp_curve_rgb()
{
  namespace file_ns = blender::nodes::node_composite_rgb_curves_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CURVE_RGB, "RGB Curves", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_rgbcurves_declare;
  blender::bke::node_type_size(&ntype, 200, 140, 320);
  ntype.initfunc = file_ns::node_composit_init_curve_rgb;
  blender::bke::node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
