/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_constants_lib.glsl"

/* Wire Color Types, matching eV3DShadingColorType. */
#define V3D_SHADING_SINGLE_COLOR 2
#define V3D_SHADING_OBJECT_COLOR 4
#define V3D_SHADING_RANDOM_COLOR 1

float4x4 extract_matrix_packed_data(float4x4 mat, float4 &dataA, float4 &dataB)
{
  constexpr float div = 1.0f / 255.0f;
  int a = int(mat[0][3]);
  int b = int(mat[1][3]);
  int c = int(mat[2][3]);
  int d = int(mat[3][3]);
  dataA = float4(a & 0xFF, a >> 8, b & 0xFF, b >> 8) * div;
  dataB = float4(c & 0xFF, c >> 8, d & 0xFF, d >> 8) * div;
  mat[0][3] = mat[1][3] = mat[2][3] = 0.0f;
  mat[3][3] = 1.0f;
  return mat;
}

/**
 * Pack overlay line data to float4.
 * Note: edge_start, edge_pos need to be in the range [0..uniform_buf.size_viewport].
 * Note: returns float4 for FBO output; only the first two components store data.
 */
float4 pack_line_data(float2 frag_co, float2 edge_start, float2 edge_pos)
{
  float2 edge = edge_start - edge_pos;
  float len = length(edge);
  if (len > 0.0f) {
    edge /= len;

    /* Get perpendicular in direction of upper hemicircle. */
    float2 perp = float2(-edge.y, edge.x);
    if (perp.y < 0.0) {
      perp = -perp;
    }

    /* Get distance along perpendicular by projection of edge.  */
    float sin_theta = perp.x;
    float dist = dot(perp, frag_co - edge_start);

    /* Pack dist to [0.1, ..., 0.9], leaving 0.1 boundary for clear or blocked pixels. */
    return float4(sin_theta * 0.5f + 0.5f, dist * 0.4f + 0.5f, 0.0f, 1.0f);
  }
  else {
    /* Default line if the origin is perfectly aligned with a pixel. */
    return float4(0.0f, 0.5f, 0.0f, 1.0f);
  }
}

/**
 * Pack overlay line data to float4, carrying a blocker value to indicate
 * a stage does its own AA in this pixel.
 */
float4 pack_line_data_no_aa()
{
  return float4(0.0f, 1.0f, 0.0f, 1.0f);
}

/**
 * Unpack overlay line data, recovering perpendicular vector direction and signed distance.
 */
void unpack_line_data(float2 data, float2 &perp, float &dist)
{
  /* Unpack distance to edge, remove 0.1 boundary around value. */
  dist = (data.y - 0.5f) * 2.5f;

  /* Recover perpendicular vector from packed sin_theta. */
  float sin_theta = (data.x - 0.5f) * 2.0f;
  float cos_theta = cos_from_sin(sin_theta);
  perp = normalize(float2(sin_theta, cos_theta));
}

/* View-space Z is used to adjust for perspective projection.
 * Homogenous W is used to convert from NDC to homogenous space.
 * Offset is in view-space, so positive values are closer to the camera. */
float get_homogenous_z_offset(float4x4 winmat, float vs_z, float hs_w, float vs_offset)
{
  if (vs_offset == 0.0f) {
    /* Don't calculate homogenous offset if view-space offset is zero. */
    return 0.0f;
  }
  else if (winmat[3][3] == 0.0f) {
    /* Clamp offset to half of Z to avoid floating point precision errors. */
    vs_offset = min(vs_offset, vs_z * -0.5f);
    /* From "Projection Matrix Tricks" by Eric Lengyel:
     * http://www.terathon.com/gdc07_lengyel.pdf (p. 24 Depth Modification) */
    return winmat[3][2] * (vs_offset / (vs_z * (vs_z + vs_offset))) * hs_w;
  }
  else {
    return winmat[2][2] * vs_offset * hs_w;
  }
}

float mul_project_m4_v3_zfac(float pixel_fac, float3 co)
{
  float3 vP = drw_point_world_to_view(co).xyz;
  float4x4 winmat = drw_view().winmat;
  return pixel_fac *
         (winmat[0][3] * vP.x + winmat[1][3] * vP.y + winmat[2][3] * vP.z + winmat[3][3]);
}
