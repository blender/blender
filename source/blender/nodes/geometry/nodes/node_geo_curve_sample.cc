/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_devirtualize_parameters.hh"
#include "BLI_length_parameterize.hh"

#include "BKE_curves.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_sample_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveSample)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve"))
      .only_realized_data()
      .supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Float>(N_("Factor"))
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .supports_field()
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_FACTOR; });
  b.add_input<decl::Float>(N_("Length"))
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .supports_field()
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_LENGTH; });
  b.add_output<decl::Vector>(N_("Position")).dependent_field();
  b.add_output<decl::Vector>(N_("Tangent")).dependent_field();
  b.add_output<decl::Vector>(N_("Normal")).dependent_field();
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void node_type_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveSample *data = MEM_cnew<NodeGeometryCurveSample>(__func__);
  data->mode = GEO_NODE_CURVE_SAMPLE_LENGTH;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryCurveSample &storage = node_storage(*node);
  const GeometryNodeCurveSampleMode mode = (GeometryNodeCurveSampleMode)storage.mode;

  bNodeSocket *factor = ((bNodeSocket *)node->inputs.first)->next;
  bNodeSocket *length = factor->next;

  nodeSetSocketAvailability(ntree, factor, mode == GEO_NODE_CURVE_SAMPLE_FACTOR);
  nodeSetSocketAvailability(ntree, length, mode == GEO_NODE_CURVE_SAMPLE_LENGTH);
}

static void sample_indices_and_lengths(const Span<float> accumulated_lengths,
                                       const Span<float> sample_lengths,
                                       const IndexMask mask,
                                       MutableSpan<int> r_segment_indices,
                                       MutableSpan<float> r_length_in_segment)
{
  const float total_length = accumulated_lengths.last();
  length_parameterize::SampleSegmentHint hint;

  mask.to_best_mask_type([&](const auto mask) {
    for (const int64_t i : mask) {
      int segment_i;
      float factor_in_segment;
      length_parameterize::sample_at_length(accumulated_lengths,
                                            std::clamp(sample_lengths[i], 0.0f, total_length),
                                            segment_i,
                                            factor_in_segment,
                                            &hint);
      const float segment_start = segment_i == 0 ? 0.0f : accumulated_lengths[segment_i - 1];
      const float segment_end = accumulated_lengths[segment_i];
      const float segment_length = segment_end - segment_start;

      r_segment_indices[i] = segment_i;
      r_length_in_segment[i] = factor_in_segment * segment_length;
    }
  });
}

static void sample_indices_and_factors_to_compressed(const Span<float> accumulated_lengths,
                                                     const Span<float> sample_lengths,
                                                     const IndexMask mask,
                                                     MutableSpan<int> r_segment_indices,
                                                     MutableSpan<float> r_factor_in_segment)
{
  const float total_length = accumulated_lengths.last();
  length_parameterize::SampleSegmentHint hint;

  mask.to_best_mask_type([&](const auto mask) {
    for (const int64_t i : IndexRange(mask.size())) {
      const float length = sample_lengths[mask[i]];
      length_parameterize::sample_at_length(accumulated_lengths,
                                            std::clamp(length, 0.0f, total_length),
                                            r_segment_indices[i],
                                            r_factor_in_segment[i],
                                            &hint);
    }
  });
}

/**
 * Given an array of accumulated lengths, find the segment indices that
 * sample lengths lie on, and how far along the segment they are.
 */
class SampleFloatSegmentsFunction : public fn::MultiFunction {
 private:
  Array<float> accumulated_lengths_;

 public:
  SampleFloatSegmentsFunction(Array<float> accumulated_lengths)
      : accumulated_lengths_(std::move(accumulated_lengths))
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"Sample Curve Index"};
    signature.single_input<float>("Length");

    signature.single_output<int>("Curve Index");
    signature.single_output<float>("Length in Curve");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArraySpan<float> lengths = params.readonly_single_input<float>(0, "Length");
    MutableSpan<int> indices = params.uninitialized_single_output<int>(1, "Curve Index");
    MutableSpan<float> lengths_in_segments = params.uninitialized_single_output<float>(
        2, "Length in Curve");

    sample_indices_and_lengths(accumulated_lengths_, lengths, mask, indices, lengths_in_segments);
  }
};

class SampleCurveFunction : public fn::MultiFunction {
 private:
  /**
   * The function holds a geometry set instead of curves or a curve component reference in order
   * to maintain a ownership of the geometry while the field tree is being built and used, so
   * that the curve is not freed before the function can execute.
   */
  GeometrySet geometry_set_;

 public:
  SampleCurveFunction(GeometrySet geometry_set) : geometry_set_(std::move(geometry_set))
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Sample Curve"};
    signature.single_input<int>("Curve Index");
    signature.single_input<float>("Length");
    signature.single_output<float3>("Position");
    signature.single_output<float3>("Tangent");
    signature.single_output<float3>("Normal");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    MutableSpan<float3> sampled_positions = params.uninitialized_single_output_if_required<float3>(
        2, "Position");
    MutableSpan<float3> sampled_tangents = params.uninitialized_single_output_if_required<float3>(
        3, "Tangent");
    MutableSpan<float3> sampled_normals = params.uninitialized_single_output_if_required<float3>(
        4, "Normal");

    auto return_default = [&]() {
      if (!sampled_positions.is_empty()) {
        sampled_positions.fill_indices(mask, {0, 0, 0});
      }
      if (!sampled_tangents.is_empty()) {
        sampled_tangents.fill_indices(mask, {0, 0, 0});
      }
      if (!sampled_normals.is_empty()) {
        sampled_normals.fill_indices(mask, {0, 0, 0});
      }
    };

    if (!geometry_set_.has_curves()) {
      return return_default();
    }

    const Curves &curves_id = *geometry_set_.get_curves_for_read();
    const bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id.geometry);
    if (curves.points_num() == 0) {
      return return_default();
    }
    Span<float3> evaluated_positions = curves.evaluated_positions();
    Span<float3> evaluated_tangents;
    Span<float3> evaluated_normals;
    if (!sampled_tangents.is_empty()) {
      evaluated_tangents = curves.evaluated_tangents();
    }
    if (!sampled_normals.is_empty()) {
      evaluated_normals = curves.evaluated_normals();
    }

    const VArray<int> curve_indices = params.readonly_single_input<int>(0, "Curve Index");
    const VArraySpan<float> lengths = params.readonly_single_input<float>(1, "Length");
    const VArray<bool> cyclic = curves.cyclic();

    Array<int> indices;
    Array<float> factors;

    auto sample_curve = [&](const int curve_i, const IndexMask mask) {
      /* Store the sampled indices and factors in arrays the size of the mask.
       * Then, during interpolation, move the results back to the masked indices. */
      indices.reinitialize(mask.size());
      factors.reinitialize(mask.size());
      sample_indices_and_factors_to_compressed(
          curves.evaluated_lengths_for_curve(curve_i, cyclic[curve_i]),
          lengths,
          mask,
          indices,
          factors);

      const IndexRange evaluated_points = curves.evaluated_points_for_curve(curve_i);
      if (!sampled_positions.is_empty()) {
        length_parameterize::interpolate_to_masked<float3>(
            evaluated_positions.slice(evaluated_points),
            indices,
            factors,
            mask,
            sampled_positions);
      }
      if (!sampled_tangents.is_empty()) {
        length_parameterize::interpolate_to_masked<float3>(
            evaluated_tangents.slice(evaluated_points), indices, factors, mask, sampled_tangents);
        for (const int64_t i : mask) {
          sampled_tangents[i] = math::normalize(sampled_tangents[i]);
        }
      }
      if (!sampled_normals.is_empty()) {
        length_parameterize::interpolate_to_masked<float3>(
            evaluated_normals.slice(evaluated_points), indices, factors, mask, sampled_normals);
        for (const int64_t i : mask) {
          sampled_normals[i] = math::normalize(sampled_normals[i]);
        }
      }
    };

    if (curve_indices.is_single()) {
      sample_curve(curve_indices.get_internal_single(), mask);
    }
    else {
      MultiValueMap<int, int64_t> indices_per_curve;
      devirtualize_varray(curve_indices, [&](const auto curve_indices) {
        for (const int64_t i : mask) {
          indices_per_curve.add(curve_indices[i], i);
        }
      });

      for (const int curve_i : indices_per_curve.keys()) {
        sample_curve(curve_i, IndexMask(indices_per_curve.lookup(curve_i)));
      }
    }
  }
};

/**
 * Pre-process the lengths or factors used for the sampling, turning factors into lengths, and
 * clamping between zero and the total length of the curves. Do this as a separate operation in the
 * field tree to make the sampling simpler, and to let the evaluator optimize better.
 *
 * \todo Use a mutable single input instead when they are supported.
 */
static Field<float> get_length_input_field(GeoNodeExecParams params,
                                           const GeometryNodeCurveSampleMode mode,
                                           const float curves_total_length)
{
  if (mode == GEO_NODE_CURVE_SAMPLE_LENGTH) {
    return params.extract_input<Field<float>>("Length");
  }

  /* Convert the factor to a length. */
  Field<float> factor_field = params.get_input<Field<float>>("Factor");
  auto clamp_fn = std::make_unique<fn::CustomMF_SI_SO<float, float>>(
      __func__,
      [curves_total_length](float factor) { return factor * curves_total_length; },
      fn::CustomMF_presets::AllSpanOrSingle());

  return Field<float>(FieldOperation::Create(std::move(clamp_fn), {std::move(factor_field)}), 0);
}

static Array<float> curve_accumulated_lengths(const bke::CurvesGeometry &curves)
{
  curves.ensure_evaluated_lengths();

  Array<float> curve_lengths(curves.curves_num());
  const VArray<bool> cyclic = curves.cyclic();
  float length = 0.0f;
  for (const int i : curves.curves_range()) {
    length += curves.evaluated_length_total_for_curve(i, cyclic[i]);
    curve_lengths[i] = length;
  }
  return curve_lengths;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  if (!geometry_set.has_curves()) {
    params.set_default_remaining_outputs();
    return;
  }

  const Curves &curves_id = *geometry_set.get_curves_for_read();
  const bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id.geometry);
  if (curves.points_num() == 0) {
    params.set_default_remaining_outputs();
    return;
  }

  Array<float> curve_lengths = curve_accumulated_lengths(curves);
  const float total_length = curve_lengths.last();
  if (total_length == 0.0f) {
    params.set_default_remaining_outputs();
    return;
  }

  const NodeGeometryCurveSample &storage = node_storage(params.node());
  const GeometryNodeCurveSampleMode mode = (GeometryNodeCurveSampleMode)storage.mode;
  Field<float> length_field = get_length_input_field(params, mode, total_length);

  auto sample_fn = std::make_unique<SampleCurveFunction>(std::move(geometry_set));

  std::shared_ptr<FieldOperation> sample_op;
  if (curves.curves_num() == 1) {
    sample_op = FieldOperation::Create(std::move(sample_fn),
                                       {fn::make_constant_field<int>(0), std::move(length_field)});
  }
  else {
    auto index_fn = std::make_unique<SampleFloatSegmentsFunction>(std::move(curve_lengths));
    auto index_op = FieldOperation::Create(std::move(index_fn), {std::move(length_field)});
    sample_op = FieldOperation::Create(std::move(sample_fn),
                                       {Field<int>(index_op, 0), Field<float>(index_op, 1)});
  }

  params.set_output("Position", Field<float3>(sample_op, 0));
  params.set_output("Tangent", Field<float3>(sample_op, 1));
  params.set_output("Normal", Field<float3>(sample_op, 2));
}

}  // namespace blender::nodes::node_geo_curve_sample_cc

void register_node_type_geo_curve_sample()
{
  namespace file_ns = blender::nodes::node_geo_curve_sample_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SAMPLE_CURVE, "Sample Curve", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  node_type_init(&ntype, file_ns::node_type_init);
  node_type_update(&ntype, file_ns::node_update);
  node_type_storage(
      &ntype, "NodeGeometryCurveSample", node_free_standard_storage, node_copy_standard_storage);
  ntype.draw_buttons = file_ns::node_layout;

  nodeRegisterType(&ntype);
}
