/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view_lib.glsl"

#define OVERLAY_UV_LINE_STYLE_OUTLINE 0
#define OVERLAY_UV_LINE_STYLE_DASH 1
#define OVERLAY_UV_LINE_STYLE_BLACK 2
#define OVERLAY_UV_LINE_STYLE_WHITE 3
#define OVERLAY_UV_LINE_STYLE_SHADOW 4

/* Wire Color Types, matching eV3DShadingColorType. */
#define V3D_SHADING_SINGLE_COLOR 2
#define V3D_SHADING_OBJECT_COLOR 4
#define V3D_SHADING_RANDOM_COLOR 1

#define DRW_BASE_SELECTED (1 << 1)
#define DRW_BASE_FROM_DUPLI (1 << 2)
#define DRW_BASE_FROM_SET (1 << 3)
#define DRW_BASE_ACTIVE (1 << 4)

mat4 extract_matrix_packed_data(mat4 mat, out vec4 dataA, out vec4 dataB)
{
  const float div = 1.0 / 255.0;
  int a = int(mat[0][3]);
  int b = int(mat[1][3]);
  int c = int(mat[2][3]);
  int d = int(mat[3][3]);
  dataA = vec4(a & 0xFF, a >> 8, b & 0xFF, b >> 8) * div;
  dataB = vec4(c & 0xFF, c >> 8, d & 0xFF, d >> 8) * div;
  mat[0][3] = mat[1][3] = mat[2][3] = 0.0;
  mat[3][3] = 1.0;
  return mat;
}

/* edge_start and edge_pos needs to be in the range [0..sizeViewport]. */
vec4 pack_line_data(vec2 frag_co, vec2 edge_start, vec2 edge_pos)
{
  vec2 edge = edge_start - edge_pos;
  float len = length(edge);
  if (len > 0.0) {
    edge /= len;
    vec2 perp = vec2(-edge.y, edge.x);
    float dist = dot(perp, frag_co - edge_start);
    /* Add 0.1 to differentiate with cleared pixels. */
    return vec4(perp * 0.5 + 0.5, dist * 0.25 + 0.5 + 0.1, 1.0);
  }
  else {
    /* Default line if the origin is perfectly aligned with a pixel. */
    return vec4(1.0, 0.0, 0.5 + 0.1, 1.0);
  }
}

/* View-space Z is used to adjust for perspective projection.
 * Homogenous W is used to convert from NDC to homogenous space.
 * Offset is in view-space, so positive values are closer to the camera. */
float get_homogenous_z_offset(mat4x4 winmat, float vs_z, float hs_w, float vs_offset)
{
  if (vs_offset == 0.0) {
    /* Don't calculate homogenous offset if view-space offset is zero. */
    return 0.0;
  }
  else if (winmat[3][3] == 0.0) {
    /* Clamp offset to half of Z to avoid floating point precision errors. */
    vs_offset = min(vs_offset, vs_z * -0.5);
    /* From "Projection Matrix Tricks" by Eric Lengyel:
     * http://www.terathon.com/gdc07_lengyel.pdf (p. 24 Depth Modification) */
    return winmat[3][2] * (vs_offset / (vs_z * (vs_z + vs_offset))) * hs_w;
  }
  else {
    return winmat[2][2] * vs_offset * hs_w;
  }
}

float mul_project_m4_v3_zfac(float pixel_fac, vec3 co)
{
  vec3 vP = drw_point_world_to_view(co).xyz;
  float4x4 winmat = drw_view.winmat;
  return pixel_fac *
         (winmat[0][3] * vP.x + winmat[1][3] * vP.y + winmat[2][3] * vP.z + winmat[3][3]);
}
