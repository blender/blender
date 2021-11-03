/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BLI_array.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"

#include "BKE_spline.hh"

using blender::Array;
using blender::float3;
using blender::IndexRange;
using blender::MutableSpan;
using blender::Span;
using blender::fn::GVArray;
using blender::fn::GVArray_For_ArrayContainer;
using blender::fn::GVArrayPtr;

void BezierSpline::copy_settings(Spline &dst) const
{
  BezierSpline &bezier = static_cast<BezierSpline &>(dst);
  bezier.resolution_ = resolution_;
}

void BezierSpline::copy_data(Spline &dst) const
{
  BezierSpline &bezier = static_cast<BezierSpline &>(dst);
  bezier.positions_ = positions_;
  bezier.handle_types_left_ = handle_types_left_;
  bezier.handle_positions_left_ = handle_positions_left_;
  bezier.handle_types_right_ = handle_types_right_;
  bezier.handle_positions_right_ = handle_positions_right_;
  bezier.radii_ = radii_;
  bezier.tilts_ = tilts_;
}

int BezierSpline::size() const
{
  const int size = positions_.size();
  BLI_assert(size == handle_types_left_.size());
  BLI_assert(size == handle_positions_left_.size());
  BLI_assert(size == handle_types_right_.size());
  BLI_assert(size == handle_positions_right_.size());
  BLI_assert(size == radii_.size());
  BLI_assert(size == tilts_.size());
  return size;
}

int BezierSpline::resolution() const
{
  return resolution_;
}

void BezierSpline::set_resolution(const int value)
{
  BLI_assert(value > 0);
  resolution_ = value;
  this->mark_cache_invalid();
}

/**
 * \warning Call #reallocate on the spline's attributes after adding all points.
 */
void BezierSpline::add_point(const float3 position,
                             const HandleType handle_type_left,
                             const float3 handle_position_left,
                             const HandleType handle_type_right,
                             const float3 handle_position_right,
                             const float radius,
                             const float tilt)
{
  handle_types_left_.append(handle_type_left);
  handle_positions_left_.append(handle_position_left);
  positions_.append(position);
  handle_types_right_.append(handle_type_right);
  handle_positions_right_.append(handle_position_right);
  radii_.append(radius);
  tilts_.append(tilt);
  this->mark_cache_invalid();
}

void BezierSpline::resize(const int size)
{
  handle_types_left_.resize(size);
  handle_positions_left_.resize(size);
  positions_.resize(size);
  handle_types_right_.resize(size);
  handle_positions_right_.resize(size);
  radii_.resize(size);
  tilts_.resize(size);
  this->mark_cache_invalid();
  attributes.reallocate(size);
}

MutableSpan<float3> BezierSpline::positions()
{
  return positions_;
}
Span<float3> BezierSpline::positions() const
{
  return positions_;
}
MutableSpan<float> BezierSpline::radii()
{
  return radii_;
}
Span<float> BezierSpline::radii() const
{
  return radii_;
}
MutableSpan<float> BezierSpline::tilts()
{
  return tilts_;
}
Span<float> BezierSpline::tilts() const
{
  return tilts_;
}
Span<BezierSpline::HandleType> BezierSpline::handle_types_left() const
{
  return handle_types_left_;
}
MutableSpan<BezierSpline::HandleType> BezierSpline::handle_types_left()
{
  return handle_types_left_;
}
Span<float3> BezierSpline::handle_positions_left() const
{
  this->ensure_auto_handles();
  return handle_positions_left_;
}
MutableSpan<float3> BezierSpline::handle_positions_left()
{
  this->ensure_auto_handles();
  return handle_positions_left_;
}
Span<BezierSpline::HandleType> BezierSpline::handle_types_right() const
{
  return handle_types_right_;
}
MutableSpan<BezierSpline::HandleType> BezierSpline::handle_types_right()
{
  return handle_types_right_;
}
Span<float3> BezierSpline::handle_positions_right() const
{
  this->ensure_auto_handles();
  return handle_positions_right_;
}
MutableSpan<float3> BezierSpline::handle_positions_right()
{
  this->ensure_auto_handles();
  return handle_positions_right_;
}

void BezierSpline::reverse_impl()
{
  this->handle_positions_left().reverse();
  this->handle_positions_right().reverse();
  std::swap(this->handle_positions_left_, this->handle_positions_right_);

  this->handle_types_left().reverse();
  this->handle_types_right().reverse();
  std::swap(this->handle_types_left_, this->handle_types_right_);
}

static float3 previous_position(Span<float3> positions, const bool cyclic, const int i)
{
  if (i == 0) {
    if (cyclic) {
      return positions[positions.size() - 1];
    }
    return 2.0f * positions[i] - positions[i + 1];
  }
  return positions[i - 1];
}

static float3 next_position(Span<float3> positions, const bool cyclic, const int i)
{
  if (i == positions.size() - 1) {
    if (cyclic) {
      return positions[0];
    }
    return 2.0f * positions[i] - positions[i - 1];
  }
  return positions[i + 1];
}

/**
 * Recalculate all #Auto and #Vector handles with positions automatically
 * derived from the neighboring control points.
 */
void BezierSpline::ensure_auto_handles() const
{
  if (!auto_handles_dirty_) {
    return;
  }

  std::lock_guard lock{auto_handle_mutex_};
  if (!auto_handles_dirty_) {
    return;
  }

  if (this->size() == 1) {
    auto_handles_dirty_ = false;
    return;
  }

  for (const int i : IndexRange(this->size())) {
    if (ELEM(HandleType::Auto, handle_types_left_[i], handle_types_right_[i])) {
      const float3 prev_diff = positions_[i] - previous_position(positions_, is_cyclic_, i);
      const float3 next_diff = next_position(positions_, is_cyclic_, i) - positions_[i];
      float prev_len = prev_diff.length();
      float next_len = next_diff.length();
      if (prev_len == 0.0f) {
        prev_len = 1.0f;
      }
      if (next_len == 0.0f) {
        next_len = 1.0f;
      }
      const float3 dir = next_diff / next_len + prev_diff / prev_len;

      /* This magic number is unfortunate, but comes from elsewhere in Blender. */
      const float len = dir.length() * 2.5614f;
      if (len != 0.0f) {
        if (handle_types_left_[i] == HandleType::Auto) {
          const float prev_len_clamped = std::min(prev_len, next_len * 5.0f);
          handle_positions_left_[i] = positions_[i] + dir * -(prev_len_clamped / len);
        }
        if (handle_types_right_[i] == HandleType::Auto) {
          const float next_len_clamped = std::min(next_len, prev_len * 5.0f);
          handle_positions_right_[i] = positions_[i] + dir * (next_len_clamped / len);
        }
      }
    }

    if (handle_types_left_[i] == HandleType::Vector) {
      const float3 prev = previous_position(positions_, is_cyclic_, i);
      handle_positions_left_[i] = float3::interpolate(positions_[i], prev, 1.0f / 3.0f);
    }

    if (handle_types_right_[i] == HandleType::Vector) {
      const float3 next = next_position(positions_, is_cyclic_, i);
      handle_positions_right_[i] = float3::interpolate(positions_[i], next, 1.0f / 3.0f);
    }
  }

  auto_handles_dirty_ = false;
}

void BezierSpline::translate(const blender::float3 &translation)
{
  for (float3 &position : this->positions()) {
    position += translation;
  }
  for (float3 &handle_position : this->handle_positions_left()) {
    handle_position += translation;
  }
  for (float3 &handle_position : this->handle_positions_right()) {
    handle_position += translation;
  }
  this->mark_cache_invalid();
}

void BezierSpline::transform(const blender::float4x4 &matrix)
{
  for (float3 &position : this->positions()) {
    position = matrix * position;
  }
  for (float3 &handle_position : this->handle_positions_left()) {
    handle_position = matrix * handle_position;
  }
  for (float3 &handle_position : this->handle_positions_right()) {
    handle_position = matrix * handle_position;
  }
  this->mark_cache_invalid();
}

static void set_handle_position(const float3 &position,
                                const BezierSpline::HandleType type,
                                const BezierSpline::HandleType type_other,
                                const float3 &new_value,
                                float3 &handle,
                                float3 &handle_other)
{
  /* Don't bother when the handle positions are calculated automatically anyway. */
  if (ELEM(type, BezierSpline::HandleType::Auto, BezierSpline::HandleType::Vector)) {
    return;
  }

  handle = new_value;
  if (type_other == BezierSpline::HandleType::Align) {
    /* Keep track of the old length of the opposite handle. */
    const float length = float3::distance(handle_other, position);
    /* Set the other handle to directly opposite from the current handle. */
    const float3 dir = (handle - position).normalized();
    handle_other = position - dir * length;
  }
}

/**
 * Set positions for the right handle of the control point, ensuring that
 * aligned handles stay aligned. Has no effect for auto and vector type handles.
 */
void BezierSpline::set_handle_position_right(const int index, const blender::float3 &value)
{
  set_handle_position(positions_[index],
                      handle_types_right_[index],
                      handle_types_left_[index],
                      value,
                      handle_positions_right_[index],
                      handle_positions_left_[index]);
}

/**
 * Set positions for the left handle of the control point, ensuring that
 * aligned handles stay aligned. Has no effect for auto and vector type handles.
 */
void BezierSpline::set_handle_position_left(const int index, const blender::float3 &value)
{
  set_handle_position(positions_[index],
                      handle_types_left_[index],
                      handle_types_right_[index],
                      value,
                      handle_positions_left_[index],
                      handle_positions_right_[index]);
}

bool BezierSpline::point_is_sharp(const int index) const
{
  return ELEM(handle_types_left_[index], HandleType::Vector, HandleType::Free) ||
         ELEM(handle_types_right_[index], HandleType::Vector, HandleType::Free);
}

/**
 * \warning This functional assumes that the spline has more than one point.
 */
bool BezierSpline::segment_is_vector(const int index) const
{
  /* Two control points are necessary to form a segment, that should be checked by the caller. */
  BLI_assert(this->size() > 1);

  if (index == this->size() - 1) {
    if (is_cyclic_) {
      return handle_types_right_.last() == HandleType::Vector &&
             handle_types_left_.first() == HandleType::Vector;
    }
    /* There is actually no segment in this case, but it's nice to avoid
     * having a special case for the last segment in calling code. */
    return true;
  }
  return handle_types_right_[index] == HandleType::Vector &&
         handle_types_left_[index + 1] == HandleType::Vector;
}

void BezierSpline::mark_cache_invalid()
{
  offset_cache_dirty_ = true;
  position_cache_dirty_ = true;
  mapping_cache_dirty_ = true;
  tangent_cache_dirty_ = true;
  normal_cache_dirty_ = true;
  length_cache_dirty_ = true;
  auto_handles_dirty_ = true;
}

int BezierSpline::evaluated_points_size() const
{
  BLI_assert(this->size() > 0);
  return this->control_point_offsets().last();
}

/**
 * If the spline is not cyclic, the direction for the first and last points is just the
 * direction formed by the corresponding handles and control points. In the unlikely situation
 * that the handles define a zero direction, fallback to using the direction defined by the
 * first and last evaluated segments already calculated in #Spline::evaluated_tangents().
 */
void BezierSpline::correct_end_tangents() const
{
  if (is_cyclic_) {
    return;
  }

  MutableSpan<float3> tangents(evaluated_tangents_cache_);

  if (handle_positions_right_.first() != positions_.first()) {
    tangents.first() = (handle_positions_right_.first() - positions_.first()).normalized();
  }
  if (handle_positions_left_.last() != positions_.last()) {
    tangents.last() = (positions_.last() - handle_positions_left_.last()).normalized();
  }
}

/**
 * De Casteljau Bezier subdivision.
 * \param index: The index of the segment's start control point.
 * \param next_index: The index of the control point at the end of the segment. Could be 0,
 * if the spline is cyclic.
 * \param parameter: The factor along the segment, between 0 and 1. Note that this is used
 * directly by the calculation, it doesn't correspond to a portion of the evaluated length.
 *
 * <pre>
 *           handle_prev         handle_next
 *                x----------------x
 *               /                  \
 *              /      x---O---x     \
 *             /        result        \
 *            /                        \
 *           O                          O
 *       point_prev                  point_next
 * </pre>
 */
BezierSpline::InsertResult BezierSpline::calculate_segment_insertion(const int index,
                                                                     const int next_index,
                                                                     const float parameter)
{
  BLI_assert(parameter <= 1.0f && parameter >= 0.0f);
  BLI_assert(next_index == 0 || next_index == index + 1);
  const float3 &point_prev = positions_[index];
  const float3 &handle_prev = handle_positions_right_[index];
  const float3 &handle_next = handle_positions_left_[next_index];
  const float3 &point_next = positions_[next_index];
  const float3 center_point = float3::interpolate(handle_prev, handle_next, parameter);

  BezierSpline::InsertResult result;
  result.handle_prev = float3::interpolate(point_prev, handle_prev, parameter);
  result.handle_next = float3::interpolate(handle_next, point_next, parameter);
  result.left_handle = float3::interpolate(result.handle_prev, center_point, parameter);
  result.right_handle = float3::interpolate(center_point, result.handle_next, parameter);
  result.position = float3::interpolate(result.left_handle, result.right_handle, parameter);
  return result;
}

static void bezier_forward_difference_3d(const float3 &point_0,
                                         const float3 &point_1,
                                         const float3 &point_2,
                                         const float3 &point_3,
                                         MutableSpan<float3> result)
{
  BLI_assert(result.size() > 0);
  const float inv_len = 1.0f / static_cast<float>(result.size());
  const float inv_len_squared = inv_len * inv_len;
  const float inv_len_cubed = inv_len_squared * inv_len;

  const float3 rt1 = 3.0f * (point_1 - point_0) * inv_len;
  const float3 rt2 = 3.0f * (point_0 - 2.0f * point_1 + point_2) * inv_len_squared;
  const float3 rt3 = (point_3 - point_0 + 3.0f * (point_1 - point_2)) * inv_len_cubed;

  float3 q0 = point_0;
  float3 q1 = rt1 + rt2 + rt3;
  float3 q2 = 2.0f * rt2 + 6.0f * rt3;
  float3 q3 = 6.0f * rt3;
  for (const int i : result.index_range()) {
    result[i] = q0;
    q0 += q1;
    q1 += q2;
    q2 += q3;
  }
}

void BezierSpline::evaluate_segment(const int index,
                                    const int next_index,
                                    MutableSpan<float3> positions) const
{
  if (this->segment_is_vector(index)) {
    BLI_assert(positions.size() == 1);
    positions.first() = positions_[index];
  }
  else {
    bezier_forward_difference_3d(positions_[index],
                                 handle_positions_right_[index],
                                 handle_positions_left_[next_index],
                                 positions_[next_index],
                                 positions);
  }
}

/**
 * Returns access to a cache of offsets into the evaluated point array for each control point.
 * While most control point edges generate the number of edges specified by the resolution, vector
 * segments only generate one edge.
 *
 * \note The length of the result is one greater than the number of points, so that the last item
 * is the total number of evaluated points. This is useful to avoid recalculating the size of the
 * last segment everywhere.
 */
Span<int> BezierSpline::control_point_offsets() const
{
  if (!offset_cache_dirty_) {
    return offset_cache_;
  }

  std::lock_guard lock{offset_cache_mutex_};
  if (!offset_cache_dirty_) {
    return offset_cache_;
  }

  const int size = this->size();
  offset_cache_.resize(size + 1);

  MutableSpan<int> offsets = offset_cache_;
  if (size == 1) {
    offsets.first() = 0;
    offsets.last() = 1;
  }
  else {
    int offset = 0;
    for (const int i : IndexRange(size)) {
      offsets[i] = offset;
      offset += this->segment_is_vector(i) ? 1 : resolution_;
    }
    offsets.last() = offset;
  }

  offset_cache_dirty_ = false;
  return offsets;
}

static void calculate_mappings_linear_resolution(Span<int> offsets,
                                                 const int size,
                                                 const int resolution,
                                                 const bool is_cyclic,
                                                 MutableSpan<float> r_mappings)
{
  const float first_segment_len_inv = 1.0f / offsets[1];
  for (const int i : IndexRange(0, offsets[1])) {
    r_mappings[i] = i * first_segment_len_inv;
  }

  const int grain_size = std::max(2048 / resolution, 1);
  blender::threading::parallel_for(IndexRange(1, size - 2), grain_size, [&](IndexRange range) {
    for (const int i_control_point : range) {
      const int segment_len = offsets[i_control_point + 1] - offsets[i_control_point];
      const float segment_len_inv = 1.0f / segment_len;
      for (const int i : IndexRange(segment_len)) {
        r_mappings[offsets[i_control_point] + i] = i_control_point + i * segment_len_inv;
      }
    }
  });

  if (is_cyclic) {
    const int last_segment_len = offsets[size] - offsets[size - 1];
    const float last_segment_len_inv = 1.0f / last_segment_len;
    for (const int i : IndexRange(last_segment_len)) {
      r_mappings[offsets[size - 1] + i] = size - 1 + i * last_segment_len_inv;
    }
  }
  else {
    r_mappings.last() = size - 1;
  }
}

/**
 * Returns non-owning access to an array of values containing the information necessary to
 * interpolate values from the original control points to evaluated points. The control point
 * index is the integer part of each value, and the factor used for interpolating to the next
 * control point is the remaining factional part.
 */
Span<float> BezierSpline::evaluated_mappings() const
{
  if (!mapping_cache_dirty_) {
    return evaluated_mapping_cache_;
  }

  std::lock_guard lock{mapping_cache_mutex_};
  if (!mapping_cache_dirty_) {
    return evaluated_mapping_cache_;
  }

  const int size = this->size();
  const int eval_size = this->evaluated_points_size();
  evaluated_mapping_cache_.resize(eval_size);
  MutableSpan<float> mappings = evaluated_mapping_cache_;

  if (eval_size == 1) {
    mappings.first() = 0.0f;
    mapping_cache_dirty_ = false;
    return mappings;
  }

  Span<int> offsets = this->control_point_offsets();

  calculate_mappings_linear_resolution(offsets, size, resolution_, is_cyclic_, mappings);

  mapping_cache_dirty_ = false;
  return mappings;
}

Span<float3> BezierSpline::evaluated_positions() const
{
  if (!position_cache_dirty_) {
    return evaluated_position_cache_;
  }

  std::lock_guard lock{position_cache_mutex_};
  if (!position_cache_dirty_) {
    return evaluated_position_cache_;
  }

  const int size = this->size();
  const int eval_size = this->evaluated_points_size();
  evaluated_position_cache_.resize(eval_size);

  MutableSpan<float3> positions = evaluated_position_cache_;

  if (size == 1) {
    /* Use a special case for single point splines to avoid checking in #evaluate_segment. */
    BLI_assert(eval_size == 1);
    positions.first() = positions_.first();
    position_cache_dirty_ = false;
    return positions;
  }

  this->ensure_auto_handles();

  Span<int> offsets = this->control_point_offsets();

  const int grain_size = std::max(512 / resolution_, 1);
  blender::threading::parallel_for(IndexRange(size - 1), grain_size, [&](IndexRange range) {
    for (const int i : range) {
      this->evaluate_segment(i, i + 1, positions.slice(offsets[i], offsets[i + 1] - offsets[i]));
    }
  });
  if (is_cyclic_) {
    this->evaluate_segment(
        size - 1, 0, positions.slice(offsets[size - 1], offsets[size] - offsets[size - 1]));
  }
  else {
    /* Since evaluating the bezier segment doesn't add the final point,
     * it must be added manually in the non-cyclic case. */
    positions.last() = positions_.last();
  }

  position_cache_dirty_ = false;
  return positions;
}

/**
 * Convert the data encoded in #evaulated_mappings into its parts-- the information necessary
 * to interpolate data from control points to evaluated points between them. The next control
 * point index result will not overflow the size of the control point vectors.
 */
BezierSpline::InterpolationData BezierSpline::interpolation_data_from_index_factor(
    const float index_factor) const
{
  const int size = this->size();

  if (is_cyclic_) {
    if (index_factor < size) {
      const int index = std::floor(index_factor);
      const int next_index = (index < size - 1) ? index + 1 : 0;
      return InterpolationData{index, next_index, index_factor - index};
    }
    return InterpolationData{size - 1, 0, 1.0f};
  }

  if (index_factor < size - 1) {
    const int index = std::floor(index_factor);
    const int next_index = index + 1;
    return InterpolationData{index, next_index, index_factor - index};
  }
  return InterpolationData{size - 2, size - 1, 1.0f};
}

/* Use a spline argument to avoid adding this to the header. */
template<typename T>
static void interpolate_to_evaluated_impl(const BezierSpline &spline,
                                          const blender::VArray<T> &src,
                                          MutableSpan<T> dst)
{
  BLI_assert(src.size() == spline.size());
  BLI_assert(dst.size() == spline.evaluated_points_size());
  Span<float> mappings = spline.evaluated_mappings();

  for (const int i : dst.index_range()) {
    BezierSpline::InterpolationData interp = spline.interpolation_data_from_index_factor(
        mappings[i]);

    const T &value = src[interp.control_point_index];
    const T &next_value = src[interp.next_control_point_index];

    dst[i] = blender::attribute_math::mix2(interp.factor, value, next_value);
  }
}

GVArrayPtr BezierSpline::interpolate_to_evaluated(const GVArray &src) const
{
  BLI_assert(src.size() == this->size());

  if (src.is_single()) {
    return src.shallow_copy();
  }

  const int eval_size = this->evaluated_points_size();
  if (eval_size == 1) {
    return src.shallow_copy();
  }

  GVArrayPtr new_varray;
  blender::attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<blender::attribute_math::DefaultMixer<T>>) {
      Array<T> values(eval_size);
      interpolate_to_evaluated_impl<T>(*this, src.typed<T>(), values);
      new_varray = std::make_unique<GVArray_For_ArrayContainer<Array<T>>>(std::move(values));
    }
  });

  return new_varray;
}
