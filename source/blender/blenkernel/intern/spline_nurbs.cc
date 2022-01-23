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
#include "BLI_virtual_array.hh"

#include "BKE_attribute_math.hh"
#include "BKE_spline.hh"

using blender::Array;
using blender::float3;
using blender::IndexRange;
using blender::MutableSpan;
using blender::Span;
using blender::VArray;
using blender::fn::GVArray;

void NURBSpline::copy_settings(Spline &dst) const
{
  NURBSpline &nurbs = static_cast<NURBSpline &>(dst);
  nurbs.knots_mode = knots_mode;
  nurbs.resolution_ = resolution_;
  nurbs.order_ = order_;
}

void NURBSpline::copy_data(Spline &dst) const
{
  NURBSpline &nurbs = static_cast<NURBSpline &>(dst);
  nurbs.positions_ = positions_;
  nurbs.weights_ = weights_;
  nurbs.knots_ = knots_;
  nurbs.knots_dirty_ = knots_dirty_;
  nurbs.radii_ = radii_;
  nurbs.tilts_ = tilts_;
}

int NURBSpline::size() const
{
  const int size = positions_.size();
  BLI_assert(size == radii_.size());
  BLI_assert(size == tilts_.size());
  BLI_assert(size == weights_.size());
  return size;
}

int NURBSpline::resolution() const
{
  return resolution_;
}

void NURBSpline::set_resolution(const int value)
{
  BLI_assert(value > 0);
  resolution_ = value;
  this->mark_cache_invalid();
}

uint8_t NURBSpline::order() const
{
  return order_;
}

void NURBSpline::set_order(const uint8_t value)
{
  BLI_assert(value >= 2 && value <= 6);
  order_ = value;
  this->mark_cache_invalid();
}

void NURBSpline::resize(const int size)
{
  positions_.resize(size);
  radii_.resize(size);
  tilts_.resize(size);
  weights_.resize(size);
  this->mark_cache_invalid();
  attributes.reallocate(size);
}

MutableSpan<float3> NURBSpline::positions()
{
  return positions_;
}
Span<float3> NURBSpline::positions() const
{
  return positions_;
}
MutableSpan<float> NURBSpline::radii()
{
  return radii_;
}
Span<float> NURBSpline::radii() const
{
  return radii_;
}
MutableSpan<float> NURBSpline::tilts()
{
  return tilts_;
}
Span<float> NURBSpline::tilts() const
{
  return tilts_;
}
MutableSpan<float> NURBSpline::weights()
{
  return weights_;
}
Span<float> NURBSpline::weights() const
{
  return weights_;
}

void NURBSpline::reverse_impl()
{
  this->weights().reverse();
}

void NURBSpline::mark_cache_invalid()
{
  basis_cache_dirty_ = true;
  position_cache_dirty_ = true;
  tangent_cache_dirty_ = true;
  normal_cache_dirty_ = true;
  length_cache_dirty_ = true;
}

int NURBSpline::evaluated_points_size() const
{
  if (!this->check_valid_size_and_order()) {
    return 0;
  }
  return resolution_ * this->segments_size();
}

void NURBSpline::correct_end_tangents() const
{
}

bool NURBSpline::check_valid_size_and_order() const
{
  if (this->size() < order_) {
    return false;
  }

  if (!is_cyclic_ && this->knots_mode == KnotsMode::Bezier) {
    if (order_ == 4) {
      if (this->size() < 5) {
        return false;
      }
    }
    else if (order_ != 3) {
      return false;
    }
  }

  return true;
}

int NURBSpline::knots_size() const
{
  const int size = this->size() + order_;
  return is_cyclic_ ? size + order_ - 1 : size;
}

void NURBSpline::calculate_knots() const
{
  const KnotsMode mode = this->knots_mode;
  const int order = order_;
  const bool is_bezier = mode == NURBSpline::KnotsMode::Bezier;
  const bool is_end_point = mode == NURBSpline::KnotsMode::EndPoint;
  /* Inner knots are always repeated once except on Bezier case. */
  const int repeat_inner = is_bezier ? order - 1 : 1;
  /* How many times to repeat 0.0 at the beginning of knot. */
  const int head = is_end_point && !is_cyclic_ ? order : (is_bezier ? order / 2 : 1);
  /* Number of knots replicating widths of the starting knots.
   * Covers both Cyclic and EndPoint cases. */
  const int tail = is_cyclic_ ? 2 * order - 1 : (is_end_point ? order : 0);

  knots_.resize(this->knots_size());
  MutableSpan<float> knots = knots_;

  int r = head;
  float current = 0.0f;

  for (const int i : IndexRange(knots.size() - tail)) {
    knots[i] = current;
    r--;
    if (r == 0) {
      current += 1.0;
      r = repeat_inner;
    }
  }

  const int tail_index = knots.size() - tail;
  for (const int i : IndexRange(tail)) {
    knots[tail_index + i] = current + (knots[i] - knots[0]);
  }
}

Span<float> NURBSpline::knots() const
{
  if (!knots_dirty_) {
    BLI_assert(knots_.size() == this->knots_size());
    return knots_;
  }

  std::lock_guard lock{knots_mutex_};
  if (!knots_dirty_) {
    BLI_assert(knots_.size() == this->knots_size());
    return knots_;
  }

  this->calculate_knots();

  knots_dirty_ = false;

  return knots_;
}

static void calculate_basis_for_point(const float parameter,
                                      const int size,
                                      const int order,
                                      Span<float> knots,
                                      MutableSpan<float> basis_buffer,
                                      NURBSpline::BasisCache &basis_cache)
{
  /* Clamp parameter due to floating point inaccuracy. */
  const float t = std::clamp(parameter, knots[0], knots[size + order - 1]);

  int start = 0;
  int end = 0;
  for (const int i : IndexRange(size + order - 1)) {
    const bool knots_equal = knots[i] == knots[i + 1];
    if (knots_equal || t < knots[i] || t > knots[i + 1]) {
      basis_buffer[i] = 0.0f;
      continue;
    }

    basis_buffer[i] = 1.0f;
    start = std::max(i - order - 1, 0);
    end = i;
    basis_buffer.slice(i + 1, size + order - 1 - i).fill(0.0f);
    break;
  }
  basis_buffer[size + order - 1] = 0.0f;

  for (const int i_order : IndexRange(2, order - 1)) {
    if (end + i_order >= size + order) {
      end = size + order - 1 - i_order;
    }
    for (const int i : IndexRange(start, end - start + 1)) {
      float new_basis = 0.0f;
      if (basis_buffer[i] != 0.0f) {
        new_basis += ((t - knots[i]) * basis_buffer[i]) / (knots[i + i_order - 1] - knots[i]);
      }

      if (basis_buffer[i + 1] != 0.0f) {
        new_basis += ((knots[i + i_order] - t) * basis_buffer[i + 1]) /
                     (knots[i + i_order] - knots[i + 1]);
      }

      basis_buffer[i] = new_basis;
    }
  }

  /* Shrink the range of calculated values to avoid storing unnecessary zeros. */
  while (basis_buffer[start] == 0.0f && start < end) {
    start++;
  }
  while (basis_buffer[end] == 0.0f && end > start) {
    end--;
  }

  basis_cache.weights.clear();
  basis_cache.weights.extend(basis_buffer.slice(start, end - start + 1));
  basis_cache.start_index = start;
}

Span<NURBSpline::BasisCache> NURBSpline::calculate_basis_cache() const
{
  if (!basis_cache_dirty_) {
    return basis_cache_;
  }

  std::lock_guard lock{basis_cache_mutex_};
  if (!basis_cache_dirty_) {
    return basis_cache_;
  }

  const int size = this->size();
  const int eval_size = this->evaluated_points_size();
  if (eval_size == 0) {
    return {};
  }

  basis_cache_.resize(eval_size);

  const int order = this->order();
  Span<float> control_weights = this->weights();
  Span<float> knots = this->knots();

  MutableSpan<BasisCache> basis_cache(basis_cache_);

  /* This buffer is reused by each basis calculation to store temporary values.
   * Theoretically it could be optimized away in the future. */
  Array<float> basis_buffer(this->knots_size());

  const float start = knots[order - 1];
  const float end = is_cyclic_ ? knots[size + order - 1] : knots[size];
  const float step = (end - start) / this->evaluated_edges_size();
  float parameter = start;
  for (const int i : IndexRange(eval_size)) {
    BasisCache &basis = basis_cache[i];
    calculate_basis_for_point(
        parameter, size + (is_cyclic_ ? order - 1 : 0), order, knots, basis_buffer, basis);
    BLI_assert(basis.weights.size() <= order);

    for (const int j : basis.weights.index_range()) {
      const int point_index = (basis.start_index + j) % size;
      basis.weights[j] *= control_weights[point_index];
    }

    parameter += step;
  }

  basis_cache_dirty_ = false;
  return basis_cache_;
}

template<typename T>
void interpolate_to_evaluated_impl(Span<NURBSpline::BasisCache> weights,
                                   const blender::VArray<T> &src,
                                   MutableSpan<T> dst)
{
  const int size = src.size();
  BLI_assert(dst.size() == weights.size());
  blender::attribute_math::DefaultMixer<T> mixer(dst);

  for (const int i : dst.index_range()) {
    Span<float> point_weights = weights[i].weights;
    const int start_index = weights[i].start_index;
    for (const int j : point_weights.index_range()) {
      const int point_index = (start_index + j) % size;
      mixer.mix_in(i, src[point_index], point_weights[j]);
    }
  }

  mixer.finalize();
}

GVArray NURBSpline::interpolate_to_evaluated(const GVArray &src) const
{
  BLI_assert(src.size() == this->size());

  if (src.is_single()) {
    return src;
  }

  Span<BasisCache> basis_cache = this->calculate_basis_cache();

  GVArray new_varray;
  blender::attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<blender::attribute_math::DefaultMixer<T>>) {
      Array<T> values(this->evaluated_points_size());
      interpolate_to_evaluated_impl<T>(basis_cache, src.typed<T>(), values);
      new_varray = VArray<T>::ForContainer(std::move(values));
    }
  });

  return new_varray;
}

Span<float3> NURBSpline::evaluated_positions() const
{
  if (!position_cache_dirty_) {
    return evaluated_position_cache_;
  }

  std::lock_guard lock{position_cache_mutex_};
  if (!position_cache_dirty_) {
    return evaluated_position_cache_;
  }

  const int eval_size = this->evaluated_points_size();
  evaluated_position_cache_.resize(eval_size);

  /* TODO: Avoid copying the evaluated data from the temporary array. */
  VArray<float3> evaluated = Spline::interpolate_to_evaluated(positions_.as_span());
  evaluated.materialize(evaluated_position_cache_);

  position_cache_dirty_ = false;
  return evaluated_position_cache_;
}
