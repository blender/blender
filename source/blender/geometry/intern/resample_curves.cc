/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_length_parameterize.hh"
#include "BLI_task.hh"

#include "FN_field.hh"
#include "FN_multi_function_builder.hh"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_geometry_fields.hh"

#include "GEO_resample_curves.hh"

namespace blender::geometry {

static fn::Field<int> get_count_input_max_one(const fn::Field<int> &count_field)
{
  static auto max_one_fn = mf::build::SI1_SO<int, int>(
      "Clamp Above One",
      [](int value) { return std::max(1, value); },
      mf::build::exec_presets::AllSpanOrSingle());
  return fn::Field<int>(fn::FieldOperation::Create(max_one_fn, {count_field}));
}

static fn::Field<int> get_count_input_from_length(const fn::Field<float> &length_field)
{
  static auto get_count_fn = mf::build::SI2_SO<float, float, int>(
      "Length Input to Count",
      [](const float curve_length, const float sample_length) {
        /* Find the number of sampled segments by dividing the total length by
         * the sample length. Then there is one more sampled point than segment. */
        const int count = int(curve_length / sample_length) + 1;
        return std::max(1, count);
      },
      mf::build::exec_presets::AllSpanOrSingle());

  auto get_count_op = fn::FieldOperation::Create(
      get_count_fn,
      {fn::Field<float>(std::make_shared<bke::CurveLengthFieldInput>()), length_field});

  return fn::Field<int>(std::move(get_count_op));
}

/**
 * Return true if the attribute should be copied/interpolated to the result curves.
 * Don't output attributes that correspond to curve types that have no curves in the result.
 */
static bool interpolate_attribute_to_curves(const bke::AttributeIDRef &attribute_id,
                                            const std::array<int, CURVE_TYPES_NUM> &type_counts)
{
  if (attribute_id.is_anonymous()) {
    return true;
  }
  if (ELEM(attribute_id.name(),
           "handle_type_left",
           "handle_type_right",
           "handle_left",
           "handle_right"))
  {
    return type_counts[CURVE_TYPE_BEZIER] != 0;
  }
  if (ELEM(attribute_id.name(), "nurbs_weight")) {
    return type_counts[CURVE_TYPE_NURBS] != 0;
  }
  return true;
}

/**
 * Return true if the attribute should be copied to poly curves.
 */
static bool interpolate_attribute_to_poly_curve(const bke::AttributeIDRef &attribute_id)
{
  static const Set<StringRef> no_interpolation{{
      "handle_type_left",
      "handle_type_right",
      "handle_right",
      "handle_left",
      "nurbs_weight",
  }};
  return !no_interpolation.contains(attribute_id.name());
}

/**
 * Retrieve spans from source and result attributes.
 */
static void retrieve_attribute_spans(const Span<bke::AttributeIDRef> ids,
                                     const CurvesGeometry &src_curves,
                                     CurvesGeometry &dst_curves,
                                     Vector<GSpan> &src,
                                     Vector<GMutableSpan> &dst,
                                     Vector<bke::GSpanAttributeWriter> &dst_attributes)
{
  const bke::AttributeAccessor src_attributes = src_curves.attributes();
  for (const int i : ids.index_range()) {
    const GVArray src_attribute = *src_attributes.lookup(ids[i], ATTR_DOMAIN_POINT);
    src.append(src_attribute.get_internal_span());

    const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(src_attribute.type());
    bke::GSpanAttributeWriter dst_attribute =
        dst_curves.attributes_for_write().lookup_or_add_for_write_only_span(
            ids[i], ATTR_DOMAIN_POINT, data_type);
    dst.append(dst_attribute.span);
    dst_attributes.append(std::move(dst_attribute));
  }
}

struct AttributesForInterpolation : NonCopyable, NonMovable {
  Vector<GSpan> src;
  Vector<GMutableSpan> dst;

  Vector<bke::GSpanAttributeWriter> dst_attributes;

  Vector<GSpan> src_no_interpolation;
  Vector<GMutableSpan> dst_no_interpolation;

  Span<float3> src_evaluated_tangents;
  Span<float3> src_evaluated_normals;
  MutableSpan<float3> dst_tangents;
  MutableSpan<float3> dst_normals;
};

/**
 * Gather a set of all generic attribute IDs to copy to the result curves.
 */
static void gather_point_attributes_to_interpolate(
    const CurvesGeometry &src_curves,
    CurvesGeometry &dst_curves,
    AttributesForInterpolation &result,
    const ResampleCurvesOutputAttributeIDs &output_ids)
{
  VectorSet<bke::AttributeIDRef> ids;
  VectorSet<bke::AttributeIDRef> ids_no_interpolation;
  src_curves.attributes().for_all(
      [&](const bke::AttributeIDRef &id, const bke::AttributeMetaData meta_data) {
        if (meta_data.domain != ATTR_DOMAIN_POINT) {
          return true;
        }
        if (meta_data.data_type == CD_PROP_STRING) {
          return true;
        }
        if (!interpolate_attribute_to_curves(id, dst_curves.curve_type_counts())) {
          return true;
        }
        if (interpolate_attribute_to_poly_curve(id)) {
          ids.add_new(id);
        }
        else {
          ids_no_interpolation.add_new(id);
        }
        return true;
      });

  /* Position is handled differently since it has non-generic interpolation for Bezier
   * curves and because the evaluated positions are cached for each evaluated point. */
  ids.remove_contained("position");

  retrieve_attribute_spans(
      ids, src_curves, dst_curves, result.src, result.dst, result.dst_attributes);

  /* Attributes that aren't interpolated like Bezier handles still have to be copied
   * to the result when there are any unselected curves of the corresponding type. */
  retrieve_attribute_spans(ids_no_interpolation,
                           src_curves,
                           dst_curves,
                           result.src_no_interpolation,
                           result.dst_no_interpolation,
                           result.dst_attributes);

  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  if (output_ids.tangent_id) {
    result.src_evaluated_tangents = src_curves.evaluated_tangents();
    bke::GSpanAttributeWriter dst_attribute = dst_attributes.lookup_or_add_for_write_only_span(
        output_ids.tangent_id, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3);
    result.dst_tangents = dst_attribute.span.typed<float3>();
    result.dst_attributes.append(std::move(dst_attribute));
  }
  if (output_ids.normal_id) {
    result.src_evaluated_normals = src_curves.evaluated_normals();
    bke::GSpanAttributeWriter dst_attribute = dst_attributes.lookup_or_add_for_write_only_span(
        output_ids.normal_id, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3);
    result.dst_normals = dst_attribute.span.typed<float3>();
    result.dst_attributes.append(std::move(dst_attribute));
  }
}

static void copy_or_defaults_for_unselected_curves(const CurvesGeometry &src_curves,
                                                   const Span<IndexRange> unselected_ranges,
                                                   const AttributesForInterpolation &attributes,
                                                   CurvesGeometry &dst_curves)
{
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();
  bke::curves::copy_point_data(src_points_by_curve,
                               dst_points_by_curve,
                               unselected_ranges,
                               src_curves.positions(),
                               dst_curves.positions_for_write());

  for (const int i : attributes.src.index_range()) {
    bke::curves::copy_point_data(src_points_by_curve,
                                 dst_points_by_curve,
                                 unselected_ranges,
                                 attributes.src[i],
                                 attributes.dst[i]);
  }
  for (const int i : attributes.src_no_interpolation.index_range()) {
    bke::curves::copy_point_data(src_points_by_curve,
                                 dst_points_by_curve,
                                 unselected_ranges,
                                 attributes.src_no_interpolation[i],
                                 attributes.dst_no_interpolation[i]);
  }

  if (!attributes.dst_tangents.is_empty()) {
    bke::curves::fill_points(
        dst_points_by_curve, unselected_ranges, float3(0), attributes.dst_tangents);
  }
  if (!attributes.dst_normals.is_empty()) {
    bke::curves::fill_points(
        dst_points_by_curve, unselected_ranges, float3(0), attributes.dst_normals);
  }
}

static void normalize_span(MutableSpan<float3> data)
{
  for (const int i : data.index_range()) {
    data[i] = math::normalize(data[i]);
  }
}

static void normalize_curve_point_data(const IndexMask curve_selection,
                                       const OffsetIndices<int> points_by_curve,
                                       MutableSpan<float3> data)
{
  for (const int i_curve : curve_selection) {
    normalize_span(data.slice(points_by_curve[i_curve]));
  }
}

static CurvesGeometry resample_to_uniform(const CurvesGeometry &src_curves,
                                          const fn::Field<bool> &selection_field,
                                          const fn::Field<int> &count_field,
                                          const ResampleCurvesOutputAttributeIDs &output_ids)
{
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const OffsetIndices evaluated_points_by_curve = src_curves.evaluated_points_by_curve();
  const VArray<bool> curves_cyclic = src_curves.cyclic();
  const VArray<int8_t> curve_types = src_curves.curve_types();
  const Span<float3> evaluated_positions = src_curves.evaluated_positions();

  CurvesGeometry dst_curves = bke::curves::copy_only_curve_domain(src_curves);
  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();

  const bke::CurvesFieldContext field_context{src_curves, ATTR_DOMAIN_CURVE};
  fn::FieldEvaluator evaluator{field_context, src_curves.curves_num()};
  evaluator.set_selection(selection_field);
  evaluator.add_with_destination(count_field, dst_offsets.drop_back(1));
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const Vector<IndexRange> unselected_ranges = selection.extract_ranges_invert(
      src_curves.curves_range(), nullptr);

  /* Fill the counts for the curves that aren't selected and accumulate the counts into offsets. */
  bke::curves::copy_curve_sizes(src_points_by_curve, unselected_ranges, dst_offsets);
  offset_indices::accumulate_counts_to_offsets(dst_offsets);
  dst_curves.resize(dst_offsets.last(), dst_curves.curves_num());

  /* All resampled curves are poly curves. */
  dst_curves.fill_curve_types(selection, CURVE_TYPE_POLY);

  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  AttributesForInterpolation attributes;
  gather_point_attributes_to_interpolate(src_curves, dst_curves, attributes, output_ids);

  src_curves.ensure_evaluated_lengths();

  /* Sampling arbitrary attributes works by first interpolating them to the curve's standard
   * "evaluated points" and then interpolating that result with the uniform samples. This is
   * potentially wasteful when down-sampling a curve to many fewer points. There are two possible
   * solutions: only sample the necessary points for interpolation, or first sample curve
   * parameter/segment indices and evaluate the curve directly. */
  Array<int> sample_indices(dst_curves.points_num());
  Array<float> sample_factors(dst_curves.points_num());

  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();

  /* Use a "for each group of curves: for each attribute: for each curve" pattern to work on
   * smaller sections of data that ideally fit into CPU cache better than simply one attribute at a
   * time or one curve at a time. */
  threading::parallel_for(selection.index_range(), 512, [&](IndexRange selection_range) {
    const IndexMask sliced_selection = selection.slice(selection_range);

    Vector<std::byte> evaluated_buffer;

    /* Gather uniform samples based on the accumulated lengths of the original curve. */
    for (const int i_curve : sliced_selection) {
      const bool cyclic = curves_cyclic[i_curve];
      const IndexRange dst_points = dst_points_by_curve[i_curve];
      const Span<float> lengths = src_curves.evaluated_lengths_for_curve(i_curve, cyclic);
      if (lengths.is_empty()) {
        /* Handle curves with only one evaluated point. */
        sample_indices.as_mutable_span().slice(dst_points).fill(0);
        sample_factors.as_mutable_span().slice(dst_points).fill(0.0f);
      }
      else {
        length_parameterize::sample_uniform(lengths,
                                            !curves_cyclic[i_curve],
                                            sample_indices.as_mutable_span().slice(dst_points),
                                            sample_factors.as_mutable_span().slice(dst_points));
      }
    }

    /* For every attribute, evaluate attributes from every curve in the range in the original
     * curve's "evaluated points", then use linear interpolation to sample to the result. */
    for (const int i_attribute : attributes.dst.index_range()) {
      const CPPType &type = attributes.src[i_attribute].type();
      bke::attribute_math::convert_to_static_type(type, [&](auto dummy) {
        using T = decltype(dummy);
        Span<T> src = attributes.src[i_attribute].typed<T>();
        MutableSpan<T> dst = attributes.dst[i_attribute].typed<T>();

        for (const int i_curve : sliced_selection) {
          const IndexRange src_points = src_points_by_curve[i_curve];
          const IndexRange dst_points = dst_points_by_curve[i_curve];

          if (curve_types[i_curve] == CURVE_TYPE_POLY) {
            length_parameterize::interpolate(src.slice(src_points),
                                             sample_indices.as_span().slice(dst_points),
                                             sample_factors.as_span().slice(dst_points),
                                             dst.slice(dst_points));
          }
          else {
            evaluated_buffer.reinitialize(sizeof(T) * evaluated_points_by_curve[i_curve].size());
            MutableSpan<T> evaluated = evaluated_buffer.as_mutable_span().cast<T>();
            src_curves.interpolate_to_evaluated(i_curve, src.slice(src_points), evaluated);

            length_parameterize::interpolate(evaluated.as_span(),
                                             sample_indices.as_span().slice(dst_points),
                                             sample_factors.as_span().slice(dst_points),
                                             dst.slice(dst_points));
          }
        }
      });
    }

    auto interpolate_evaluated_data = [&](const Span<float3> src, MutableSpan<float3> dst) {
      for (const int i_curve : sliced_selection) {
        const IndexRange src_points = evaluated_points_by_curve[i_curve];
        const IndexRange dst_points = dst_points_by_curve[i_curve];
        length_parameterize::interpolate(src.slice(src_points),
                                         sample_indices.as_span().slice(dst_points),
                                         sample_factors.as_span().slice(dst_points),
                                         dst.slice(dst_points));
      }
    };

    /* Interpolate the evaluated positions to the resampled curves. */
    interpolate_evaluated_data(evaluated_positions, dst_positions);

    if (!attributes.dst_tangents.is_empty()) {
      interpolate_evaluated_data(attributes.src_evaluated_tangents, attributes.dst_tangents);
      normalize_curve_point_data(sliced_selection, dst_points_by_curve, attributes.dst_tangents);
    }
    if (!attributes.dst_normals.is_empty()) {
      interpolate_evaluated_data(attributes.src_evaluated_normals, attributes.dst_normals);
      normalize_curve_point_data(sliced_selection, dst_points_by_curve, attributes.dst_normals);
    }

    /* Fill the default value for non-interpolating attributes that still must be copied. */
    for (GMutableSpan dst : attributes.dst_no_interpolation) {
      for (const int i_curve : sliced_selection) {
        const IndexRange dst_points = dst_points_by_curve[i_curve];
        dst.type().value_initialize_n(dst.slice(dst_points).data(), dst_points.size());
      }
    }
  });

  copy_or_defaults_for_unselected_curves(src_curves, unselected_ranges, attributes, dst_curves);

  for (bke::GSpanAttributeWriter &attribute : attributes.dst_attributes) {
    attribute.finish();
  }

  return dst_curves;
}

CurvesGeometry resample_to_count(const CurvesGeometry &src_curves,
                                 const fn::Field<bool> &selection_field,
                                 const fn::Field<int> &count_field,
                                 const ResampleCurvesOutputAttributeIDs &output_ids)
{
  return resample_to_uniform(
      src_curves, selection_field, get_count_input_max_one(count_field), output_ids);
}

CurvesGeometry resample_to_length(const CurvesGeometry &src_curves,
                                  const fn::Field<bool> &selection_field,
                                  const fn::Field<float> &segment_length_field,
                                  const ResampleCurvesOutputAttributeIDs &output_ids)
{
  return resample_to_uniform(
      src_curves, selection_field, get_count_input_from_length(segment_length_field), output_ids);
}

CurvesGeometry resample_to_evaluated(const CurvesGeometry &src_curves,
                                     const fn::Field<bool> &selection_field,
                                     const ResampleCurvesOutputAttributeIDs &output_ids)
{
  const OffsetIndices src_points_by_curve = src_curves.points_by_curve();
  const OffsetIndices src_evaluated_points_by_curve = src_curves.evaluated_points_by_curve();
  const Span<float3> evaluated_positions = src_curves.evaluated_positions();

  const bke::CurvesFieldContext field_context{src_curves, ATTR_DOMAIN_CURVE};
  fn::FieldEvaluator evaluator{field_context, src_curves.curves_num()};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const Vector<IndexRange> unselected_ranges = selection.extract_ranges_invert(
      src_curves.curves_range(), nullptr);

  CurvesGeometry dst_curves = bke::curves::copy_only_curve_domain(src_curves);
  dst_curves.fill_curve_types(selection, CURVE_TYPE_POLY);
  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();
  bke::curves::copy_curve_sizes(src_evaluated_points_by_curve, selection, dst_offsets);
  bke::curves::copy_curve_sizes(src_points_by_curve, unselected_ranges, dst_offsets);
  offset_indices::accumulate_counts_to_offsets(dst_offsets);
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();

  dst_curves.resize(dst_offsets.last(), dst_curves.curves_num());

  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  AttributesForInterpolation attributes;
  gather_point_attributes_to_interpolate(src_curves, dst_curves, attributes, output_ids);

  src_curves.ensure_can_interpolate_to_evaluated();
  threading::parallel_for(selection.index_range(), 512, [&](IndexRange selection_range) {
    const IndexMask sliced_selection = selection.slice(selection_range);

    /* Evaluate generic point attributes directly to the result attributes. */
    for (const int i_attribute : attributes.dst.index_range()) {
      const CPPType &type = attributes.src[i_attribute].type();
      bke::attribute_math::convert_to_static_type(type, [&](auto dummy) {
        using T = decltype(dummy);
        Span<T> src = attributes.src[i_attribute].typed<T>();
        MutableSpan<T> dst = attributes.dst[i_attribute].typed<T>();

        for (const int i_curve : sliced_selection) {
          const IndexRange src_points = src_points_by_curve[i_curve];
          const IndexRange dst_points = dst_points_by_curve[i_curve];
          src_curves.interpolate_to_evaluated(
              i_curve, src.slice(src_points), dst.slice(dst_points));
        }
      });
    }

    auto copy_evaluated_data = [&](const Span<float3> src, MutableSpan<float3> dst) {
      for (const int i_curve : sliced_selection) {
        const IndexRange src_points = src_evaluated_points_by_curve[i_curve];
        const IndexRange dst_points = dst_points_by_curve[i_curve];
        dst.slice(dst_points).copy_from(src.slice(src_points));
      }
    };

    /* Copy the evaluated positions to the selected curves. */
    copy_evaluated_data(evaluated_positions, dst_positions);

    if (!attributes.dst_tangents.is_empty()) {
      copy_evaluated_data(attributes.src_evaluated_tangents, attributes.dst_tangents);
      normalize_curve_point_data(sliced_selection, dst_points_by_curve, attributes.dst_tangents);
    }
    if (!attributes.dst_normals.is_empty()) {
      copy_evaluated_data(attributes.src_evaluated_normals, attributes.dst_normals);
      normalize_curve_point_data(sliced_selection, dst_points_by_curve, attributes.dst_normals);
    }

    /* Fill the default value for non-interpolating attributes that still must be copied. */
    for (GMutableSpan dst : attributes.dst_no_interpolation) {
      for (const int i_curve : sliced_selection) {
        const IndexRange dst_points = dst_points_by_curve[i_curve];
        dst.type().value_initialize_n(dst.slice(dst_points).data(), dst_points.size());
      }
    }
  });

  copy_or_defaults_for_unselected_curves(src_curves, unselected_ranges, attributes, dst_curves);

  for (bke::GSpanAttributeWriter &attribute : attributes.dst_attributes) {
    attribute.finish();
  }

  return dst_curves;
}

}  // namespace blender::geometry
