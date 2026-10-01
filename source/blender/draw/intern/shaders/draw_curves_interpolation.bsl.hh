/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * GPU generated interpolated position and radius. Updated on attribute change.
 * One thread processes one curve.
 *
 * Equivalent of `CurvesGeometry::evaluated_positions()`.
 */

#pragma once

#include "draw_attribute_shader_shared.hh"
#include "gpu_shader_attribute_load.bsl.hh"
#include "gpu_shader_math_matrix_transform.bsl.hh"
#include "gpu_shader_math_safe.bsl.hh"
#include "gpu_shader_offset_indices_lib.glsl"

namespace curves {

/* We workaround the lack of function pointers by using different type to overload the attribute
 * implementation. */
struct InterpPosition {
  /* Position, Radius. */
  float4 data;
};

template<typename StoredT, typename T> struct AttribBuf {
  /* Inputs. */
  [[storage(POINT_ATTR_SLOT, read)]] StoredT (&attribute_buf)[];
  /* Outputs. */
  [[storage(EVALUATED_ATTR_SLOT, read_write)]] StoredT (&evaluated_buf)[];

  T input_load(int point_index) const
  {
    StoredT attr = attribute_buf[point_index];
    return load_data(attr);
  }

  T output_load(int evaluated_point_index) const
  {
    StoredT attr = evaluated_buf[evaluated_point_index];
    return load_data(attr);
  }

  void output_write(int evaluated_point_index, T data)
  {
    evaluated_buf[evaluated_point_index] = as_data(data);
  }

  void output_weighted_add(int evaluated_point_index, float w, const T src)
  {
    T dst = output_load(evaluated_point_index);
    dst += src * w;
    output_write(evaluated_point_index, dst);
  }

  void output_mul(int evaluated_point_index, float w)
  {
    T dst = output_load(evaluated_point_index);
    dst *= w;
    output_write(evaluated_point_index, dst);
  }

  void output_set_zero(int evaluated_point_index)
  {
    output_write(evaluated_point_index, T(0.0f));
  }
};

template struct AttribBuf<StoredFloat, float>;
template struct AttribBuf<StoredFloat2, float2>;
template struct AttribBuf<StoredFloat3, float3>;
template struct AttribBuf<StoredFloat4, float4>;

template<> struct AttribBuf<InterpPosition, float4> {
  /* Inputs. */
  [[storage(POINT_POSITIONS_SLOT, read)]] float (&positions_buf)[];
  [[storage(POINT_RADII_SLOT, read)]] float (&radii_buf)[];
  /* Outputs. */
  [[storage(EVALUATED_POS_RAD_SLOT, read_write)]] float4 (&evaluated_positions_radii_buf)[];

  [[push_constant]] float4x4 transform;

  float4 input_load(int point_index) const
  {
    float4 data;
    data.xyz = float3(positions_buf[gpu_attr_load_index(uint(point_index), int2(3, 0)) + 0],
                      positions_buf[gpu_attr_load_index(uint(point_index), int2(3, 0)) + 1],
                      positions_buf[gpu_attr_load_index(uint(point_index), int2(3, 0)) + 2]);
    data.w = radii_buf[point_index];
    /* Bake object transform for legacy hair particle. */
    data.xyz = transform_point(transform, data.xyz);
    return data;
  }

  float4 output_load(int evaluated_point_index) const
  {
    return evaluated_positions_radii_buf[evaluated_point_index];
  }

  void output_write(int evaluated_point_index, float4 data)
  {
    /* Clamp radius to 0 to avoid negative radius due to interpolation. */
    data.w = max(0.0f, data.w);
    evaluated_positions_radii_buf[evaluated_point_index] = data;
  }

  void output_weighted_add(int evaluated_point_index, float w, const float4 src)
  {
    evaluated_positions_radii_buf[evaluated_point_index] += src * w;
  }

  void output_mul(int evaluated_point_index, float w)
  {
    evaluated_positions_radii_buf[evaluated_point_index] *= w;
  }

  void output_set_zero(int evaluated_point_index)
  {
    evaluated_positions_radii_buf[evaluated_point_index] = float4(0.0f);
  }
};

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

struct CurvesData {
  /* Offsets giving the start and end of the curve. */
  [[storage(EVALUATED_POINT_SLOT, read)]] const int (&evaluated_points_by_curve_buf)[];
  [[storage(POINTS_BY_CURVES_SLOT, read)]] const int (&points_by_curve_buf)[];
  [[storage(CURVE_RESOLUTION_SLOT, read)]] const uint (&curves_resolution_buf)[];
  /* Note: Actually int8_t. */
  [[storage(CURVE_TYPE_SLOT, read)]] const uint (&curves_type_buf)[];
  /* Note: Actually bool (1 byte). */
  [[storage(CURVE_CYCLIC_SLOT, read)]] const uint (&curves_cyclic_buf)[];
  /* Bezier handles (if needed). */
  [[storage(HANDLES_POS_LEFT_SLOT, read)]] const float (&handles_positions_left_buf)[];
  [[storage(HANDLES_POS_RIGHT_SLOT, read)]] const float (&handles_positions_right_buf)[];
  [[storage(BEZIER_OFFSETS_SLOT, read)]] const int (&bezier_offsets_buf)[];
  /* Nurbs (alias of other buffers). */
  // [[storage(CURVES_ORDER_SLOT, read)]] uint (&curves_order_buf)[];  /* Actually int8_t. */
  // [[storage(BASIS_CACHE_SLOT, read)]] float (&basis_cache_buf)[];
  // [[storage(CONTROL_WEIGHTS_SLOT, read)]] float (&control_weights_buf)[];
  // [[storage(BASIS_CACHE_OFFSET_SLOT, read)]] int (&basis_cache_offset_buf)[];
  //
  [[push_constant]] int curves_start;
  [[push_constant]] int curves_count;
  [[push_constant]] bool use_point_weight;
  [[push_constant]] bool use_cyclic;

  /**
   * IMPORTANT: For very dumb reasons, on GL the default specialization is compiled and used for
   * creating the shader interface. If this happens to optimize out some push_constants that are
   * valid in other specialization, we will never be able to set them. So choose the specialization
   * that uses all push_constants.
   */
  [[specialization_constant(3 /* CURVE_TYPE_NURBS */)]] int evaluated_type;

  bool curve_cyclic_get(int curve_index) const
  {
    if (use_cyclic) {
      return gpu_attr_load_bool(curves_cyclic_buf, curve_index);
    }
    return false;
  }
};

/** Utilities. */

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

template<typename StorageT, typename AttrT>
void evaluate_curve([[resource_table]] const CurvesData &srt,
                    [[resource_table]] AttribBuf<StorageT, AttrT> &attr,
                    const IndexRange points,
                    const IndexRange evaluated_points,
                    const int curve_index)
{
  const uint curve_resolution = srt.curves_resolution_buf[curve_index];
  const bool is_curve_cyclic = srt.curve_cyclic_get(curve_index);

  const uint evaluated_points_count = uint(evaluated_points.size());
  for (uint i = 0; i < evaluated_points_count; i++) {
    const int evaluated_point_id = evaluated_points.start() + int(i);
    const uint point_id = i / curve_resolution;
    const float parameter = float(i % curve_resolution) / float(curve_resolution);
    const float4 weights = calculate_basis(parameter);
    const int4 point_ids = get_points(point_id, points, is_curve_cyclic);

    attr.output_write(evaluated_point_id,
                      mix4(attr.input_load(point_ids.x),
                           attr.input_load(point_ids.y),
                           attr.input_load(point_ids.z),
                           attr.input_load(point_ids.w),
                           weights));
  }
}

template void evaluate_curve<InterpPosition, float4>(
    const CurvesData &, AttribBuf<InterpPosition, float4> &, IndexRange, IndexRange, int);
template void evaluate_curve<StoredFloat, float>(
    const CurvesData &, AttribBuf<StoredFloat, float> &, IndexRange, IndexRange, int);
template void evaluate_curve<StoredFloat2, float2>(
    const CurvesData &, AttribBuf<StoredFloat2, float2> &, IndexRange, IndexRange, int);
template void evaluate_curve<StoredFloat3, float3>(
    const CurvesData &, AttribBuf<StoredFloat3, float3> &, IndexRange, IndexRange, int);
template void evaluate_curve<StoredFloat4, float4>(
    const CurvesData &, AttribBuf<StoredFloat4, float4> &, IndexRange, IndexRange, int);

}  // namespace catmull_rom

namespace bezier {

template<typename StorageT, typename AttrT>
void evaluate_segment([[resource_table]] const CurvesData & /*srt*/,
                      [[resource_table]] AttribBuf<StorageT, AttrT> &attr,
                      const int2 points,
                      const IndexRange result)
{
  AttrT p0 = attr.input_load(points.x);
  AttrT p1 = attr.input_load(points.y);

  const float step = 1.0f / float(result.size());
  for (int i = 0; i < result.size(); i++) {
    attr.output_write(result.start() + i, mix(p0, p1, float(i) * step));
  }
}

template<>
void evaluate_segment<InterpPosition, float4>(
    [[resource_table]] const CurvesData &srt,
    [[resource_table]] AttribBuf<InterpPosition, float4> &attr,
    const int2 points,
    const IndexRange result)
{
  const auto &handles_right = srt.handles_positions_right_buf;
  const auto &handles_left = srt.handles_positions_left_buf;

  float4 p0 = attr.input_load(points.x);
  float4 p1 = attr.input_load(points.y);

  const float3 point_0 = p0.xyz;
  float3 point_1 = gpu_attr_load_float3(handles_right, int2(3, 0), uint(points.x));
  float3 point_2 = gpu_attr_load_float3(handles_left, int2(3, 0), uint(points.y));
  const float3 point_3 = p1.xyz;

  const float rad_0 = p0.w;
  const float rad_1 = p1.w;

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
    attr.output_write(result.start() + i, float4(q0, rad));
    q0 += q1;
    q1 += q2;
    q2 += q3;
  }
}

template void evaluate_segment<StoredFloat, float>(const CurvesData &,
                                                   AttribBuf<StoredFloat, float> &,
                                                   int2,
                                                   IndexRange);
template void evaluate_segment<StoredFloat2, float2>(const CurvesData &,
                                                     AttribBuf<StoredFloat2, float2> &,
                                                     int2,
                                                     IndexRange);
template void evaluate_segment<StoredFloat3, float3>(const CurvesData &,
                                                     AttribBuf<StoredFloat3, float3> &,
                                                     int2,
                                                     IndexRange);
template void evaluate_segment<StoredFloat4, float4>(const CurvesData &,
                                                     AttribBuf<StoredFloat4, float4> &,
                                                     int2,
                                                     IndexRange);

IndexRange per_curve_point_offsets_range(const IndexRange points, const int curve_index)
{
  return {curve_index + points.start(), points.size() + 1};
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

template<typename StorageT, typename AttrT>
void evaluate_curve([[resource_table]] const CurvesData &srt,
                    [[resource_table]] AttribBuf<StorageT, AttrT> &attr,
                    const IndexRange points,
                    const IndexRange evaluated_points,
                    const int curve_index)
{
  /* Range used for indexing bezier offsets. */
  const IndexRange offsets = per_curve_point_offsets_range(points, curve_index);
  const bool is_curve_cyclic = srt.curve_cyclic_get(curve_index);

  for (int i = 0; i < points.size(); i++) {
    /* Bezier curves can have different number of evaluated segment per curve segment. */
    const IndexRange segment_range = offset_indices::load_range_from_buffer(srt.bezier_offsets_buf,
                                                                            offsets.start() + i);
    const IndexRange evaluated_segment_range = evaluated_points.slice(segment_range);
    const int2 point_ids = get_points(uint(i), points, is_curve_cyclic);

    evaluate_segment<StorageT, AttrT>(srt, attr, point_ids, evaluated_segment_range);
  }

  if (is_curve_cyclic) {
    /* The closing point is not contained inside `bezier_offsets_buf` so we do manual copy. */
    attr.output_write(evaluated_points.last(), attr.output_load(evaluated_points.first()));
  }
}

template void evaluate_curve<InterpPosition, float4>(
    const CurvesData &, AttribBuf<InterpPosition, float4> &, IndexRange, IndexRange, int);
template void evaluate_curve<StoredFloat, float>(
    const CurvesData &, AttribBuf<StoredFloat, float> &, IndexRange, IndexRange, int);
template void evaluate_curve<StoredFloat2, float2>(
    const CurvesData &, AttribBuf<StoredFloat2, float2> &, IndexRange, IndexRange, int);
template void evaluate_curve<StoredFloat3, float3>(
    const CurvesData &, AttribBuf<StoredFloat3, float3> &, IndexRange, IndexRange, int);
template void evaluate_curve<StoredFloat4, float4>(
    const CurvesData &, AttribBuf<StoredFloat4, float4> &, IndexRange, IndexRange, int);

}  // namespace bezier

template<typename StorageT, typename AttrT>
void copy_curve_data([[resource_table]] const CurvesData &srt,
                     [[resource_table]] AttribBuf<StorageT, AttrT> &attr,
                     const IndexRange points,
                     const IndexRange evaluated_points,
                     const int curve_index)
{
  assert(points.size == evaluated_points.size);
  const bool is_curve_cyclic = srt.curve_cyclic_get(curve_index);

  for (int i = 0; i < points.size(); i++) {
    attr.output_write(evaluated_points.start() + i, attr.input_load(points.start() + i));
  }

  if (is_curve_cyclic) {
    /* The closing point is not contained inside `bezier_offsets_buf` so we do manual copy. */
    attr.output_write(evaluated_points.last(), attr.output_load(evaluated_points.first()));
  }
}

template void copy_curve_data<InterpPosition, float4>(
    const CurvesData &, AttribBuf<InterpPosition, float4> &, IndexRange, IndexRange, int);
template void copy_curve_data<StoredFloat, float>(
    const CurvesData &, AttribBuf<StoredFloat, float> &, IndexRange, IndexRange, int);
template void copy_curve_data<StoredFloat2, float2>(
    const CurvesData &, AttribBuf<StoredFloat2, float2> &, IndexRange, IndexRange, int);
template void copy_curve_data<StoredFloat3, float3>(
    const CurvesData &, AttribBuf<StoredFloat3, float3> &, IndexRange, IndexRange, int);
template void copy_curve_data<StoredFloat4, float4>(
    const CurvesData &, AttribBuf<StoredFloat4, float4> &, IndexRange, IndexRange, int);

namespace nurbs {

template<typename StorageT, typename AttrT>
void evaluate_curve([[resource_table]] const CurvesData &srt,
                    [[resource_table]] AttribBuf<StorageT, AttrT> &attr,
                    const IndexRange points,
                    const IndexRange evaluated_points_padded,
                    const int curve_index)
{
  /* Buffer aliasing to same bind point. We cannot dispatch with different type of curve. */
  const auto &curves_order_buf = srt.curves_resolution_buf;
  const auto &basis_cache_offset_buf = srt.bezier_offsets_buf;

  uint order = gpu_attr_load_uchar(curves_order_buf, curve_index);

  const int basis_cache_start = basis_cache_offset_buf[curve_index];
  const bool invalid = basis_cache_start < 0;

  if (invalid) {
    copy_curve_data<StorageT, AttrT>(srt, attr, points, evaluated_points_padded, curve_index);
    return;
  }

  const bool is_curve_cyclic = srt.curve_cyclic_get(curve_index);

  /* Recover original points range without closing cyclic point. */
  const IndexRange evaluated_points{evaluated_points_padded.start(),
                                    evaluated_points_padded.size() - int(srt.use_cyclic)};

  const int start_indices_range_start = basis_cache_start;
  const int weights_range_start = basis_cache_start + evaluated_points.size();

  /* Buffer aliasing to same bind point. We cannot dispatch with different type of curve. */
  const auto &basis_cache_buf = srt.handles_positions_left_buf;
  const auto &control_weights_buf = srt.handles_positions_right_buf;

  for (int i = 0; i < evaluated_points.size(); i++) {
    int evaluated_point_index = evaluated_points.start() + i;
    /* Equivalent to `attribute_math::DefaultMixer<T> mixer{dst}`. */
    attr.output_set_zero(evaluated_point_index);
    float total_weight = 0.0f;

    const IndexRange point_weights{weights_range_start + i * int(order), int(order)};
    const int start_index = floatBitsToInt(basis_cache_buf[start_indices_range_start + i]);

    for (int j = 0; j < point_weights.size(); j++) {
      const int point_index = points.start() + (start_index + j) % points.size();
      const float point_weight = basis_cache_buf[point_weights.start() + j];
      const float control_weight = srt.use_point_weight ? control_weights_buf[point_index] : 1.0f;
      const float weight = point_weight * control_weight;
      /* Equivalent to `mixer.mix_in()`. */
      attr.output_weighted_add(evaluated_point_index, weight, attr.input_load(point_index));
      total_weight += weight;
    }
    /* Equivalent to `mixer.finalize()` */
    attr.output_mul(evaluated_point_index, safe_rcp(total_weight));
  }

  if (is_curve_cyclic) {
    /* The closing point is not contained inside the NURBS data structure so we do manual copy. */
    attr.output_write(evaluated_points_padded.last(),
                      attr.output_load(evaluated_points_padded.first()));
  }
}

template void evaluate_curve<InterpPosition, float4>(
    const CurvesData &, AttribBuf<InterpPosition, float4> &, IndexRange, IndexRange, int);
template void evaluate_curve<StoredFloat, float>(
    const CurvesData &, AttribBuf<StoredFloat, float> &, IndexRange, IndexRange, int);
template void evaluate_curve<StoredFloat2, float2>(
    const CurvesData &, AttribBuf<StoredFloat2, float2> &, IndexRange, IndexRange, int);
template void evaluate_curve<StoredFloat3, float3>(
    const CurvesData &, AttribBuf<StoredFloat3, float3> &, IndexRange, IndexRange, int);
template void evaluate_curve<StoredFloat4, float4>(
    const CurvesData &, AttribBuf<StoredFloat4, float4> &, IndexRange, IndexRange, int);

}  // namespace nurbs

template<typename StorageT, typename AttrT>
[[compute, local_size(CURVES_PER_THREADGROUP)]] void evaluate_curve(
    [[resource_table]] const CurvesData &srt,
    [[resource_table]] AttribBuf<StorageT, AttrT> &attr,
    [[global_invocation_id]] const uint3 global_id)
{
  if (global_id.x >= uint(srt.curves_count)) {
    return;
  }
  int curve_index = int(global_id.x) + srt.curves_start;

  uint curve_type = gpu_attr_load_uchar(srt.curves_type_buf, curve_index);
  if (CurveType(curve_type) != CurveType(srt.evaluated_type)) {
    return;
  }
  IndexRange points = offset_indices::load_range_from_buffer(srt.points_by_curve_buf, curve_index);
  IndexRange evaluated_points = offset_indices::load_range_from_buffer(
      srt.evaluated_points_by_curve_buf, curve_index);

  if (srt.use_cyclic) {
    evaluated_points = IndexRange{evaluated_points.start() + curve_index,
                                  evaluated_points.size() + 1};
  }

  if (CurveType(srt.evaluated_type) == CURVE_TYPE_CATMULL_ROM) {
    catmull_rom::evaluate_curve<StorageT, AttrT>(srt, attr, points, evaluated_points, curve_index);
  }
  else if (CurveType(srt.evaluated_type) == CURVE_TYPE_BEZIER) {
    bezier::evaluate_curve<StorageT, AttrT>(srt, attr, points, evaluated_points, curve_index);
  }
  else if (CurveType(srt.evaluated_type) == CURVE_TYPE_NURBS) {
    nurbs::evaluate_curve<StorageT, AttrT>(srt, attr, points, evaluated_points, curve_index);
  }
  else if (CurveType(srt.evaluated_type) == CURVE_TYPE_POLY) {
    /* Simple copy. */
    copy_curve_data<StorageT, AttrT>(srt, attr, points, evaluated_points, curve_index);
  }
}

template void evaluate_curve<InterpPosition, float4>(const CurvesData &,
                                                     AttribBuf<InterpPosition, float4> &,
                                                     const uint3);
template void evaluate_curve<StoredFloat, float>(const CurvesData &,
                                                 AttribBuf<StoredFloat, float> &,
                                                 const uint3);
template void evaluate_curve<StoredFloat2, float2>(const CurvesData &,
                                                   AttribBuf<StoredFloat2, float2> &,
                                                   const uint3);
template void evaluate_curve<StoredFloat3, float3>(const CurvesData &,
                                                   AttribBuf<StoredFloat3, float3> &,
                                                   const uint3);
template void evaluate_curve<StoredFloat4, float4>(const CurvesData &,
                                                   AttribBuf<StoredFloat4, float4> &,
                                                   const uint3);

}  // namespace curves

PipelineCompute draw_curves_interpolate_position(
    curves::evaluate_curve<curves::InterpPosition, float4>);
PipelineCompute draw_curves_interpolate_float4_attribute(
    curves::evaluate_curve<StoredFloat4, float4>);
PipelineCompute draw_curves_interpolate_float3_attribute(
    curves::evaluate_curve<StoredFloat3, float3>);
PipelineCompute draw_curves_interpolate_float2_attribute(
    curves::evaluate_curve<StoredFloat2, float2>);
PipelineCompute draw_curves_interpolate_float_attribute(
    curves::evaluate_curve<StoredFloat, float>);
