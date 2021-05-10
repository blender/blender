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

SplinePtr NURBSpline::copy() const
{
  return std::make_unique<NURBSpline>(*this);
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

void NURBSpline::add_point(const float3 position,
                           const float radius,
                           const float tilt,
                           const float weight)
{
  positions_.append(position);
  radii_.append(radius);
  tilts_.append(tilt);
  weights_.append(weight);
  knots_dirty_ = true;
  this->mark_cache_invalid();
}

void NURBSpline::resize(const int size)
{
  positions_.resize(size);
  radii_.resize(size);
  tilts_.resize(size);
  weights_.resize(size);
  this->mark_cache_invalid();
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
  const int length = this->size();
  const int order = order_;

  knots_.resize(this->knots_size());

  MutableSpan<float> knots = knots_;

  if (mode == NURBSpline::KnotsMode::Normal || is_cyclic_) {
    for (const int i : knots.index_range()) {
      knots[i] = static_cast<float>(i);
    }
  }
  else if (mode == NURBSpline::KnotsMode::EndPoint) {
    float k = 0.0f;
    for (const int i : IndexRange(1, knots.size())) {
      knots[i - 1] = k;
      if (i >= order && i <= length) {
        k += 1.0f;
      }
    }
  }
  else if (mode == NURBSpline::KnotsMode::Bezier) {
    BLI_assert(ELEM(order, 3, 4));
    if (order == 3) {
      float k = 0.6f;
      for (const int i : knots.index_range()) {
        if (i >= order && i <= length) {
          k += 0.5f;
        }
        knots[i] = std::floor(k);
      }
    }
    else {
      float k = 0.34f;
      for (const int i : knots.index_range()) {
        knots[i] = std::floor(k);
        k += 1.0f / 3.0f;
      }
    }
  }

  if (is_cyclic_) {
    const int b = length + order - 1;
    if (order > 2) {
      for (const int i : IndexRange(1, order - 2)) {
        if (knots[b] != knots[b - i]) {
          if (i == order - 1) {
            knots[length + order - 2] += 1.0f;
            break;
          }
        }
      }
    }

    int c = order;
    for (int i = b; i < this->knots_size(); i++) {
      knots[i] = knots[i - 1] + (knots[c] - knots[c - 1]);
      c--;
    }
  }
}

Span<float> NURBSpline::knots() const
{
  if (!knots_dirty_) {
    BLI_assert(knots_.size() == this->size() + order_);
    return knots_;
  }

  std::lock_guard lock{knots_mutex_};
  if (!knots_dirty_) {
    BLI_assert(knots_.size() == this->size() + order_);
    return knots_;
  }

  this->calculate_knots();

  knots_dirty_ = false;

  return knots_;
}

static void calculate_basis_for_point(const float parameter,
                                      const int points_len,
                                      const int order,
                                      Span<float> knots,
                                      MutableSpan<float> basis_buffer,
                                      NURBSpline::BasisCache &basis_cache)
{
  /* Clamp parameter due to floating point inaccuracy. TODO: Look into using doubles. */
  const float t = std::clamp(parameter, knots[0], knots[points_len + order - 1]);

  int start = 0;
  int end = 0;
  for (const int i : IndexRange(points_len + order - 1)) {
    const bool knots_equal = knots[i] == knots[i + 1];
    if (knots_equal || t < knots[i] || t > knots[i + 1]) {
      basis_buffer[i] = 0.0f;
      continue;
    }

    basis_buffer[i] = 1.0f;
    start = std::max(i - order - 1, 0);
    end = i;
    basis_buffer.slice(i + 1, points_len + order - 1 - i).fill(0.0f);
    break;
  }
  basis_buffer[points_len + order - 1] = 0.0f;

  for (const int i_order : IndexRange(2, order - 1)) {
    if (end + i_order >= points_len + order) {
      end = points_len + order - 1 - i_order;
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

void NURBSpline::calculate_basis_cache() const
{
  if (!basis_cache_dirty_) {
    return;
  }

  std::lock_guard lock{basis_cache_mutex_};
  if (!basis_cache_dirty_) {
    return;
  }

  const int points_len = this->size();
  const int eval_size = this->evaluated_points_size();
  BLI_assert(this->evaluated_edges_size() > 0);
  basis_cache_.resize(eval_size);

  const int order = this->order();
  Span<float> control_weights = this->weights();
  Span<float> knots = this->knots();

  MutableSpan<BasisCache> basis_cache(basis_cache_);

  /* This buffer is reused by each basis calculation to store temporary values.
   * Theoretically it could be optimized away in the future. */
  Array<float> basis_buffer(this->knots_size());

  const float start = knots[order - 1];
  const float end = is_cyclic_ ? knots[points_len + order - 1] : knots[points_len];
  const float step = (end - start) / this->evaluated_edges_size();
  float parameter = start;
  for (const int i : IndexRange(eval_size)) {
    BasisCache &basis = basis_cache[i];
    calculate_basis_for_point(
        parameter, points_len + (is_cyclic_ ? order - 1 : 0), order, knots, basis_buffer, basis);
    BLI_assert(basis.weights.size() <= order);

    for (const int j : basis.weights.index_range()) {
      const int point_index = (basis.start_index + j) % points_len;
      basis.weights[j] *= control_weights[point_index];
    }

    parameter += step;
  }

  basis_cache_dirty_ = false;
}

template<typename T>
void interpolate_to_evaluated_points_impl(Span<NURBSpline::BasisCache> weights,
                                          const blender::VArray<T> &source_data,
                                          MutableSpan<T> result_data)
{
  const int points_len = source_data.size();
  BLI_assert(result_data.size() == weights.size());
  blender::attribute_math::DefaultMixer<T> mixer(result_data);

  for (const int i : result_data.index_range()) {
    Span<float> point_weights = weights[i].weights;
    const int start_index = weights[i].start_index;

    for (const int j : point_weights.index_range()) {
      const int point_index = (start_index + j) % points_len;
      mixer.mix_in(i, source_data[point_index], point_weights[j]);
    }
  }

  mixer.finalize();
}

blender::fn::GVArrayPtr NURBSpline::interpolate_to_evaluated_points(
    const blender::fn::GVArray &source_data) const
{
  BLI_assert(source_data.size() == this->size());

  this->calculate_basis_cache();
  Span<BasisCache> weights(basis_cache_);

  blender::fn::GVArrayPtr new_varray;
  blender::attribute_math::convert_to_static_type(source_data.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<blender::attribute_math::DefaultMixer<T>>) {
      Array<T> values(this->evaluated_points_size());
      interpolate_to_evaluated_points_impl<T>(weights, source_data.typed<T>(), values);
      new_varray = std::make_unique<blender::fn::GVArray_For_ArrayContainer<Array<T>>>(
          std::move(values));
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

  blender::fn::GVArray_Typed<float3> evaluated_positions{
      this->interpolate_to_evaluated_points(blender::fn::GVArray_For_Span<float3>(positions_))};

  evaluated_positions->materialize(evaluated_position_cache_);

  position_cache_dirty_ = false;
  return evaluated_position_cache_;
}
