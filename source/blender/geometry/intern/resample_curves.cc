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
  static fn::CustomMF_SI_SO<int, int> max_one_fn(
      "Clamp Above One",
      [](int value) { return std::max(1, value); },
      fn::CustomMF_presets::AllSpanOrSingle());
  auto clamp_op = std::make_shared<fn::FieldOperation>(
      fn::FieldOperation(max_one_fn, {count_field}));

  return fn::Field<int>(std::move(clamp_op));
}

static fn::Field<int> get_count_input_from_length(const fn::Field<float> &length_field)
{
  static fn::CustomMF_SI_SI_SO<float, float, int> get_count_fn(
      "Length Input to Count",
      [](const float curve_length, const float sample_length) {
        /* Find the number of sampled segments by dividing the total length by
         * the sample length. Then there is one more sampled point than segment. */
        const int count = int(curve_length / sample_length) + 1;
        return std::max(1, count);
      },
      fn::CustomMF_presets::AllSpanOrSingle());

  auto get_count_op = std::make_shared<fn::FieldOperation>(fn::FieldOperation(
      get_count_fn,
      {fn::Field<float>(std::make_shared<bke::CurveLengthFieldInput>()), length_field}));

  return fn::Field<int>(std::move(get_count_op));
}

/**
 * Return true if the attribute should be copied/interpolated to the result curves.
 * Don't output attributes that correspond to curve types that have no curves in the result.
 */
static bool interpolate_attribute_to_curves(const bke::AttributeIDRef &attribute_id,
                                            const std::array<int, CURVE_TYPES_NUM> &type_counts)
{
  if (!attribute_id.is_named()) {
    return true;
  }
  if (ELEM(attribute_id.name(),
           "handle_type_left",
           "handle_type_right",
           "handle_left",
           "handle_right")) {
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
      "handle_position_right",
      "handle_position_left",
      "nurbs_weight",
  }};
  return !(attribute_id.is_named() && no_interpolation.contains(attribute_id.name()));
}

/**
 * Retrieve spans from source and result attributes.
 */
static void retrieve_attribute_spans(const Span<bke::AttributeIDRef> ids,
                                     const CurveComponent &src_component,
                                     CurveComponent &dst_component,
                                     Vector<GSpan> &src,
                                     Vector<GMutableSpan> &dst,
                                     Vector<bke::OutputAttribute> &dst_attributes)
{
  for (const int i : ids.index_range()) {
    GVArray src_attribute = src_component.attribute_try_get_for_read(ids[i], ATTR_DOMAIN_POINT);
    BLI_assert(src_attribute);
    src.append(src_attribute.get_internal_span());

    const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(src_attribute.type());
    bke::OutputAttribute dst_attribute = dst_component.attribute_try_get_for_output_only(
        ids[i], ATTR_DOMAIN_POINT, data_type);
    dst.append(dst_attribute.as_span());
    dst_attributes.append(std::move(dst_attribute));
  }
}

struct AttributesForInterpolation : NonCopyable, NonMovable {
  Vector<GSpan> src;
  Vector<GMutableSpan> dst;

  Vector<bke::OutputAttribute> dst_attributes;

  Vector<GSpan> src_no_interpolation;
  Vector<GMutableSpan> dst_no_interpolation;
};

/**
 * Gather a set of all generic attribute IDs to copy to the result curves.
 */
static void gather_point_attributes_to_interpolate(const CurveComponent &src_component,
                                                   CurveComponent &dst_component,
                                                   AttributesForInterpolation &result)
{
  bke::CurvesGeometry &dst_curves = bke::CurvesGeometry::wrap(
      dst_component.get_for_write()->geometry);

  VectorSet<bke::AttributeIDRef> ids;
  VectorSet<bke::AttributeIDRef> ids_no_interpolation;
  src_component.attribute_foreach(
      [&](const bke::AttributeIDRef &id, const AttributeMetaData meta_data) {
        if (meta_data.domain != ATTR_DOMAIN_POINT) {
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
      ids, src_component, dst_component, result.src, result.dst, result.dst_attributes);

  /* Attributes that aren't interpolated like Bezier handles still have to be be copied
   * to the result when there are any unselected curves of the corresponding type. */
  retrieve_attribute_spans(ids_no_interpolation,
                           src_component,
                           dst_component,
                           result.src_no_interpolation,
                           result.dst_no_interpolation,
                           result.dst_attributes);

  dst_curves.update_customdata_pointers();
}

static Curves *resample_to_uniform(const CurveComponent &src_component,
                                   const fn::Field<bool> &selection_field,
                                   const fn::Field<int> &count_field)
{
  const bke::CurvesGeometry &src_curves = bke::CurvesGeometry::wrap(
      src_component.get_for_read()->geometry);

  /* Create the new curves without any points and evaluate the final count directly
   * into the offsets array, in order to be accumulated into offsets later. */
  Curves *dst_curves_id = bke::curves_new_nomain(0, src_curves.curves_num());
  bke::CurvesGeometry &dst_curves = bke::CurvesGeometry::wrap(dst_curves_id->geometry);

  /* Directly copy curve attributes, since they stay the same (except for curve types). */
  CustomData_copy(&src_curves.curve_data,
                  &dst_curves.curve_data,
                  CD_MASK_ALL,
                  CD_DUPLICATE,
                  src_curves.curves_num());
  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();

  bke::GeometryComponentFieldContext field_context{src_component, ATTR_DOMAIN_CURVE};
  fn::FieldEvaluator evaluator{field_context, src_curves.curves_num()};
  evaluator.set_selection(selection_field);
  evaluator.add_with_destination(count_field, dst_offsets);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const Vector<IndexRange> unselected_ranges = selection.extract_ranges_invert(
      src_curves.curves_range(), nullptr);

  /* Fill the counts for the curves that aren't selected and accumulate the counts into offsets. */
  bke::curves::fill_curve_counts(src_curves, unselected_ranges, dst_offsets);
  bke::curves::accumulate_counts_to_offsets(dst_offsets);
  dst_curves.resize(dst_offsets.last(), dst_curves.curves_num());

  /* All resampled curves are poly curves. */
  dst_curves.fill_curve_types(selection, CURVE_TYPE_POLY);

  VArray<bool> curves_cyclic = src_curves.cyclic();
  VArray<int8_t> curve_types = src_curves.curve_types();
  Span<float3> evaluated_positions = src_curves.evaluated_positions();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  AttributesForInterpolation attributes;
  CurveComponent dst_component;
  dst_component.replace(dst_curves_id, GeometryOwnershipType::Editable);
  gather_point_attributes_to_interpolate(src_component, dst_component, attributes);

  src_curves.ensure_evaluated_lengths();

  /* Sampling arbitrary attributes works by first interpolating them to the curve's standard
   * "evaluated points" and then interpolating that result with the uniform samples. This is
   * potentially wasteful when down-sampling a curve to many fewer points. There are two possible
   * solutions: only sample the necessary points for interpolation, or first sample curve
   * parameter/segment indices and evaluate the curve directly. */
  Array<int> sample_indices(dst_curves.points_num());
  Array<float> sample_factors(dst_curves.points_num());

  /* Use a "for each group of curves: for each attribute: for each curve" pattern to work on
   * smaller sections of data that ideally fit into CPU cache better than simply one attribute at a
   * time or one curve at a time. */
  threading::parallel_for(selection.index_range(), 512, [&](IndexRange selection_range) {
    const IndexMask sliced_selection = selection.slice(selection_range);

    Vector<std::byte> evaluated_buffer;

    /* Gather uniform samples based on the accumulated lengths of the original curve. */
    for (const int i_curve : sliced_selection) {
      const bool cyclic = curves_cyclic[i_curve];
      const IndexRange dst_points = dst_curves.points_for_curve(i_curve);
      length_parameterize::create_uniform_samples(
          src_curves.evaluated_lengths_for_curve(i_curve, cyclic),
          curves_cyclic[i_curve],
          sample_indices.as_mutable_span().slice(dst_points),
          sample_factors.as_mutable_span().slice(dst_points));
    }

    /* For every attribute, evaluate attributes from every curve in the range in the original
     * curve's "evaluated points", then use linear interpolation to sample to the result. */
    for (const int i_attribute : attributes.dst.index_range()) {
      attribute_math::convert_to_static_type(attributes.src[i_attribute].type(), [&](auto dummy) {
        using T = decltype(dummy);
        Span<T> src = attributes.src[i_attribute].typed<T>();
        MutableSpan<T> dst = attributes.dst[i_attribute].typed<T>();

        for (const int i_curve : sliced_selection) {
          const IndexRange src_points = src_curves.points_for_curve(i_curve);
          const IndexRange dst_points = dst_curves.points_for_curve(i_curve);

          if (curve_types[i_curve] == CURVE_TYPE_POLY) {
            length_parameterize::linear_interpolation(src.slice(src_points),
                                                      sample_indices.as_span().slice(dst_points),
                                                      sample_factors.as_span().slice(dst_points),
                                                      dst.slice(dst_points));
          }
          else {
            const int evaluated_size = src_curves.evaluated_points_for_curve(i_curve).size();
            evaluated_buffer.clear();
            evaluated_buffer.resize(sizeof(T) * evaluated_size);
            MutableSpan<T> evaluated = evaluated_buffer.as_mutable_span().cast<T>();
            src_curves.interpolate_to_evaluated(i_curve, src.slice(src_points), evaluated);

            length_parameterize::linear_interpolation(evaluated.as_span(),
                                                      sample_indices.as_span().slice(dst_points),
                                                      sample_factors.as_span().slice(dst_points),
                                                      dst.slice(dst_points));
          }
        }
      });
    }

    /* Interpolate the evaluated positions to the resampled curves. */
    for (const int i_curve : sliced_selection) {
      const IndexRange src_points = src_curves.evaluated_points_for_curve(i_curve);
      const IndexRange dst_points = dst_curves.points_for_curve(i_curve);
      length_parameterize::linear_interpolation(evaluated_positions.slice(src_points),
                                                sample_indices.as_span().slice(dst_points),
                                                sample_factors.as_span().slice(dst_points),
                                                dst_positions.slice(dst_points));
    }

    /* Fill the default value for non-interpolating attributes that still must be copied. */
    for (GMutableSpan dst : attributes.dst_no_interpolation) {
      for (const int i_curve : sliced_selection) {
        const IndexRange dst_points = dst_curves.points_for_curve(i_curve);
        dst.type().value_initialize_n(dst.slice(dst_points).data(), dst_points.size());
      }
    }
  });

  /* Any attribute data from unselected curve points can be directly copied. */
  for (const int i : attributes.src.index_range()) {
    bke::curves::copy_point_data(
        src_curves, dst_curves, unselected_ranges, attributes.src[i], attributes.dst[i]);
  }
  for (const int i : attributes.src_no_interpolation.index_range()) {
    bke::curves::copy_point_data(src_curves,
                                 dst_curves,
                                 unselected_ranges,
                                 attributes.src_no_interpolation[i],
                                 attributes.dst_no_interpolation[i]);
  }

  /* Copy positions for unselected curves. */
  Span<float3> src_positions = src_curves.positions();
  bke::curves::copy_point_data(
      src_curves, dst_curves, unselected_ranges, src_positions, dst_positions);

  for (bke::OutputAttribute &attribute : attributes.dst_attributes) {
    attribute.save();
  }

  return dst_curves_id;
}

Curves *resample_to_count(const CurveComponent &src_component,
                          const fn::Field<bool> &selection_field,
                          const fn::Field<int> &count_field)
{
  return resample_to_uniform(src_component, selection_field, get_count_input_max_one(count_field));
}

Curves *resample_to_length(const CurveComponent &src_component,
                           const fn::Field<bool> &selection_field,
                           const fn::Field<float> &segment_length_field)
{
  return resample_to_uniform(
      src_component, selection_field, get_count_input_from_length(segment_length_field));
}

Curves *resample_to_evaluated(const CurveComponent &src_component,
                              const fn::Field<bool> &selection_field)
{
  const bke::CurvesGeometry &src_curves = bke::CurvesGeometry::wrap(
      src_component.get_for_read()->geometry);

  bke::GeometryComponentFieldContext field_context{src_component, ATTR_DOMAIN_CURVE};
  fn::FieldEvaluator evaluator{field_context, src_curves.curves_num()};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const Vector<IndexRange> unselected_ranges = selection.extract_ranges_invert(
      src_curves.curves_range(), nullptr);

  Curves *dst_curves_id = bke::curves_new_nomain(0, src_curves.curves_num());
  bke::CurvesGeometry &dst_curves = bke::CurvesGeometry::wrap(dst_curves_id->geometry);

  /* Directly copy curve attributes, since they stay the same (except for curve types). */
  CustomData_copy(&src_curves.curve_data,
                  &dst_curves.curve_data,
                  CD_MASK_ALL,
                  CD_DUPLICATE,
                  src_curves.curves_num());
  /* All resampled curves are poly curves. */
  dst_curves.fill_curve_types(selection, CURVE_TYPE_POLY);
  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();

  src_curves.ensure_evaluated_offsets();
  threading::parallel_for(selection.index_range(), 4096, [&](IndexRange range) {
    for (const int i : selection.slice(range)) {
      dst_offsets[i] = src_curves.evaluated_points_for_curve(i).size();
    }
  });
  bke::curves::fill_curve_counts(src_curves, unselected_ranges, dst_offsets);
  bke::curves::accumulate_counts_to_offsets(dst_offsets);

  dst_curves.resize(dst_offsets.last(), dst_curves.curves_num());

  /* Create the correct number of uniform-length samples for every selected curve. */
  Span<float3> evaluated_positions = src_curves.evaluated_positions();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  AttributesForInterpolation attributes;
  CurveComponent dst_component;
  dst_component.replace(dst_curves_id, GeometryOwnershipType::Editable);
  gather_point_attributes_to_interpolate(src_component, dst_component, attributes);

  threading::parallel_for(selection.index_range(), 512, [&](IndexRange selection_range) {
    const IndexMask sliced_selection = selection.slice(selection_range);

    /* Evaluate generic point attributes directly to the result attributes. */
    for (const int i_attribute : attributes.dst.index_range()) {
      attribute_math::convert_to_static_type(attributes.src[i_attribute].type(), [&](auto dummy) {
        using T = decltype(dummy);
        Span<T> src = attributes.src[i_attribute].typed<T>();
        MutableSpan<T> dst = attributes.dst[i_attribute].typed<T>();

        for (const int i_curve : sliced_selection) {
          const IndexRange src_points = src_curves.points_for_curve(i_curve);
          const IndexRange dst_points = dst_curves.points_for_curve(i_curve);
          src_curves.interpolate_to_evaluated(
              i_curve, src.slice(src_points), dst.slice(dst_points));
        }
      });
    }

    /* Copy the evaluated positions to the selected curves. */
    for (const int i_curve : sliced_selection) {
      const IndexRange src_points = src_curves.evaluated_points_for_curve(i_curve);
      const IndexRange dst_points = dst_curves.points_for_curve(i_curve);
      dst_positions.slice(dst_points).copy_from(evaluated_positions.slice(src_points));
    }

    /* Fill the default value for non-interpolating attributes that still must be copied. */
    for (GMutableSpan dst : attributes.dst_no_interpolation) {
      for (const int i_curve : sliced_selection) {
        const IndexRange dst_points = dst_curves.points_for_curve(i_curve);
        dst.type().value_initialize_n(dst.slice(dst_points).data(), dst_points.size());
      }
    }
  });

  /* Any attribute data from unselected curve points can be directly copied. */
  for (const int i : attributes.src.index_range()) {
    bke::curves::copy_point_data(
        src_curves, dst_curves, unselected_ranges, attributes.src[i], attributes.dst[i]);
  }
  for (const int i : attributes.src_no_interpolation.index_range()) {
    bke::curves::copy_point_data(src_curves,
                                 dst_curves,
                                 unselected_ranges,
                                 attributes.src_no_interpolation[i],
                                 attributes.dst_no_interpolation[i]);
  }

  /* Copy positions for unselected curves. */
  Span<float3> src_positions = src_curves.positions();
  bke::curves::copy_point_data(
      src_curves, dst_curves, unselected_ranges, src_positions, dst_positions);

  for (bke::OutputAttribute &attribute : attributes.dst_attributes) {
    attribute.save();
  }

  return dst_curves_id;
}

}  // namespace blender::geometry
