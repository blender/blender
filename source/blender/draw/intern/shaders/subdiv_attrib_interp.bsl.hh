/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "subdiv_common.bsl.hh"

namespace subdiv {

enum ComponentType : int {
  U16 = 0,
  I32 = 1,
  F32 = 2,
};

struct AttribInterp {
  [[storage(CUSTOM_DATA_FACE_PTEX_OFFSET_BUF_SLOT, read)]] uint (&face_ptex_offset)[];
  [[storage(CUSTOM_DATA_PATCH_COORDS_BUF_SLOT, read)]] BlenderPatchCoord (&patch_coords)[];
  [[storage(CUSTOM_DATA_EXTRA_COARSE_FACE_DATA_BUF_SLOT, read)]] uint (&extra_coarse_face_data)[];

  [[storage(CUSTOM_DATA_SOURCE_DATA_BUF_SLOT, read)]] uint (&src_data)[];
  [[storage(CUSTOM_DATA_DESTINATION_DATA_BUF_SLOT, write)]] uint (&dst_data)[];

  uint get_vertex_count(uint coarse_face) const
  {
    uint number_of_patches = face_ptex_offset[coarse_face + 1] - face_ptex_offset[coarse_face];
    if (number_of_patches == 1) {
      /* If there is only one patch for the current coarse polygon, then it is a quad. */
      return 4;
    }
    /* Otherwise, the number of patches is the number of vertices. */
    return number_of_patches;
  }

  uint get_polygon_corner_index(uint coarse_face, uint patch_index) const
  {
    uint patch_offset = face_ptex_offset[coarse_face];
    return patch_index - patch_offset;
  }

  uint get_loop_start([[resource_table]] const SubdivResources &srt, uint coarse_face) const
  {
    return extra_coarse_face_data[coarse_face] & srt.shader_data.coarse_face_loopstart_mask;
  }
};

template<typename T, int Dim, enum ComponentType Comp> T read(const AttribInterp &res, uint index)
{
  T value;
  if constexpr (Comp == U16) {
    const uint base_index = index * 2;
    const uint xy = res.src_data[base_index];
    const uint zw = res.src_data[base_index + 1];

    value[0] = float((xy >> 16) & 0xFFFFu) / 65535.0f;
    value[1] = float(xy & 0xFFFFu) / 65535.0f;
    value[2] = float((zw >> 16) & 0xFFFFu) / 65535.0f;
    value[3] = float(zw & 0xFFFFu) / 65535.0f;
#ifdef GLSL_CPP_STUBS
    /* Other cases unsupported for now. */
    static_assert(Dim == 4);
#endif
  }
  else if constexpr (Comp == I32) {
    const uint base_index = index * Dim;
    if constexpr (Dim == 1) {
      value = float(int(res.src_data[base_index]));
    }
    else {
      for (int i = 0; i < Dim; i++) [[unroll]] {
        value[i] = float(int(res.src_data[base_index + i]));
      }
    }
  }
  else if constexpr (Comp == F32) {
    const uint base_index = index * Dim;
    if constexpr (Dim == 1) {
      value = uintBitsToFloat(res.src_data[base_index]);
    }
    else {
      for (int i = 0; i < Dim; i++) [[unroll]] {
        value[i] = uintBitsToFloat(res.src_data[base_index + uint(i)]);
      }
    }
  }
  return value;
}

template float4 read<float4, 4, U16>(const AttribInterp &, uint);
template float read<float, 1, I32>(const AttribInterp &, uint);
template float2 read<float2, 2, I32>(const AttribInterp &, uint);
template float3 read<float3, 3, I32>(const AttribInterp &, uint);
template float4 read<float4, 4, I32>(const AttribInterp &, uint);
template float read<float, 1, F32>(const AttribInterp &, uint);
template float2 read<float2, 2, F32>(const AttribInterp &, uint);
template float3 read<float3, 3, F32>(const AttribInterp &, uint);
template float4 read<float4, 4, F32>(const AttribInterp &, uint);

template<typename T, int Dim, enum ComponentType Comp>
void write(AttribInterp &res, uint dst_offset, uint index, const T value)
{
  if constexpr (Comp == U16) {
    const uint base_index = dst_offset + index * 2;
    uint x = uint(value[0] * 65535.0f);
    uint y = uint(value[1] * 65535.0f);
    uint z = uint(value[2] * 65535.0f);
    uint w = uint(value[3] * 65535.0f);

    res.dst_data[base_index + 0] = x << 16 | y;
    res.dst_data[base_index + 1] = z << 16 | w;
#ifdef GLSL_CPP_STUBS
    /* Other cases unsupported for now. */
    static_assert(Dim == 4);
#endif
  }
  else if constexpr (Comp == I32) {
    uint base_index = dst_offset + index * Dim;
    if constexpr (Dim == 1) {
      res.dst_data[base_index] = uint(int(round(value)));
    }
    else {
      for (int i = 0; i < Dim; i++) {
        res.dst_data[base_index + i] = uint(int(round(value[i])));
      }
    }
  }
  else if constexpr (Comp == F32) {
    uint base_index = dst_offset + index * Dim;
    if constexpr (Dim == 1) {
      res.dst_data[base_index] = floatBitsToUint(value);
    }
    else {
      for (uint i = 0; i < Dim; i++) {
        res.dst_data[base_index + i] = floatBitsToUint(value[i]);
      }
    }
  }
}

template void write<float4, 4, U16>(AttribInterp &, uint, uint, float4);
template void write<float, 1, I32>(AttribInterp &, uint, uint, float);
template void write<float2, 2, I32>(AttribInterp &, uint, uint, float2);
template void write<float3, 3, I32>(AttribInterp &, uint, uint, float3);
template void write<float4, 4, I32>(AttribInterp &, uint, uint, float4);
template void write<float, 1, F32>(AttribInterp &, uint, uint, float);
template void write<float2, 2, F32>(AttribInterp &, uint, uint, float2);
template void write<float3, 3, F32>(AttribInterp &, uint, uint, float3);
template void write<float4, 4, F32>(AttribInterp &, uint, uint, float4);

template<typename T, int Dim, enum ComponentType Comp, bool Normalize>
[[compute, local_size(SUBDIV_GROUP_SIZE)]] void attribute_interp(
    [[resource_table]] const SubdivResources &srt,
    [[resource_table]] const PolygonOffsetBase &poly_ofs,
    [[resource_table]] AttribInterp &res,
    [[global_invocation_id]] const uint3 global_id,
    [[num_work_groups]] const uint3 num_work_groups)
{
  /* We execute for each quad. */
  const uint quad_index = srt.get_global_invocation_index(global_id, num_work_groups);
  if (quad_index >= srt.shader_data.total_dispatch_size) {
    return;
  }

  const uint start_loop_index = quad_index * 4;

  /* Find which coarse polygon we came from. */
  const uint coarse_face = poly_ofs.coarse_face_index_from_subdiv_quad_index(
      quad_index, uint(srt.shader_data.coarse_face_count));
  const uint loop_start = res.get_loop_start(srt, coarse_face);

  /* Find the number of vertices for the coarse polygon. */
  T v0 = T(0.0f), v1 = T(0.0f), v2 = T(0.0f), v3 = T(0.0f);

  const uint number_of_vertices = res.get_vertex_count(coarse_face);
  if (number_of_vertices == 4) {
    /* Interpolate the src data. */
    v0 = read<T, Dim, Comp>(res, loop_start + 0);
    v1 = read<T, Dim, Comp>(res, loop_start + 1);
    v2 = read<T, Dim, Comp>(res, loop_start + 2);
    v3 = read<T, Dim, Comp>(res, loop_start + 3);
  }
  else {
    /* Interpolate the src data for the center. */
    uint loop_end = loop_start + number_of_vertices;
    T center_value = T(0);

    float weight = 1.0f / float(number_of_vertices);

    for (uint l = loop_start; l < loop_end; l++) {
      center_value += read<T, Dim, Comp>(res, l) * weight;
    }

    /* Interpolate between the previous and next corner for the middle values for the edges. */
    uint patch_index = uint(res.patch_coords[start_loop_index].patch_index);
    uint current_coarse_corner = res.get_polygon_corner_index(coarse_face, patch_index);
    uint next_coarse_corner = (current_coarse_corner + 1) % number_of_vertices;
    uint prev_coarse_corner = (current_coarse_corner + number_of_vertices - 1) %
                              number_of_vertices;

    v0 = read<T, Dim, Comp>(res, loop_start + current_coarse_corner);
    v1 = read<T, Dim, Comp>(res, loop_start + next_coarse_corner);
    v3 = read<T, Dim, Comp>(res, loop_start + prev_coarse_corner);
    /* Midpoint. */
    v1 = (v0 + v1) * 0.5f;
    v3 = (v0 + v3) * 0.5f;
    /* Interpolate between the current value, and the ones for the center and mid-edges. */
    v2 = center_value;
  }

  /* Do a linear interpolation of the data based on the UVs for each loop of this subdivided quad.
   */
  for (uint loop_index = start_loop_index; loop_index < start_loop_index + 4; loop_index++) {
    BlenderPatchCoord co = res.patch_coords[loop_index];
    float2 uv = decode_uv(co.encoded_uv);
    /* NOTE: v2 and v3 are reversed to stay consistent with the interpolation weight on the x-axis:
     *
     * v3 +-----+ v2
     *    |     |
     *    |     |
     * v0 +-----+ v1
     *
     * otherwise, weight would be `1.0f - uv.x` for `v2 <-> v3`, but `uv.x` for `v0 <-> v1`.
     */
    T e = mix(v0, v1, uv.x);
    T f = mix(v3, v2, uv.x);
    T result = mix(e, f, uv.y);

    if constexpr (Normalize) {
      result = normalize(result);
    }

    write<T, Dim, Comp>(res, srt.shader_data.dst_offset, loop_index, result);
  }
}

/* clang-format off */
template void attribute_interp<float4, 4, U16, false>(const SubdivResources &, const PolygonOffsetBase &, AttribInterp &, const uint3, const uint3);
template void attribute_interp<float,  1, I32, false>(const SubdivResources &, const PolygonOffsetBase &, AttribInterp &, const uint3, const uint3);
template void attribute_interp<float2, 2, I32, false>(const SubdivResources &, const PolygonOffsetBase &, AttribInterp &, const uint3, const uint3);
template void attribute_interp<float3, 3, I32, false>(const SubdivResources &, const PolygonOffsetBase &, AttribInterp &, const uint3, const uint3);
template void attribute_interp<float4, 4, I32, false>(const SubdivResources &, const PolygonOffsetBase &, AttribInterp &, const uint3, const uint3);
template void attribute_interp<float,  1, F32, false>(const SubdivResources &, const PolygonOffsetBase &, AttribInterp &, const uint3, const uint3);
template void attribute_interp<float2, 2, F32, false>(const SubdivResources &, const PolygonOffsetBase &, AttribInterp &, const uint3, const uint3);
template void attribute_interp<float3, 3, F32, false>(const SubdivResources &, const PolygonOffsetBase &, AttribInterp &, const uint3, const uint3);
template void attribute_interp<float4, 4, F32, false>(const SubdivResources &, const PolygonOffsetBase &, AttribInterp &, const uint3, const uint3);
template void attribute_interp<float3, 3, F32, true>(const SubdivResources &, const PolygonOffsetBase &, AttribInterp &, const uint3, const uint3);
/* clang-format on */

PipelineCompute custom_data_interp_4d_u16(attribute_interp<float4, 4, U16, false>);
PipelineCompute custom_data_interp_1d_i32(attribute_interp<float, 1, I32, false>);
PipelineCompute custom_data_interp_2d_i32(attribute_interp<float2, 2, I32, false>);
PipelineCompute custom_data_interp_3d_i32(attribute_interp<float3, 3, I32, false>);
PipelineCompute custom_data_interp_4d_i32(attribute_interp<float4, 4, I32, false>);
PipelineCompute custom_data_interp_1d_f32(attribute_interp<float, 1, F32, false>);
PipelineCompute custom_data_interp_2d_f32(attribute_interp<float2, 2, F32, false>);
PipelineCompute custom_data_interp_3d_f32(attribute_interp<float3, 3, F32, false>);
PipelineCompute custom_data_interp_4d_f32(attribute_interp<float4, 4, F32, false>);
PipelineCompute custom_data_interp_3d_f32_normalize(attribute_interp<float3, 3, F32, true>);

}  // namespace subdiv
