/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

#include "BKE_attribute_access.hh"
#include "BKE_attribute_math.hh"
#include "BKE_spline.hh"

using blender::Array;
using blender::float3;
using blender::GMutableSpan;
using blender::GSpan;
using blender::GVArray;
using blender::IndexRange;
using blender::MutableSpan;
using blender::Span;
using blender::VArray;
using blender::attribute_math::convert_to_static_type;
using blender::bke::AttributeIDRef;

CurveType Spline::type() const
{
  return type_;
}

void Spline::copy_base_settings(const Spline &src, Spline &dst)
{
  dst.normal_mode = src.normal_mode;
  dst.is_cyclic_ = src.is_cyclic_;
}

static SplinePtr create_spline(const CurveType type)
{
  switch (type) {
    case CURVE_TYPE_POLY:
      return std::make_unique<PolySpline>();
    case CURVE_TYPE_BEZIER:
      return std::make_unique<BezierSpline>();
    case CURVE_TYPE_NURBS:
      return std::make_unique<NURBSpline>();
    case CURVE_TYPE_CATMULL_ROM:
      BLI_assert_unreachable();
      return {};
  }
  BLI_assert_unreachable();
  return {};
}

SplinePtr Spline::copy() const
{
  SplinePtr dst = this->copy_without_attributes();
  dst->attributes = this->attributes;
  return dst;
}

SplinePtr Spline::copy_only_settings() const
{
  SplinePtr dst = create_spline(type_);
  this->copy_base_settings(*this, *dst);
  this->copy_settings(*dst);
  return dst;
}

SplinePtr Spline::copy_without_attributes() const
{
  SplinePtr dst = this->copy_only_settings();
  this->copy_data(*dst);

  /* Though the attributes storage is empty, it still needs to know the correct size. */
  dst->attributes.reallocate(dst->size());
  return dst;
}

void Spline::translate(const blender::float3 &translation)
{
  for (float3 &position : this->positions()) {
    position += translation;
  }
  this->mark_cache_invalid();
}

void Spline::transform(const blender::float4x4 &matrix)
{
  for (float3 &position : this->positions()) {
    position = matrix * position;
  }
  this->mark_cache_invalid();
}

void Spline::reverse()
{
  this->positions().reverse();
  this->radii().reverse();
  this->tilts().reverse();

  this->attributes.foreach_attribute(
      [&](const AttributeIDRef &id, const AttributeMetaData &meta_data) {
        std::optional<blender::GMutableSpan> attribute = this->attributes.get_for_write(id);
        if (!attribute) {
          BLI_assert_unreachable();
          return false;
        }
        convert_to_static_type(meta_data.data_type, [&](auto dummy) {
          using T = decltype(dummy);
          attribute->typed<T>().reverse();
        });
        return true;
      },
      ATTR_DOMAIN_POINT);

  this->reverse_impl();
  this->mark_cache_invalid();
}

int Spline::evaluated_edges_num() const
{
  const int eval_num = this->evaluated_points_num();
  if (eval_num < 2) {
    /* Two points are required for an edge. */
    return 0;
  }

  return this->is_cyclic_ ? eval_num : eval_num - 1;
}

float Spline::length() const
{
  Span<float> lengths = this->evaluated_lengths();
  return lengths.is_empty() ? 0.0f : this->evaluated_lengths().last();
}

int Spline::segments_num() const
{
  const int num = this->size();

  return is_cyclic_ ? num : num - 1;
}

bool Spline::is_cyclic() const
{
  return is_cyclic_;
}

void Spline::set_cyclic(const bool value)
{
  is_cyclic_ = value;
}

static void accumulate_lengths(Span<float3> positions,
                               const bool is_cyclic,
                               MutableSpan<float> lengths)
{
  using namespace blender::math;

  float length = 0.0f;
  for (const int i : IndexRange(positions.size() - 1)) {
    length += distance(positions[i], positions[i + 1]);
    lengths[i] = length;
  }
  if (is_cyclic) {
    lengths.last() = length + distance(positions.last(), positions.first());
  }
}

Span<float> Spline::evaluated_lengths() const
{
  if (!length_cache_dirty_) {
    return evaluated_lengths_cache_;
  }

  std::lock_guard lock{length_cache_mutex_};
  if (!length_cache_dirty_) {
    return evaluated_lengths_cache_;
  }

  const int total = evaluated_edges_num();
  evaluated_lengths_cache_.resize(total);
  if (total != 0) {
    Span<float3> positions = this->evaluated_positions();
    accumulate_lengths(positions, is_cyclic_, evaluated_lengths_cache_);
  }

  length_cache_dirty_ = false;
  return evaluated_lengths_cache_;
}

static float3 direction_bisect(const float3 &prev, const float3 &middle, const float3 &next)
{
  using namespace blender::math;

  const float3 dir_prev = normalize(middle - prev);
  const float3 dir_next = normalize(next - middle);

  const float3 result = normalize(dir_prev + dir_next);
  if (UNLIKELY(is_zero(result))) {
    return float3(0.0f, 0.0f, 1.0f);
  }
  return result;
}

static void calculate_tangents(Span<float3> positions,
                               const bool is_cyclic,
                               MutableSpan<float3> tangents)
{
  using namespace blender::math;

  if (positions.size() == 1) {
    tangents.first() = float3(0.0f, 0.0f, 1.0f);
    return;
  }

  for (const int i : IndexRange(1, positions.size() - 2)) {
    tangents[i] = direction_bisect(positions[i - 1], positions[i], positions[i + 1]);
  }

  if (is_cyclic) {
    const float3 &second_to_last = positions[positions.size() - 2];
    const float3 &last = positions.last();
    const float3 &first = positions.first();
    const float3 &second = positions[1];
    tangents.first() = direction_bisect(last, first, second);
    tangents.last() = direction_bisect(second_to_last, last, first);
  }
  else {
    tangents.first() = normalize(positions[1] - positions[0]);
    tangents.last() = normalize(positions.last() - positions[positions.size() - 2]);
  }
}

Span<float3> Spline::evaluated_tangents() const
{
  if (!tangent_cache_dirty_) {
    return evaluated_tangents_cache_;
  }

  std::lock_guard lock{tangent_cache_mutex_};
  if (!tangent_cache_dirty_) {
    return evaluated_tangents_cache_;
  }

  const int eval_num = this->evaluated_points_num();
  evaluated_tangents_cache_.resize(eval_num);

  Span<float3> positions = this->evaluated_positions();

  calculate_tangents(positions, is_cyclic_, evaluated_tangents_cache_);
  this->correct_end_tangents();

  tangent_cache_dirty_ = false;
  return evaluated_tangents_cache_;
}

static float3 rotate_direction_around_axis(const float3 &direction,
                                           const float3 &axis,
                                           const float angle)
{
  using namespace blender::math;

  BLI_ASSERT_UNIT_V3(direction);
  BLI_ASSERT_UNIT_V3(axis);

  const float3 axis_scaled = axis * dot(direction, axis);
  const float3 diff = direction - axis_scaled;
  const float3 cross = blender::math::cross(axis, diff);

  return axis_scaled + diff * std::cos(angle) + cross * std::sin(angle);
}

static void calculate_normals_z_up(Span<float3> tangents, MutableSpan<float3> r_normals)
{
  using namespace blender::math;

  BLI_assert(r_normals.size() == tangents.size());

  /* Same as in `vec_to_quat`. */
  const float epsilon = 1e-4f;
  for (const int i : r_normals.index_range()) {
    const float3 &tangent = tangents[i];
    if (fabsf(tangent.x) + fabsf(tangent.y) < epsilon) {
      r_normals[i] = {1.0f, 0.0f, 0.0f};
    }
    else {
      r_normals[i] = normalize(float3(tangent.y, -tangent.x, 0.0f));
    }
  }
}

/**
 * Rotate the last normal in the same way the tangent has been rotated.
 */
static float3 calculate_next_normal(const float3 &last_normal,
                                    const float3 &last_tangent,
                                    const float3 &current_tangent)
{
  using namespace blender::math;

  if (is_zero(last_tangent) || is_zero(current_tangent)) {
    return last_normal;
  }
  const float angle = angle_normalized_v3v3(last_tangent, current_tangent);
  if (angle != 0.0) {
    const float3 axis = normalize(cross(last_tangent, current_tangent));
    return rotate_direction_around_axis(last_normal, axis, angle);
  }
  return last_normal;
}

static void calculate_normals_minimum(Span<float3> tangents,
                                      const bool cyclic,
                                      MutableSpan<float3> r_normals)
{
  using namespace blender::math;
  BLI_assert(r_normals.size() == tangents.size());

  if (r_normals.is_empty()) {
    return;
  }

  const float epsilon = 1e-4f;

  /* Set initial normal. */
  const float3 &first_tangent = tangents[0];
  if (fabs(first_tangent.x) + fabs(first_tangent.y) < epsilon) {
    r_normals[0] = {1.0f, 0.0f, 0.0f};
  }
  else {
    r_normals[0] = normalize(float3(first_tangent.y, -first_tangent.x, 0.0f));
  }

  /* Forward normal with minimum twist along the entire spline. */
  for (const int i : IndexRange(1, r_normals.size() - 1)) {
    r_normals[i] = calculate_next_normal(r_normals[i - 1], tangents[i - 1], tangents[i]);
  }

  if (!cyclic) {
    return;
  }

  /* Compute how much the first normal deviates from the normal that has been forwarded along the
   * entire cyclic spline. */
  const float3 uncorrected_last_normal = calculate_next_normal(
      r_normals.last(), tangents.last(), tangents[0]);
  float correction_angle = angle_signed_on_axis_v3v3_v3(
      r_normals[0], uncorrected_last_normal, tangents[0]);
  if (correction_angle > M_PI) {
    correction_angle = correction_angle - 2 * M_PI;
  }

  /* Gradually apply correction by rotating all normals slightly. */
  const float angle_step = correction_angle / r_normals.size();
  for (const int i : r_normals.index_range()) {
    const float angle = angle_step * i;
    r_normals[i] = rotate_direction_around_axis(r_normals[i], tangents[i], angle);
  }
}

Span<float3> Spline::evaluated_normals() const
{
  if (!normal_cache_dirty_) {
    return evaluated_normals_cache_;
  }

  std::lock_guard lock{normal_cache_mutex_};
  if (!normal_cache_dirty_) {
    return evaluated_normals_cache_;
  }

  const int eval_num = this->evaluated_points_num();
  evaluated_normals_cache_.resize(eval_num);

  Span<float3> tangents = this->evaluated_tangents();
  MutableSpan<float3> normals = evaluated_normals_cache_;

  /* Only Z up normals are supported at the moment. */
  switch (this->normal_mode) {
    case NORMAL_MODE_Z_UP: {
      calculate_normals_z_up(tangents, normals);
      break;
    }
    case NORMAL_MODE_MINIMUM_TWIST: {
      calculate_normals_minimum(tangents, is_cyclic_, normals);
      break;
    }
  }

  /* Rotate the generated normals with the interpolated tilt data. */
  VArray<float> tilts = this->interpolate_to_evaluated(this->tilts());
  for (const int i : normals.index_range()) {
    normals[i] = rotate_direction_around_axis(normals[i], tangents[i], tilts[i]);
  }

  normal_cache_dirty_ = false;
  return evaluated_normals_cache_;
}

Spline::LookupResult Spline::lookup_evaluated_factor(const float factor) const
{
  return this->lookup_evaluated_length(this->length() * factor);
}

Spline::LookupResult Spline::lookup_evaluated_length(const float length) const
{
  BLI_assert(length >= 0.0f && length <= this->length());

  Span<float> lengths = this->evaluated_lengths();

  const float *offset = std::lower_bound(lengths.begin(), lengths.end(), length);
  const int index = offset - lengths.begin();
  const int next_index = (index == this->evaluated_points_num() - 1) ? 0 : index + 1;

  const float previous_length = (index == 0) ? 0.0f : lengths[index - 1];
  const float length_in_segment = length - previous_length;
  const float segment_length = lengths[index] - previous_length;
  const float factor = segment_length == 0.0f ? 0.0f : length_in_segment / segment_length;

  return LookupResult{index, next_index, factor};
}

Array<float> Spline::sample_uniform_index_factors(const int samples_num) const
{
  const Span<float> lengths = this->evaluated_lengths();

  BLI_assert(samples_num > 0);
  Array<float> samples(samples_num);

  samples[0] = 0.0f;
  if (samples_num == 1) {
    return samples;
  }

  const float total_length = this->length();
  const float sample_length = total_length / (samples_num - (is_cyclic_ ? 0 : 1));

  /* Store the length at the previous evaluated point in a variable so it can
   * start out at zero (the lengths array doesn't contain 0 for the first point). */
  float prev_length = 0.0f;
  int i_sample = 1;
  for (const int i_evaluated : IndexRange(this->evaluated_edges_num())) {
    const float length = lengths[i_evaluated];

    /* Add every sample that fits in this evaluated edge. */
    while ((sample_length * i_sample) < length && i_sample < samples_num) {
      const float factor = (sample_length * i_sample - prev_length) / (length - prev_length);
      samples[i_sample] = i_evaluated + factor;
      i_sample++;
    }

    prev_length = length;
  }

  /* Zero lengths or float inaccuracies can cause invalid values, or simply
   * skip some, so set the values that weren't completed in the main loop. */
  for (const int i : IndexRange(i_sample, samples_num - i_sample)) {
    samples[i] = float(samples_num);
  }

  if (!is_cyclic_) {
    /* In rare cases this can prevent overflow of the stored index. */
    samples.last() = lengths.size();
  }

  return samples;
}

Spline::LookupResult Spline::lookup_data_from_index_factor(const float index_factor) const
{
  const int eval_num = this->evaluated_points_num();

  if (is_cyclic_) {
    if (index_factor < eval_num) {
      const int index = std::floor(index_factor);
      const int next_index = (index < eval_num - 1) ? index + 1 : 0;
      return LookupResult{index, next_index, index_factor - index};
    }
    return LookupResult{eval_num - 1, 0, 1.0f};
  }

  if (index_factor < eval_num - 1) {
    const int index = std::floor(index_factor);
    const int next_index = index + 1;
    return LookupResult{index, next_index, index_factor - index};
  }
  return LookupResult{eval_num - 2, eval_num - 1, 1.0f};
}

void Spline::bounds_min_max(float3 &min, float3 &max, const bool use_evaluated) const
{
  Span<float3> positions = use_evaluated ? this->evaluated_positions() : this->positions();
  for (const float3 &position : positions) {
    minmax_v3v3_v3(min, max, position);
  }
}

GVArray Spline::interpolate_to_evaluated(GSpan data) const
{
  return this->interpolate_to_evaluated(GVArray::ForSpan(data));
}

void Spline::sample_with_index_factors(const GVArray &src,
                                       Span<float> index_factors,
                                       GMutableSpan dst) const
{
  BLI_assert(src.size() == this->evaluated_points_num());

  blender::attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    const VArray<T> src_typed = src.typed<T>();
    MutableSpan<T> dst_typed = dst.typed<T>();
    if (src.size() == 1) {
      dst_typed.fill(src_typed[0]);
      return;
    }
    blender::threading::parallel_for(dst_typed.index_range(), 1024, [&](IndexRange range) {
      for (const int i : range) {
        const LookupResult interp = this->lookup_data_from_index_factor(index_factors[i]);
        dst_typed[i] = blender::attribute_math::mix2(interp.factor,
                                                     src_typed[interp.evaluated_index],
                                                     src_typed[interp.next_evaluated_index]);
      }
    });
  });
}
