/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * GPU generated interpolated position and radius. Updated on attribute change.
 * One thread processes one curve.
 *
 * Equivalent of `CurvesGeometry::evaluated_positions()`.
 */

#include "draw_curves_infos.hh"

#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"
#include "gpu_shader_offset_indices_lib.glsl"

/* We workaround the lack of function pointers by using different type to overload the attribute
 * implementation. */
struct InterpPosition {
  /* Position, Radius. */
  float4 data;

  METAL_CONSTRUCTOR_1(InterpPosition, float4, data)

  static InterpPosition zero()
  {
    return InterpPosition(float4(0));
  }
};

/** Input Load. */

/* Template this function to be able to call it with only no extra argument. */
template<typename T> T input_load(int point_index)
{
  return T(0);
}
template<> InterpPosition input_load<InterpPosition>(int point_index)
{
  const auto &transform = push_constant_get(draw_curves_interpolate_position, transform);
  const auto &positions = buffer_get(draw_curves_interpolate_position, positions_buf);
  InterpPosition interp;
  interp.data.xyz = gpu_attr_load_float3(positions, int2(3, 0), point_index);
  interp.data.w = buffer_get(draw_curves_interpolate_position, radii_buf)[point_index];
  /* Bake object transform for legacy hair particle. */
  interp.data.xyz = transform_point(transform, interp.data.xyz);
  return interp;
}
template<> float4 input_load<float4>(int point_index)
{
  StoredFloat4 data = buffer_get(draw_curves_interpolate_float4_attribute,
                                 attribute_float4_buf)[point_index];
  return load_data(data);
}
template<> float3 input_load<float3>(int point_index)
{
  StoredFloat3 data = buffer_get(draw_curves_interpolate_float3_attribute,
                                 attribute_float3_buf)[point_index];
  return load_data(data);
}
template<> float2 input_load<float2>(int point_index)
{
  StoredFloat2 data = buffer_get(draw_curves_interpolate_float2_attribute,
                                 attribute_float2_buf)[point_index];
  return load_data(data);
}
template<> float input_load<float>(int point_index)
{
  StoredFloat data = buffer_get(draw_curves_interpolate_float_attribute,
                                attribute_float_buf)[point_index];
  return load_data(data);
}

/** Output Load. */

/* Template this function to be able to call it with only no extra argument. */
template<typename T> T output_load(int evaluated_point_index)
{
  return T(0);
}
template<> InterpPosition output_load<InterpPosition>(int evaluated_point_index)
{
  InterpPosition data;
  data.data = buffer_get(draw_curves_interpolate_position,
                         evaluated_positions_radii_buf)[evaluated_point_index];
  return data;
}
template<> float4 output_load<float4>(int evaluated_point_index)
{
  StoredFloat4 data = buffer_get(draw_curves_interpolate_float4_attribute,
                                 evaluated_float4_buf)[evaluated_point_index];
  return load_data(data);
}
template<> float3 output_load<float3>(int evaluated_point_index)
{
  StoredFloat3 data = buffer_get(draw_curves_interpolate_float3_attribute,
                                 evaluated_float3_buf)[evaluated_point_index];
  return load_data(data);
}
template<> float2 output_load<float2>(int evaluated_point_index)
{
  StoredFloat2 data = buffer_get(draw_curves_interpolate_float2_attribute,
                                 evaluated_float2_buf)[evaluated_point_index];
  return load_data(data);
}
template<> float output_load<float>(int evaluated_point_index)
{
  StoredFloat data = buffer_get(draw_curves_interpolate_float_attribute,
                                evaluated_float_buf)[evaluated_point_index];
  return load_data(data);
}

/** Output Write. */

void output_write(int evaluated_point_index, InterpPosition interp)
{
  /* Clamp radius to 0 to avoid negative radius due to interpolation. */
  interp.data.w = max(0.0, interp.data.w);
  buffer_get(draw_curves_interpolate_position,
             evaluated_positions_radii_buf)[evaluated_point_index] = interp.data;
}
void output_write(int evaluated_point_index, const float4 interp)
{
  buffer_get(draw_curves_interpolate_float4_attribute,
             evaluated_float4_buf)[evaluated_point_index] = as_data(interp);
}
void output_write(int evaluated_point_index, const float3 interp)
{
  buffer_get(draw_curves_interpolate_float3_attribute,
             evaluated_float3_buf)[evaluated_point_index] = as_data(interp);
}
void output_write(int evaluated_point_index, const float2 interp)
{
  buffer_get(draw_curves_interpolate_float2_attribute,
             evaluated_float2_buf)[evaluated_point_index] = as_data(interp);
}
void output_write(int evaluated_point_index, const float interp)
{
  buffer_get(draw_curves_interpolate_float_attribute,
             evaluated_float_buf)[evaluated_point_index] = as_data(interp);
}

/** Output Weighted Add. */

void output_weighted_add(int evaluated_point_index, float w, const InterpPosition src)
{
  buffer_get(draw_curves_interpolate_position,
             evaluated_positions_radii_buf)[evaluated_point_index] += src.data * w;
}

template<typename InterpType>
void output_weighted_add(int evaluated_point_index, float w, const InterpType src)
{
  InterpType dst = output_load<InterpType>(evaluated_point_index);
  dst += src * w;
  output_write(evaluated_point_index, dst);
}
template void output_weighted_add<float4>(int, float, float4);
template void output_weighted_add<float3>(int, float, float3);
template void output_weighted_add<float2>(int, float, float2);
template void output_weighted_add<float>(int, float, float);

/** Output Mul. */

template<typename InterpType> void output_mul(int evaluated_point_index, float w)
{
  InterpType dst = output_load<InterpType>(evaluated_point_index);
  dst *= w;
  output_write(evaluated_point_index, dst);
}
template void output_mul<float4>(int, float);
template void output_mul<float3>(int, float);
template void output_mul<float2>(int, float);
template void output_mul<float>(int, float);
template<> void output_mul<InterpPosition>(int evaluated_point_index, float w)
{
  buffer_get(draw_curves_interpolate_position,
             evaluated_positions_radii_buf)[evaluated_point_index] *= w;
}

/** Output Set To Zero. */

template<typename InterpType> void output_set_zero(int evaluated_point_index)
{
  output_write(evaluated_point_index, InterpType(0.0f));
}
template void output_set_zero<float4>(int);
template void output_set_zero<float3>(int);
template void output_set_zero<float2>(int);
template void output_set_zero<float>(int);
template<> void output_set_zero<InterpPosition>(int evaluated_point_index)
{
  buffer_get(draw_curves_interpolate_position,
             evaluated_positions_radii_buf)[evaluated_point_index] = float4(0.0);
}

/** Mix 4. */

InterpPosition mix4(
    InterpPosition v0, InterpPosition v1, InterpPosition v2, InterpPosition v3, float4 w)
{
  v0.data = v0.data * w.x + v1.data * w.y + v2.data * w.z + v3.data * w.w;
  return v0;
}

template<typename DataT> DataT mix4(DataT v0, DataT v1, DataT v2, DataT v3, float4 w)
{
  v0 = v0 * w.x + v1 * w.y + v2 * w.z + v3 * w.w;
  return v0;
}
template float4 mix4<float4>(float4, float4, float4, float4, float4);
template float3 mix4<float3>(float3, float3, float3, float3, float4);
template float2 mix4<float2>(float2, float2, float2, float2, float4);
template float mix4<float>(float, float, float, float, float4);

/** Utilities. */

bool curve_cyclic_get(int curve_index)
{
  const auto &use_cyclic = push_constant_get(draw_curves_data, use_cyclic);
  const auto &curves_cyclic_buf = buffer_get(draw_curves_data, curves_cyclic_buf);
  if (use_cyclic) {
    return gpu_attr_load_bool(curves_cyclic_buf, curve_index);
  }
  return false;
}

namespace catmull_rom {

float4 calculate_basis(const float parameter)
{
  /* Adapted from Cycles #catmull_rom_basis_eval function. */
  const float t = parameter;
  const float s = 1.0f - parameter;
  return 0.5f * float4(-t * s * s,
                       2.0f + t * t * (3.0f * t - 5.0f),
                       2.0f + s * s * (3.0f * s - 5.0f),
                       -s * t * t);
}

int4 get_points(uint point_id, IndexRange points, const bool cyclic)
{
  int4 point_ids = int(point_id) + int4(-1, +0, +1, +2);
  if (cyclic) {
    /* Wrap around. Note the offset by size to avoid modulo with negative values. */
    point_ids = ((point_ids + points.size()) % points.size());
  }
  else {
    point_ids = clamp(point_ids, int4(0), int4(points.size() - 1));
  }
  return points.start() + point_ids;
}

template<typename InterpType>
void evaluate_curve(const IndexRange points,
                    const IndexRange evaluated_points,
                    const int curve_index)
{
  const auto &curves_resolution_buf = buffer_get(draw_curves_data, curves_resolution_buf);
  const uint curve_resolution = curves_resolution_buf[curve_index];
  const bool is_curve_cyclic = curve_cyclic_get(curve_index);

  const uint evaluated_points_count = uint(evaluated_points.size());
  for (uint i = 0; i < evaluated_points_count; i++) {
    const int evaluated_point_id = evaluated_points.start() + int(i);
    const uint point_id = i / curve_resolution;
    const float parameter = float(i % curve_resolution) / float(curve_resolution);
    const float4 weights = calculate_basis(parameter);
    const int4 point_ids = get_points(point_id, points, is_curve_cyclic);

    InterpType p0 = input_load<InterpType>(point_ids.x);
    InterpType p1 = input_load<InterpType>(point_ids.y);
    InterpType p2 = input_load<InterpType>(point_ids.z);
    InterpType p3 = input_load<InterpType>(point_ids.w);
    InterpType result = mix4(p0, p1, p2, p3, weights);

    output_write(evaluated_point_id, result);
  }
}

template void evaluate_curve<InterpPosition>(IndexRange, IndexRange, int);
template void evaluate_curve<float>(IndexRange, IndexRange, int);
template void evaluate_curve<float2>(IndexRange, IndexRange, int);
template void evaluate_curve<float3>(IndexRange, IndexRange, int);
template void evaluate_curve<float4>(IndexRange, IndexRange, int);

}  // namespace catmull_rom

namespace bezier {

template<typename InterpType> void evaluate_segment(const int2 points, const IndexRange result)
{
  InterpType p0 = input_load<InterpType>(points.x);
  InterpType p1 = input_load<InterpType>(points.y);

  const float step = 1.0f / float(result.size());
  for (int i = 0; i < result.size(); i++) {
    output_write(result.start() + i, mix(p0, p1, float(i) * step));
  }
}

template<> void evaluate_segment<InterpPosition>(const int2 points, const IndexRange result)
{
  const auto &handles_right = buffer_get(draw_curves_interpolate_position,
                                         handles_positions_right_buf);
  const auto &handles_left = buffer_get(draw_curves_interpolate_position,
                                        handles_positions_left_buf);

  InterpPosition p0 = input_load<InterpPosition>(points.x);
  InterpPosition p1 = input_load<InterpPosition>(points.y);

  const float3 point_0 = p0.data.xyz;
  const float3 point_1 = gpu_attr_load_float3(handles_right, int2(3, 0), points.x);
  const float3 point_2 = gpu_attr_load_float3(handles_left, int2(3, 0), points.y);
  const float3 point_3 = p1.data.xyz;

  const float rad_0 = p0.data.w;
  const float rad_1 = p1.data.w;

  assert(result.size > 0);
  const float inv_len = 1.0f / float(result.size());
  const float inv_len_squared = inv_len * inv_len;
  const float inv_len_cubed = inv_len_squared * inv_len;

  const float3 rt1 = 3.0f * (point_1 - point_0) * inv_len;
  const float3 rt2 = 3.0f * (point_0 - 2.0f * point_1 + point_2) * inv_len_squared;
  const float3 rt3 = (point_3 - point_0 + 3.0f * (point_1 - point_2)) * inv_len_cubed;

  float3 q0 = point_0;
  float3 q1 = rt1 + rt2 + rt3;
  float3 q2 = 2.0f * rt2 + 6.0f * rt3;
  float3 q3 = 6.0f * rt3;
  for (int i = 0; i < result.size(); i++) {
    float rad = mix(rad_0, rad_1, float(i) * inv_len);
    InterpPosition interp;
    interp.data = float4(q0, rad);
    output_write(result.start() + i, interp);
    q0 += q1;
    q1 += q2;
    q2 += q3;
  }
}

template void evaluate_segment<float>(int2, IndexRange);
template void evaluate_segment<float2>(int2, IndexRange);
template void evaluate_segment<float3>(int2, IndexRange);
template void evaluate_segment<float4>(int2, IndexRange);

IndexRange per_curve_point_offsets_range(const IndexRange points, const int curve_index)
{
  return IndexRange(curve_index + points.start(), points.size() + 1);
}

int2 get_points(uint point_id, IndexRange points, const bool cyclic)
{
  int2 point_ids = int(point_id) + int2(+0, +1);
  if (cyclic) {
    /* Wrap around. Note the offset by size to avoid modulo with negative values. */
    point_ids = ((point_ids + points.size()) % points.size());
  }
  else {
    point_ids = clamp(point_ids, int2(0), int2(points.size() - 1));
  }
  return points.start() + point_ids;
}

template<typename InterpType>
void evaluate_curve(const IndexRange points,
                    const IndexRange evaluated_points,
                    const int curve_index)
{
  const auto &bezier_offsets_buf = buffer_get(draw_curves_data, bezier_offsets_buf);
  /* Range used for indexing bezier offsets. */
  const IndexRange offsets = per_curve_point_offsets_range(points, curve_index);
  const bool is_curve_cyclic = curve_cyclic_get(curve_index);

  for (int i = 0; i < points.size(); i++) {
    /* Bezier curves can have different number of evaluated segment per curve segment. */
    const IndexRange segment_range = offset_indices::load_range_from_buffer(bezier_offsets_buf,
                                                                            offsets.start() + i);
    const IndexRange evaluated_segment_range = evaluated_points.slice(segment_range);
    const int2 point_ids = get_points(i, points, is_curve_cyclic);

    evaluate_segment<InterpType>(point_ids, evaluated_segment_range);
  }

  if (is_curve_cyclic) {
    /* The closing point is not contained inside `bezier_offsets_buf` so we do manual copy. */
    output_write(evaluated_points.last(), output_load<InterpType>(evaluated_points.first()));
  }
}

template void evaluate_curve<InterpPosition>(IndexRange, IndexRange, int);
template void evaluate_curve<float>(IndexRange, IndexRange, int);
template void evaluate_curve<float2>(IndexRange, IndexRange, int);
template void evaluate_curve<float3>(IndexRange, IndexRange, int);
template void evaluate_curve<float4>(IndexRange, IndexRange, int);

}  // namespace bezier

template<typename InterpType>
void copy_curve_data(const IndexRange points,
                     const IndexRange evaluated_points,
                     const int curve_index)
{
  assert(points.size == evaluated_points.size);
  const bool is_curve_cyclic = curve_cyclic_get(curve_index);

  for (int i = 0; i < points.size(); i++) {
    output_write(evaluated_points.start() + i, input_load<InterpType>(points.start() + i));
  }

  if (is_curve_cyclic) {
    /* The closing point is not contained inside `bezier_offsets_buf` so we do manual copy. */
    output_write(evaluated_points.last(), output_load<InterpType>(evaluated_points.first()));
  }
}

template void copy_curve_data<InterpPosition>(IndexRange, IndexRange, int);
template void copy_curve_data<float>(IndexRange, IndexRange, int);
template void copy_curve_data<float2>(IndexRange, IndexRange, int);
template void copy_curve_data<float3>(IndexRange, IndexRange, int);
template void copy_curve_data<float4>(IndexRange, IndexRange, int);

namespace nurbs {

template<typename InterpType>
void evaluate_curve(const IndexRange points,
                    const IndexRange evaluated_points_padded,
                    const int curve_index)
{
  const auto &use_cyclic = push_constant_get(draw_curves_data, use_cyclic);
  /* Buffer aliasing to same bind point. We cannot dispatch with different type of curve. */
  const auto &curves_order_buf = buffer_get(draw_curves_data, curves_resolution_buf);
  const auto &basis_cache_offset_buf = buffer_get(draw_curves_data, bezier_offsets_buf);

  const int order = int(gpu_attr_load_uchar(curves_order_buf, curve_index));

  const int basis_cache_start = basis_cache_offset_buf[curve_index];
  const bool invalid = basis_cache_start < 0;

  if (invalid) {
    copy_curve_data<InterpType>(points, evaluated_points_padded, curve_index);
    return;
  }

  const auto &use_point_weight = push_constant_get(draw_curves_data, use_point_weight);
  const bool is_curve_cyclic = curve_cyclic_get(curve_index);

  /* Recover original points range without closing cyclic point. */
  const IndexRange evaluated_points = IndexRange(evaluated_points_padded.start(),
                                                 evaluated_points_padded.size() - int(use_cyclic));

  const int start_indices_range_start = basis_cache_start;
  const int weights_range_start = basis_cache_start + evaluated_points.size();

  /* Buffer aliasing to same bind point. We cannot dispatch with different type of curve. */
  const auto &basis_cache_buf = buffer_get(draw_curves_data, handles_positions_left_buf);
  const auto &control_weights_buf = buffer_get(draw_curves_data, handles_positions_right_buf);

  for (int i = 0; i < evaluated_points.size(); i++) {
    int evaluated_point_index = evaluated_points.start() + i;
    /* Equivalent to `attribute_math::DefaultMixer<T> mixer{dst}`. */
    output_set_zero<InterpType>(evaluated_point_index);
    float total_weight = 0.0f;

    const IndexRange point_weights = IndexRange(weights_range_start + i * order, order);
    const int start_index = floatBitsToInt(basis_cache_buf[start_indices_range_start + i]);

    for (int j = 0; j < point_weights.size(); j++) {
      const int point_index = points.start() + (start_index + j) % points.size();
      const float point_weight = basis_cache_buf[point_weights.start() + j];
      const float control_weight = use_point_weight ? control_weights_buf[point_index] : 1.0f;
      const float weight = point_weight * control_weight;
      /* Equivalent to `mixer.mix_in()`. */
      output_weighted_add(evaluated_point_index, weight, input_load<InterpType>(point_index));
      total_weight += weight;
    }
    /* Equivalent to `mixer.finalize()` */
    output_mul<InterpType>(evaluated_point_index, safe_rcp(total_weight));
  }

  if (is_curve_cyclic) {
    /* The closing point is not contained inside the NURBS data structure so we do manual copy. */
    output_write(evaluated_points_padded.last(),
                 output_load<InterpType>(evaluated_points_padded.first()));
  }
}

template void evaluate_curve<InterpPosition>(IndexRange, IndexRange, int);
template void evaluate_curve<float>(IndexRange, IndexRange, int);
template void evaluate_curve<float2>(IndexRange, IndexRange, int);
template void evaluate_curve<float3>(IndexRange, IndexRange, int);
template void evaluate_curve<float4>(IndexRange, IndexRange, int);

}  // namespace nurbs

template<typename InterpType> void evaluate_curve()
{
  const auto &use_cyclic = push_constant_get(draw_curves_data, use_cyclic);
  const auto &curves_count = push_constant_get(draw_curves_data, curves_count);
  const auto &curves_start = push_constant_get(draw_curves_data, curves_start);
  const auto &evaluated_type = push_constant_get(draw_curves_data, evaluated_type);
  const auto &curves_type_buf = buffer_get(draw_curves_data, curves_type_buf);
  const auto &points_by_curve_buf = buffer_get(draw_curves_data, points_by_curve_buf);
  const auto &evaluated_points_by_curve_buf = buffer_get(draw_curves_data,
                                                         evaluated_points_by_curve_buf);

  /* Only for gl_GlobalInvocationID. To be removed. */
  COMPUTE_SHADER_CREATE_INFO(draw_curves_interpolate_position)

  if (gl_GlobalInvocationID.x >= uint(curves_count)) {
    return;
  }
  int curve_index = int(gl_GlobalInvocationID.x) + curves_start;

  const CurveType curve_type = CurveType(gpu_attr_load_uchar(curves_type_buf, curve_index));
  if (curve_type != CurveType(evaluated_type)) {
    return;
  }
  IndexRange points = offset_indices::load_range_from_buffer(points_by_curve_buf, curve_index);
  IndexRange evaluated_points = offset_indices::load_range_from_buffer(
      evaluated_points_by_curve_buf, curve_index);

  if (use_cyclic) {
    evaluated_points = IndexRange(evaluated_points.start() + curve_index,
                                  evaluated_points.size() + 1);
  }

  if (CurveType(evaluated_type) == CURVE_TYPE_CATMULL_ROM) {
    catmull_rom::evaluate_curve<InterpType>(points, evaluated_points, curve_index);
  }
  else if (CurveType(evaluated_type) == CURVE_TYPE_BEZIER) {
    bezier::evaluate_curve<InterpType>(points, evaluated_points, curve_index);
  }
  else if (CurveType(evaluated_type) == CURVE_TYPE_NURBS) {
    nurbs::evaluate_curve<InterpType>(points, evaluated_points, curve_index);
  }
  else if (CurveType(evaluated_type) == CURVE_TYPE_POLY) {
    /* Simple copy. */
    copy_curve_data<InterpType>(points, evaluated_points, curve_index);
  }
}

template void evaluate_curve<InterpPosition>();
template void evaluate_curve<float>();
template void evaluate_curve<float2>();
template void evaluate_curve<float3>();
template void evaluate_curve<float4>();

void evaluate_position_radius()
{
  evaluate_curve<InterpPosition>();
}

void evaluate_attribute_float()
{
  evaluate_curve<float>();
}

void evaluate_attribute_float2()
{
  evaluate_curve<float2>();
}

void evaluate_attribute_float3()
{
  evaluate_curve<float3>();
}

void evaluate_attribute_float4()
{
  evaluate_curve<float4>();
}
