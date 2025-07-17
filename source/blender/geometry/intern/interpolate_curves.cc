/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_quaternion.hh"

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"

#include "BLI_array_utils.hh"
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

/* Returns a map that places each point in the sample index space.
 * The map has one additional point at the end to simplify cyclic curve mapping. */
static Array<float> build_point_to_sample_map(const Span<float3> positions,
                                              const bool cyclic,
                                              const int samples_num)
{
  const IndexRange points = positions.index_range();
  Array<float> sample_by_point(points.size() + 1);
  sample_by_point[0] = 0.0f;
  for (const int i : points.drop_front(1)) {
    sample_by_point[i] = sample_by_point[i - 1] + math::distance(positions[i - 1], positions[i]);
  }
  sample_by_point.last() = cyclic ? sample_by_point[points.size() - 1] +
                                        math::distance(positions.last(), positions.first()) :
                                    sample_by_point[points.size() - 1];

  /* If source segment lengths are zero use uniform mapping by index as a fallback. */
  constexpr float length_epsilon = 1e-4f;
  if (sample_by_point.last() <= length_epsilon) {
    array_utils::fill_index_range(sample_by_point.as_mutable_span());
  }

  const float total_length = sample_by_point.last();
  /* Factor for mapping segment length to sample index space. */
  const float length_to_sample_count = math::safe_divide(float(samples_num), total_length);
  for (float &sample_value : sample_by_point) {
    sample_value *= length_to_sample_count;
  }

  return sample_by_point;
}

static void assign_samples_to_segments(const int num_dst_points,
                                       const Span<float3> src_positions,
                                       const bool cyclic,
                                       MutableSpan<int> dst_sample_offsets)
{
  const IndexRange src_points = src_positions.index_range();
  BLI_assert(src_points.size() > 0);
  BLI_assert(num_dst_points > 0);
  BLI_assert(num_dst_points >= src_points.size());
  BLI_assert(dst_sample_offsets.size() == src_points.size() + 1);

  /* Extra points of the destination curve that need to be distributed on source segments. */
  const int num_free_samples = num_dst_points - int(src_points.size());
  const Array<float> sample_by_point = build_point_to_sample_map(
      src_positions, cyclic, num_free_samples);

  int samples_start = 0;
  for (const int src_point_i : src_points) {
    dst_sample_offsets[src_point_i] = samples_start;

    /* Use rounding to distribute samples equally over all segments. */
    const int free_samples = math::round(sample_by_point[src_point_i + 1]) -
                             math::round(sample_by_point[src_point_i]);
    samples_start += 1 + free_samples;
  }

  /* This also assigns any remaining samples in case of rounding error. */
  dst_sample_offsets.last() = num_dst_points;
}

void sample_curve_padded(const Span<float3> positions,
                         const bool cyclic,
                         MutableSpan<int> r_indices,
                         MutableSpan<float> r_factors)
{
  const int num_dst_points = r_indices.size();
  BLI_assert(r_factors.size() == num_dst_points);
  const IndexRange src_points = positions.index_range();

  if (num_dst_points == 0) {
    return;
  }
  if (num_dst_points == 1) {
    r_indices.first() = 0;
    r_factors.first() = 0.0f;
    return;
  }

  if (src_points.is_empty()) {
    return;
  }
  if (src_points.size() == 1) {
    r_indices.fill(0);
    r_factors.fill(0.0f);
    return;
  }

  /* If the destination curve has equal or more points then the excess samples are distributed
   * equally over all the segments.
   * If the destination curve is shorter the samples are placed equidistant along the source
   * segments. */
  if (num_dst_points >= src_points.size()) {
    /* First destination point in each source segment. */
    Array<int> dst_sample_offsets(src_points.size() + 1);
    assign_samples_to_segments(num_dst_points, positions, cyclic, dst_sample_offsets);

    OffsetIndices dst_samples_by_src_point = OffsetIndices<int>(dst_sample_offsets);
    for (const int src_point_i : src_points.index_range()) {
      const IndexRange samples = dst_samples_by_src_point[src_point_i];

      r_indices.slice(samples).fill(src_point_i);
      for (const int sample_i : samples.index_range()) {
        const int sample = samples[sample_i];
        const float factor = float(sample_i) / samples.size();
        r_factors[sample] = factor;
      }
    }
  }
  else {
    const Array<float> sample_by_point = build_point_to_sample_map(
        positions, cyclic, num_dst_points - (cyclic ? 0 : 1));

    for (const int src_point_i : src_points.index_range()) {
      const float sample_start = sample_by_point[src_point_i];
      const float sample_end = sample_by_point[src_point_i + 1];
      const IndexRange samples = IndexRange::from_begin_end(math::ceil(sample_start),
                                                            math::ceil(sample_end));

      for (const int sample : samples) {
        r_indices[sample] = src_point_i;
        r_factors[sample] = math::safe_divide(float(sample) - sample_start,
                                              sample_end - sample_start);
      }
    }
    if (!cyclic) {
      r_indices.last() = src_points.size() - 1;
      r_factors.last() = 0.0f;
    }
  }
}

static void reverse_samples(const int points_num,
                            MutableSpan<int> r_indices,
                            MutableSpan<float> r_factors)
{
  Vector<int> reverse_indices;
  Vector<float> reverse_factors;
  reverse_indices.reserve(r_indices.size());
  reverse_factors.reserve(r_factors.size());
  /* Indices in the last (cyclic) segment are also in the last segment when reversed. */
  for (const int i : r_indices.index_range()) {
    const int index = r_indices[i];
    const float factor = r_factors[i];
    const bool is_last_segment = index >= points_num - 1;

    if (is_last_segment && factor > 0.0f) {
      reverse_indices.append(points_num - 1);
      reverse_factors.append(1.0f - factor);
    }
  }
  /* Insert reversed indices except the last (cyclic) segment. */
  for (const int i : r_indices.index_range()) {
    const int index = r_indices[i];
    const float factor = r_factors[i];
    const bool is_last_segment = index >= points_num - 1;

    if (factor > 0.0f) {
      /* Skip the last (cyclic) segment, handled below. */
      if (is_last_segment) {
        continue;
      }
      reverse_indices.append(points_num - 2 - index);
      reverse_factors.append(1.0f - r_factors[i]);
    }
    else {
      /* Move factor 1.0 into the next segment. */
      reverse_indices.append(points_num - 1 - index);
      reverse_factors.append(0.0f);
    }
  }

  r_indices.copy_from(reverse_indices);
  r_factors.copy_from(reverse_factors);
}

void sample_curve_padded(const bke::CurvesGeometry &curves,
                         const int curve_index,
                         const bool cyclic,
                         const bool reverse,
                         MutableSpan<int> r_indices,
                         MutableSpan<float> r_factors)
{
  BLI_assert(curves.curves_range().contains(curve_index));
  BLI_assert(r_indices.size() == r_factors.size());
  const IndexRange points = curves.points_by_curve()[curve_index];
  const Span<float3> positions = curves.positions().slice(points);

  if (reverse) {
    const int points_num = positions.size();
    Array<float3> reverse_positions(points_num);
    for (const int i : reverse_positions.index_range()) {
      reverse_positions[i] = positions[points_num - 1 - i];
    }

    sample_curve_padded(reverse_positions, cyclic, r_indices, r_factors);

    reverse_samples(points_num, r_indices, r_factors);
  }
  else {
    sample_curve_padded(positions, cyclic, r_indices, r_factors);
  }
}

/**
 * Return true if the attribute should be copied/interpolated to the result curves.
 * Don't output attributes that correspond to curve types that have no curves in the result.
 */
static bool interpolate_attribute_to_curves(const StringRef attribute_id,
                                            const std::array<int, CURVE_TYPES_NUM> &type_counts)
{
  if (bke::attribute_name_is_anonymous(attribute_id)) {
    return true;
  }
  if (ELEM(attribute_id, "handle_type_left", "handle_type_right", "handle_left", "handle_right")) {
    return type_counts[CURVE_TYPE_BEZIER] != 0;
  }
  if (ELEM(attribute_id, "nurbs_weight")) {
    return type_counts[CURVE_TYPE_NURBS] != 0;
  }
  return true;
}

/**
 * Return true if the attribute should be copied to poly curves.
 */
static bool interpolate_attribute_to_poly_curve(const StringRef attribute_id)
{
  static const Set<StringRef> no_interpolation{{
      "handle_type_left",
      "handle_type_right",
      "handle_right",
      "handle_left",
      "nurbs_weight",
  }};
  return !no_interpolation.contains(attribute_id);
}

struct AttributesForInterpolation {
  Vector<GVArraySpan> src_from;
  Vector<GVArraySpan> src_to;

  Vector<bke::GSpanAttributeWriter> dst;
};

/**
 * Retrieve spans from source and result attributes.
 */
static AttributesForInterpolation retrieve_attribute_spans(const Span<StringRef> ids,
                                                           const CurvesGeometry &src_from_curves,
                                                           const CurvesGeometry &src_to_curves,
                                                           const bke::AttrDomain domain,
                                                           CurvesGeometry &dst_curves)
{
  AttributesForInterpolation result;

  const bke::AttributeAccessor src_from_attributes = src_from_curves.attributes();
  const bke::AttributeAccessor src_to_attributes = src_to_curves.attributes();
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  for (const int i : ids.index_range()) {
    bke::AttrType data_type;

    const GVArray src_from_attribute = *src_from_attributes.lookup(ids[i], domain);
    if (src_from_attribute) {
      data_type = bke::cpp_type_to_attribute_type(src_from_attribute.type());

      const GVArray src_to_attribute = *src_to_attributes.lookup(ids[i], domain, data_type);

      result.src_from.append(src_from_attribute);
      result.src_to.append(src_to_attribute ? src_to_attribute : GVArraySpan{});
    }
    else {
      const GVArray src_to_attribute = *src_to_attributes.lookup(ids[i], domain);
      /* Attribute should exist on at least one of the geometries. */
      BLI_assert(src_to_attribute);

      data_type = bke::cpp_type_to_attribute_type(src_to_attribute.type());

      result.src_from.append(GVArraySpan{});
      result.src_to.append(src_to_attribute);
    }

    bke::GSpanAttributeWriter dst_attribute = dst_attributes.lookup_or_add_for_write_span(
        ids[i], domain, data_type);
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
  VectorSet<StringRef> ids;
  auto add_attribute = [&](const bke::AttributeIter &iter) {
    if (iter.domain != bke::AttrDomain::Point) {
      return;
    }
    if (iter.data_type == bke::AttrType::String) {
      return;
    }
    if (!interpolate_attribute_to_curves(iter.name, dst_curves.curve_type_counts())) {
      return;
    }
    if (!interpolate_attribute_to_poly_curve(iter.name)) {
      return;
    }
    /* Position is handled differently since it has non-generic interpolation for Bezier
     * curves and because the evaluated positions are cached for each evaluated point. */
    if (iter.name == "position") {
      return;
    }

    ids.add(iter.name);
  };

  from_curves.attributes().foreach_attribute(add_attribute);
  to_curves.attributes().foreach_attribute(add_attribute);

  return retrieve_attribute_spans(ids, from_curves, to_curves, bke::AttrDomain::Point, dst_curves);
}

/**
 * Gather a set of all generic attribute IDs to copy to the result curves.
 */
static AttributesForInterpolation gather_curve_attributes_to_interpolate(
    const CurvesGeometry &from_curves, const CurvesGeometry &to_curves, CurvesGeometry &dst_curves)
{
  VectorSet<StringRef> ids;
  auto add_attribute = [&](const bke::AttributeIter &iter) {
    if (iter.domain != bke::AttrDomain::Curve) {
      return;
    }
    if (iter.data_type == bke::AttrType::String) {
      return;
    }
    if (bke::attribute_name_is_anonymous(iter.name)) {
      return;
    }
    /* Interpolation tool always outputs poly curves. */
    if (iter.name == "curve_type") {
      return;
    }

    ids.add(iter.name);
  };

  from_curves.attributes().foreach_attribute(add_attribute);
  to_curves.attributes().foreach_attribute(add_attribute);

  return retrieve_attribute_spans(ids, from_curves, to_curves, bke::AttrDomain::Curve, dst_curves);
}

/* Resample a span of attribute values from source curves to a destination buffer. */
static void sample_curve_attribute(const bke::CurvesGeometry &src_curves,
                                   const Span<int> src_curve_indices,
                                   const OffsetIndices<int> dst_points_by_curve,
                                   const GSpan src_data,
                                   const IndexMask &dst_curve_mask,
                                   const Span<int> dst_sample_indices,
                                   const Span<float> dst_sample_factors,
                                   GMutableSpan dst_data)
{
  const CPPType &type = src_data.type();
  BLI_assert(dst_data.type() == type);

  const OffsetIndices<int> src_points_by_curve = src_curves.points_by_curve();
  const OffsetIndices<int> src_evaluated_points_by_curve = src_curves.evaluated_points_by_curve();
  const VArray<int8_t> curve_types = src_curves.curve_types();

#ifndef NDEBUG
  const int dst_points_num = dst_data.size();
  BLI_assert(dst_sample_indices.size() == dst_points_num);
  BLI_assert(dst_sample_factors.size() == dst_points_num);
#endif

  bke::attribute_math::convert_to_static_type(type, [&](auto dummy) {
    using T = decltype(dummy);
    Span<T> src = src_data.typed<T>();
    MutableSpan<T> dst = dst_data.typed<T>();

    Vector<T> evaluated_data;
    dst_curve_mask.foreach_index([&](const int i_dst_curve, const int pos) {
      const int i_src_curve = src_curve_indices[pos];
      if (i_src_curve < 0) {
        return;
      }

      const IndexRange src_points = src_points_by_curve[i_src_curve];
      const IndexRange dst_points = dst_points_by_curve[i_dst_curve];

      if (curve_types[i_src_curve] == CURVE_TYPE_POLY) {
        length_parameterize::interpolate(src.slice(src_points),
                                         dst_sample_indices.slice(dst_points),
                                         dst_sample_factors.slice(dst_points),
                                         dst.slice(dst_points));
      }
      else {
        const IndexRange src_evaluated_points = src_evaluated_points_by_curve[i_src_curve];
        evaluated_data.reinitialize(src_evaluated_points.size());
        src_curves.interpolate_to_evaluated(
            i_src_curve, src.slice(src_points), evaluated_data.as_mutable_span());
        length_parameterize::interpolate(evaluated_data.as_span(),
                                         dst_sample_indices.slice(dst_points),
                                         dst_sample_factors.slice(dst_points),
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
  if (mix_factor == 0.0f) {
    dst.copy_from(from);
  }
  else if (mix_factor == 1.0f) {
    dst.copy_from(to);
  }
  else {
    for (const int i : dst.index_range()) {
      dst[i] = math::interpolate(from[i], to[i], mix_factor);
    }
  }
}

static void mix_arrays(const GSpan src_from,
                       const GSpan src_to,
                       const Span<float> mix_factors,
                       const IndexMask &selection,
                       const GMutableSpan dst)
{
  bke::attribute_math::convert_to_static_type(dst.type(), [&](auto dummy) {
    using T = decltype(dummy);
    const Span<T> from = src_from.typed<T>();
    const Span<T> to = src_to.typed<T>();
    const MutableSpan<T> dst_typed = dst.typed<T>();
    selection.foreach_index(GrainSize(512), [&](const int curve) {
      const float mix_factor = mix_factors[curve];
      if (mix_factor == 0.0f) {
        dst_typed[curve] = from[curve];
      }
      else if (mix_factor == 1.0f) {
        dst_typed[curve] = to[curve];
      }
      else {
        dst_typed[curve] = math::interpolate(from[curve], to[curve], mix_factor);
      }
    });
  });
}

static void mix_arrays(const GSpan src_from,
                       const GSpan src_to,
                       const Span<float> mix_factors,
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
      mix_arrays(from.slice(range), to.slice(range), mix_factors[curve], dst_typed.slice(range));
    });
  });
}

void interpolate_curves_with_samples(const CurvesGeometry &from_curves,
                                     const CurvesGeometry &to_curves,
                                     const Span<int> from_curve_indices,
                                     const Span<int> to_curve_indices,
                                     const Span<int> from_sample_indices,
                                     const Span<int> to_sample_indices,
                                     const Span<float> from_sample_factors,
                                     const Span<float> to_sample_factors,
                                     const IndexMask &dst_curve_mask,
                                     const float mix_factor,
                                     CurvesGeometry &dst_curves,
                                     IndexMaskMemory &memory)
{
  BLI_assert(from_curve_indices.size() == dst_curve_mask.size());
  BLI_assert(to_curve_indices.size() == dst_curve_mask.size());
  BLI_assert(from_sample_indices.size() == dst_curves.points_num());
  BLI_assert(to_sample_indices.size() == dst_curves.points_num());
  BLI_assert(from_sample_factors.size() == dst_curves.points_num());
  BLI_assert(to_sample_factors.size() == dst_curves.points_num());

  if (from_curves.is_empty() || to_curves.is_empty()) {
    return;
  }

  const Span<float3> from_evaluated_positions = from_curves.evaluated_positions();
  const Span<float3> to_evaluated_positions = to_curves.evaluated_positions();

  /* All resampled curves are poly curves. */
  dst_curves.fill_curve_types(dst_curve_mask, CURVE_TYPE_POLY);

  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  AttributesForInterpolation point_attributes = gather_point_attributes_to_interpolate(
      from_curves, to_curves, dst_curves);
  AttributesForInterpolation curve_attributes = gather_curve_attributes_to_interpolate(
      from_curves, to_curves, dst_curves);

  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();

  Array<bool> mix_from_to(dst_curves.curves_num());
  Array<bool> exclusive_from(dst_curves.curves_num());
  Array<bool> exclusive_to(dst_curves.curves_num());
  Array<float> mix_factors(dst_curves.curves_num());
  dst_curve_mask.foreach_index(GrainSize(512), [&](const int i_dst_curve, const int pos) {
    const int i_from_curve = from_curve_indices[pos];
    const int i_to_curve = to_curve_indices[pos];
    if (i_from_curve >= 0 && i_to_curve >= 0) {
      mix_factors[i_dst_curve] = mix_factor;
      mix_from_to[i_dst_curve] = true;
      exclusive_from[i_dst_curve] = false;
      exclusive_to[i_dst_curve] = false;
    }
    else if (i_to_curve >= 0) {
      mix_factors[i_dst_curve] = 1.0f;
      mix_from_to[i_dst_curve] = false;
      exclusive_from[i_dst_curve] = false;
      exclusive_to[i_dst_curve] = true;
    }
    else {
      mix_factors[i_dst_curve] = 0.0f;
      mix_from_to[i_dst_curve] = false;
      exclusive_from[i_dst_curve] = true;
      exclusive_to[i_dst_curve] = false;
    }
  });

  /* Curve mask contains indices that may not be valid for both "from" and "to" curves. These need
   * to be filtered out before use with the generic array utils. These masks are exclusive so that
   * each element is only mixed in by one mask. */
  const IndexMask mix_curve_mask = IndexMask::from_bools(dst_curve_mask, mix_from_to, memory);
  const IndexMask from_curve_mask = IndexMask::from_bools(dst_curve_mask, exclusive_from, memory);
  const IndexMask to_curve_mask = IndexMask::from_bools(dst_curve_mask, exclusive_to, memory);

  /* For every attribute, evaluate attributes from every curve in the range in the original
   * curve's "evaluated points", then use linear interpolation to sample to the result. */
  for (const int i_attribute : point_attributes.dst.index_range()) {
    /* Attributes that exist already on another domain can not be written to. */
    if (!point_attributes.dst[i_attribute]) {
      continue;
    }

    const GSpan src_from = point_attributes.src_from[i_attribute];
    const GSpan src_to = point_attributes.src_to[i_attribute];
    GMutableSpan dst = point_attributes.dst[i_attribute].span;

    /* Mix factors depend on which of the from/to curves geometries has attribute data. If
     * only one geometry has attribute data it gets the full mix weight. */
    if (!src_from.is_empty() && !src_to.is_empty()) {
      GArray<> from_samples(dst.type(), dst.size());
      GArray<> to_samples(dst.type(), dst.size());
      sample_curve_attribute(from_curves,
                             from_curve_indices,
                             dst_points_by_curve,
                             src_from,
                             dst_curve_mask,
                             from_sample_indices,
                             from_sample_factors,
                             from_samples);
      sample_curve_attribute(to_curves,
                             to_curve_indices,
                             dst_points_by_curve,
                             src_to,
                             dst_curve_mask,
                             to_sample_indices,
                             to_sample_factors,
                             to_samples);
      mix_arrays(from_samples, to_samples, mix_factors, dst_curve_mask, dst_points_by_curve, dst);
    }
    else if (!src_from.is_empty()) {
      sample_curve_attribute(from_curves,
                             from_curve_indices,
                             dst_points_by_curve,
                             src_from,
                             dst_curve_mask,
                             from_sample_indices,
                             from_sample_factors,
                             dst);
    }
    else if (!src_to.is_empty()) {
      sample_curve_attribute(to_curves,
                             to_curve_indices,
                             dst_points_by_curve,
                             src_to,
                             dst_curve_mask,
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
                           from_curve_indices,
                           dst_points_by_curve,
                           from_evaluated_positions,
                           dst_curve_mask,
                           from_sample_indices,
                           from_sample_factors,
                           from_samples.as_mutable_span());
    sample_curve_attribute(to_curves,
                           to_curve_indices,
                           dst_points_by_curve,
                           to_evaluated_positions,
                           dst_curve_mask,
                           to_sample_indices,
                           to_sample_factors,
                           to_samples.as_mutable_span());

    mix_arrays(from_samples.as_span(),
               to_samples.as_span(),
               mix_factors,
               dst_curve_mask,
               dst_points_by_curve,
               dst_positions);
  }

  for (const int i_attribute : curve_attributes.dst.index_range()) {
    /* Attributes that exist already on another domain can not be written to. */
    if (!curve_attributes.dst[i_attribute]) {
      continue;
    }

    const GSpan src_from = curve_attributes.src_from[i_attribute];
    const GSpan src_to = curve_attributes.src_to[i_attribute];
    GMutableSpan dst = curve_attributes.dst[i_attribute].span;

    /* Only mix "safe" attribute types for now. Other types (int, bool, etc.) are just copied from
     * the first curve of each pair. */
    const bool can_mix_attribute = ELEM(bke::cpp_type_to_custom_data_type(dst.type()),
                                        CD_PROP_FLOAT,
                                        CD_PROP_FLOAT2,
                                        CD_PROP_FLOAT3);
    if (can_mix_attribute && !src_from.is_empty() && !src_to.is_empty()) {
      array_utils::copy(GVArray::from_span(src_from), from_curve_mask, dst);
      array_utils::copy(GVArray::from_span(src_to), to_curve_mask, dst);

      GArray<> from_samples(dst.type(), dst.size());
      GArray<> to_samples(dst.type(), dst.size());
      array_utils::copy(GVArray::from_span(src_from), mix_curve_mask, from_samples);
      array_utils::copy(GVArray::from_span(src_to), mix_curve_mask, to_samples);
      mix_arrays(from_samples, to_samples, mix_factors, mix_curve_mask, dst);
    }
    else if (!src_from.is_empty()) {
      array_utils::copy(GVArray::from_span(src_from), from_curve_mask, dst);
      array_utils::copy(GVArray::from_span(src_from), mix_curve_mask, dst);
    }
    else if (!src_to.is_empty()) {
      array_utils::copy(GVArray::from_span(src_to), to_curve_mask, dst);
      array_utils::copy(GVArray::from_span(src_to), mix_curve_mask, dst);
    }
  }

  for (bke::GSpanAttributeWriter &attribute : point_attributes.dst) {
    attribute.finish();
  }
  for (bke::GSpanAttributeWriter &attribute : curve_attributes.dst) {
    attribute.finish();
  }
}

static void sample_curve_uniform(const bke::CurvesGeometry &curves,
                                 const int curve_index,
                                 const bool cyclic,
                                 const bool reverse,
                                 MutableSpan<int> r_segment_indices,
                                 MutableSpan<float> r_factors)
{
  const Span<float> segment_lengths = curves.evaluated_lengths_for_curve(curve_index, cyclic);
  if (segment_lengths.is_empty()) {
    /* Handle curves with only one evaluated point. */
    r_segment_indices.fill(0);
    r_factors.fill(0.0f);
    return;
  }

  if (reverse) {
    length_parameterize::sample_uniform_reverse(
        segment_lengths, !cyclic, r_segment_indices, r_factors);
  }
  else {
    length_parameterize::sample_uniform(segment_lengths, !cyclic, r_segment_indices, r_factors);
  }
};

void interpolate_curves(const CurvesGeometry &from_curves,
                        const CurvesGeometry &to_curves,
                        const Span<int> from_curve_indices,
                        const Span<int> to_curve_indices,
                        const IndexMask &dst_curve_mask,
                        const Span<bool> dst_curve_flip_direction,
                        const float mix_factor,
                        CurvesGeometry &dst_curves,
                        IndexMaskMemory &memory)
{
  const VArray<bool> from_curves_cyclic = from_curves.cyclic();
  const VArray<bool> to_curves_cyclic = to_curves.cyclic();
  const OffsetIndices dst_points_by_curve = dst_curves.points_by_curve();

  /* Sampling arbitrary attributes works by first interpolating them to the curve's standard
   * "evaluated points" and then interpolating that result with the uniform samples. This is
   * potentially wasteful when down-sampling a curve to many fewer points. There are two possible
   * solutions: only sample the necessary points for interpolation, or first sample curve
   * parameter/segment indices and evaluate the curve directly. */
  Array<int> from_sample_indices(dst_curves.points_num());
  Array<int> to_sample_indices(dst_curves.points_num());
  Array<float> from_sample_factors(dst_curves.points_num());
  Array<float> to_sample_factors(dst_curves.points_num());

  from_curves.ensure_evaluated_lengths();
  to_curves.ensure_evaluated_lengths();

  /* Gather uniform samples based on the accumulated lengths of the original curve. */
  dst_curve_mask.foreach_index(GrainSize(32), [&](const int i_dst_curve, const int pos) {
    const int i_from_curve = from_curve_indices[pos];
    const int i_to_curve = to_curve_indices[pos];

    const IndexRange dst_points = dst_points_by_curve[i_dst_curve];
    /* First curve is sampled in forward direction, second curve may be reversed. */
    sample_curve_uniform(from_curves,
                         i_from_curve,
                         from_curves_cyclic[i_from_curve],
                         false,
                         from_sample_indices.as_mutable_span().slice(dst_points),
                         from_sample_factors.as_mutable_span().slice(dst_points));
    sample_curve_uniform(to_curves,
                         i_to_curve,
                         to_curves_cyclic[i_to_curve],
                         dst_curve_flip_direction[i_dst_curve],
                         to_sample_indices.as_mutable_span().slice(dst_points),
                         to_sample_factors.as_mutable_span().slice(dst_points));
  });

  interpolate_curves_with_samples(from_curves,
                                  to_curves,
                                  from_curve_indices,
                                  to_curve_indices,
                                  from_sample_indices,
                                  to_sample_indices,
                                  from_sample_factors,
                                  to_sample_factors,
                                  dst_curve_mask,
                                  mix_factor,
                                  dst_curves,
                                  memory);
}

}  // namespace blender::geometry
