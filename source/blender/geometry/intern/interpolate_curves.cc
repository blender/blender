/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"

#include "BLI_assert.h"
#include "BLI_length_parameterize.hh"
#include "BLI_math_vector.hh"
#include "BLI_offset_indices.hh"
#include "BLI_task.hh"

#include "DNA_customdata_types.h"

#include "GEO_interpolate_curves.hh"
#include "GEO_resample_curves.hh"

namespace blender::geometry {

using bke::CurvesGeometry;

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

struct AttributesForInterpolation {
  Vector<GVArraySpan> src_from;
  Vector<GVArraySpan> src_to;

  Vector<bke::GSpanAttributeWriter> dst;
};

/**
 * Retrieve spans from source and result attributes.
 */
static AttributesForInterpolation retrieve_attribute_spans(const Span<bke::AttributeIDRef> ids,
                                                           const CurvesGeometry &src_from_curves,
                                                           const CurvesGeometry &src_to_curves,
                                                           CurvesGeometry &dst_curves)
{
  AttributesForInterpolation result;

  const bke::AttributeAccessor src_from_attributes = src_from_curves.attributes();
  const bke::AttributeAccessor src_to_attributes = src_to_curves.attributes();
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  for (const int i : ids.index_range()) {
    eCustomDataType data_type;

    const GVArray src_from_attribute = *src_from_attributes.lookup(ids[i], bke::AttrDomain::Point);
    if (src_from_attribute) {
      data_type = bke::cpp_type_to_custom_data_type(src_from_attribute.type());

      const GVArray src_to_attribute = *src_to_attributes.lookup(
          ids[i], bke::AttrDomain::Point, data_type);

      result.src_from.append(src_from_attribute);
      result.src_to.append(src_to_attribute ? src_to_attribute : GVArraySpan{});
    }
    else {
      const GVArray src_to_attribute = *src_to_attributes.lookup(ids[i], bke::AttrDomain::Point);
      /* Attribute should exist on at least one of the geometries. */
      BLI_assert(src_to_attribute);

      data_type = bke::cpp_type_to_custom_data_type(src_to_attribute.type());

      result.src_from.append(GVArraySpan{});
      result.src_to.append(src_to_attribute);
    }

    bke::GSpanAttributeWriter dst_attribute = dst_attributes.lookup_or_add_for_write_only_span(
        ids[i], bke::AttrDomain::Point, data_type);
    result.dst.append(std::move(dst_attribute));
  }

  return result;
}

/**
 * Gather a set of all generic attribute IDs to copy to the result curves.
 */
static AttributesForInterpolation gather_point_attributes_to_interpolate(
    const CurvesGeometry &from_curves, const CurvesGeometry &to_curves, CurvesGeometry &dst_curves)
{
  VectorSet<bke::AttributeIDRef> ids;
  auto add_attribute = [&](const bke::AttributeIDRef &id, const bke::AttributeMetaData meta_data) {
    if (meta_data.domain != bke::AttrDomain::Point) {
      return true;
    }
    if (meta_data.data_type == CD_PROP_STRING) {
      return true;
    }
    if (!interpolate_attribute_to_curves(id, dst_curves.curve_type_counts())) {
      return true;
    }
    if (interpolate_attribute_to_poly_curve(id)) {
      ids.add(id);
    }
    return true;
  };

  from_curves.attributes().for_all(add_attribute);
  to_curves.attributes().for_all(add_attribute);

  /* Position is handled differently since it has non-generic interpolation for Bezier
   * curves and because the evaluated positions are cached for each evaluated point. */
  ids.remove_contained("position");

  return retrieve_attribute_spans(ids, from_curves, to_curves, dst_curves);
}

/* Resample a span of attribute values from source curves to a destination buffer. */
static void sample_curve_attribute(const bke::CurvesGeometry &src_curves,
                                   const OffsetIndices<int> dst_points_by_curve,
                                   const GSpan src_data,
                                   const IndexMask &curve_selection,
                                   const Span<int> sample_indices,
                                   const Span<float> sample_factors,
                                   GMutableSpan dst_data)
{
  const CPPType &type = src_data.type();
  BLI_assert(dst_data.type() == type);

  const OffsetIndices<int> src_points_by_curve = src_curves.points_by_curve();
  const OffsetIndices<int> src_evaluated_points_by_curve = src_curves.evaluated_points_by_curve();
  const VArray<int8_t> curve_types = src_curves.curve_types();

#ifndef NDEBUG
  const int dst_points_num = dst_data.size();
  BLI_assert(sample_indices.size() == dst_points_num);
  BLI_assert(sample_factors.size() == dst_points_num);
#endif

  bke::attribute_math::convert_to_static_type(type, [&](auto dummy) {
    using T = decltype(dummy);
    Span<T> src = src_data.typed<T>();
    MutableSpan<T> dst = dst_data.typed<T>();

    Vector<T> evaluated_data;
    curve_selection.foreach_index([&](const int i_curve) {
      const IndexRange src_points = src_points_by_curve[i_curve];
      const IndexRange dst_points = dst_points_by_curve[i_curve];

      if (curve_types[i_curve] == CURVE_TYPE_POLY) {
        length_parameterize::interpolate(src.slice(src_points),
                                         sample_indices.slice(dst_points),
                                         sample_factors.slice(dst_points),
                                         dst.slice(dst_points));
      }
      else {
        const IndexRange src_evaluated_points = src_evaluated_points_by_curve[i_curve];
        evaluated_data.reinitialize(src_evaluated_points.size());
        src_curves.interpolate_to_evaluated(
            i_curve, src.slice(src_points), evaluated_data.as_mutable_span());
        length_parameterize::interpolate(evaluated_data.as_span(),
                                         sample_indices.slice(dst_points),
                                         sample_factors.slice(dst_points),
                                         dst.slice(dst_points));
      }
    });
  });
}

template<typename T>
static void mix_arrays(const Span<T> from,
                       const Span<T> to,
                       const float mix_factor,
                       const MutableSpan<T> dst)
{
  for (const int i : dst.index_range()) {
    dst[i] = math::interpolate(from[i], to[i], mix_factor);
  }
}

static void mix_arrays(const GSpan src_from,
                       const GSpan src_to,
                       const float mix_factor,
                       const IndexMask &group_selection,
                       const OffsetIndices<int> groups,
                       const GMutableSpan dst)
{
  group_selection.foreach_index(GrainSize(32), [&](const int curve) {
    const IndexRange range = groups[curve];
    bke::attribute_math::convert_to_static_type(dst.type(), [&](auto dummy) {
      using T = decltype(dummy);
      const Span<T> from = src_from.typed<T>();
      const Span<T> to = src_to.typed<T>();
      const MutableSpan<T> dst_typed = dst.typed<T>();
      mix_arrays(from.slice(range), to.slice(range), mix_factor, dst_typed.slice(range));
    });
  });
}

void interpolate_curves(const CurvesGeometry &from_curves,
                        const CurvesGeometry &to_curves,
                        const Span<int> from_curve_indices,
                        const Span<int> to_curve_indices,
                        const IndexMask &selection,
                        const Span<bool> curve_flip_direction,
                        const float mix_factor,
                        CurvesGeometry &dst_curves)
{
  BLI_assert(from_curve_indices.size() == selection.size());
  BLI_assert(to_curve_indices.size() == selection.size());

  if (from_curves.curves_num() == 0 || to_curves.curves_num() == 0) {
    return;
  }

  const VArray<bool> from_curves_cyclic = from_curves.cyclic();
  const VArray<bool> to_curves_cyclic = to_curves.cyclic();
  const Span<float3> from_evaluated_positions = from_curves.evaluated_positions();
  const Span<float3> to_evaluated_positions = to_curves.evaluated_positions();

  /* All resampled curves are poly curves. */
  dst_curves.fill_curve_types(selection, CURVE_TYPE_POLY);

  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  AttributesForInterpolation attributes = gather_point_attributes_to_interpolate(
      from_curves, to_curves, dst_curves);

  from_curves.ensure_evaluated_lengths();
  to_curves.ensure_evaluated_lengths();

  /* Sampling arbitrary attributes works by first interpolating them to the curve's standard
   * "evaluated points" and then interpolating that result with the uniform samples. This is
   * potentially wasteful when down-sampling a curve to many fewer points. There are two possible
   * solutions: only sample the necessary points for interpolation, or first sample curve
   * parameter/segment indices and evaluate the curve directly. */
  Array<int> from_sample_indices(dst_curves.points_num());
  Array<int> to_sample_indices(dst_curves.points_num());
  Array<float> from_sample_factors(dst_curves.points_num());
  Array<float> to_sample_factors(dst_curves.points_num());

  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();

  /* Gather uniform samples based on the accumulated lengths of the original curve. */
  selection.foreach_index(GrainSize(32), [&](const int i_dst_curve, const int pos) {
    const int i_from_curve = from_curve_indices[pos];
    const int i_to_curve = to_curve_indices[pos];
    const IndexRange dst_points = dst_points_by_curve[i_dst_curve];
    const Span<float> from_lengths = from_curves.evaluated_lengths_for_curve(
        i_from_curve, from_curves_cyclic[i_from_curve]);
    const Span<float> to_lengths = to_curves.evaluated_lengths_for_curve(
        i_to_curve, to_curves_cyclic[i_to_curve]);

    if (from_lengths.is_empty()) {
      /* Handle curves with only one evaluated point. */
      from_sample_indices.as_mutable_span().slice(dst_points).fill(0);
      from_sample_factors.as_mutable_span().slice(dst_points).fill(0.0f);
    }
    else {
      length_parameterize::sample_uniform(from_lengths,
                                          !from_curves_cyclic[i_from_curve],
                                          from_sample_indices.as_mutable_span().slice(dst_points),
                                          from_sample_factors.as_mutable_span().slice(dst_points));
    }
    if (to_lengths.is_empty()) {
      /* Handle curves with only one evaluated point. */
      to_sample_indices.as_mutable_span().slice(dst_points).fill(0);
      to_sample_factors.as_mutable_span().slice(dst_points).fill(0.0f);
    }
    else {
      if (curve_flip_direction[i_dst_curve]) {
        length_parameterize::sample_uniform_reverse(
            to_lengths,
            !to_curves_cyclic[i_to_curve],
            to_sample_indices.as_mutable_span().slice(dst_points),
            to_sample_factors.as_mutable_span().slice(dst_points));
      }
      else {
        length_parameterize::sample_uniform(to_lengths,
                                            !to_curves_cyclic[i_to_curve],
                                            to_sample_indices.as_mutable_span().slice(dst_points),
                                            to_sample_factors.as_mutable_span().slice(dst_points));
      }
    }
  });

  /* For every attribute, evaluate attributes from every curve in the range in the original
   * curve's "evaluated points", then use linear interpolation to sample to the result. */
  for (const int i_attribute : attributes.dst.index_range()) {
    const GSpan src_from = attributes.src_from[i_attribute];
    const GSpan src_to = attributes.src_to[i_attribute];
    GMutableSpan dst = attributes.dst[i_attribute].span;

    /* Mix factors depend on which of the from/to curves geometries has attribute data. If
     * only one geometry has attribute data it gets the full mix weight. */
    if (!src_from.is_empty() && !src_to.is_empty()) {
      GArray<> from_samples(dst.type(), dst.size());
      GArray<> to_samples(dst.type(), dst.size());
      sample_curve_attribute(from_curves,
                             dst_points_by_curve,
                             src_from,
                             selection,
                             from_sample_indices,
                             from_sample_factors,
                             from_samples);
      sample_curve_attribute(to_curves,
                             dst_points_by_curve,
                             src_to,
                             selection,
                             to_sample_indices,
                             to_sample_factors,
                             to_samples);
      mix_arrays(from_samples, to_samples, mix_factor, selection, dst_points_by_curve, dst);
    }
    else if (!src_from.is_empty()) {
      sample_curve_attribute(from_curves,
                             dst_points_by_curve,
                             src_from,
                             selection,
                             from_sample_indices,
                             from_sample_factors,
                             dst);
    }
    else if (!src_to.is_empty()) {
      sample_curve_attribute(to_curves,
                             dst_points_by_curve,
                             src_to,
                             selection,
                             to_sample_indices,
                             to_sample_factors,
                             dst);
    }
  }

  {
    Array<float3> from_samples(dst_positions.size());
    Array<float3> to_samples(dst_positions.size());

    /* Interpolate the evaluated positions to the resampled curves. */
    sample_curve_attribute(from_curves,
                           dst_points_by_curve,
                           from_evaluated_positions,
                           selection,
                           from_sample_indices,
                           from_sample_factors,
                           from_samples.as_mutable_span());
    sample_curve_attribute(to_curves,
                           dst_points_by_curve,
                           to_evaluated_positions,
                           selection,
                           to_sample_indices,
                           to_sample_factors,
                           to_samples.as_mutable_span());

    mix_arrays(from_samples.as_span(),
               to_samples.as_span(),
               mix_factor,
               selection,
               dst_points_by_curve,
               dst_positions);
  }

  for (bke::GSpanAttributeWriter &attribute : attributes.dst) {
    attribute.finish();
  }
}

}  // namespace blender::geometry
