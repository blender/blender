/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "subdiv_lib.glsl"

#define M_PI 3.1415926535897932
#define M_1_PI 0.31830988618379067154

/* Adapted from BLI_math_vector.h */
float angle_normalized_v3v3(vec3 v1, vec3 v2)
{
  /* this is the same as acos(dot_v3v3(v1, v2)), but more accurate */
  bool q = (dot(v1, v2) >= 0.0);
  vec3 v = (q) ? (v1 - v2) : (v1 + v2);
  float a = 2.0 * asin(length(v) / 2.0);
  return (q) ? a : M_PI - a;
}

void main()
{
  /* We execute for each quad. */
  uint quad_index = get_global_invocation_index();
  if (quad_index >= shader_data.total_dispatch_size) {
    return;
  }

  uint start_loop_index = quad_index * 4;

  for (uint i = 0; i < 4; i++) {
    uint cur_loop_index = start_loop_index + i;
    uint next_loop_index = start_loop_index + (i + 1) % 4;
    uint prev_loop_index = start_loop_index + (i + 3) % 4;

    /* Compute 2d edge vectors from UVs. */
    vec2 cur_uv = uvs[shader_data.src_offset + cur_loop_index];
    vec2 next_uv = uvs[shader_data.src_offset + next_loop_index];
    vec2 prev_uv = uvs[shader_data.src_offset + prev_loop_index];

    vec2 norm_uv_edge0 = normalize(prev_uv - cur_uv);
    vec2 norm_uv_edge1 = normalize(cur_uv - next_uv);

    /* Compute 3d edge vectors from positions. */
    PosNorLoop cur_pos_nor = pos_nor[cur_loop_index];
    vec3 cur_pos = subdiv_get_vertex_pos(cur_pos_nor);
    PosNorLoop next_pos_nor = pos_nor[next_loop_index];
    vec3 next_pos = subdiv_get_vertex_pos(next_pos_nor);
    PosNorLoop prev_pos_nor = pos_nor[prev_loop_index];
    vec3 prev_pos = subdiv_get_vertex_pos(prev_pos_nor);

    vec3 norm_pos_edge0 = normalize(prev_pos - cur_pos);
    vec3 norm_pos_edge1 = normalize(cur_pos - next_pos);

    /* Compute stretches, this logic is adapted from #edituv_get_edituv_stretch_angle.
     * Keep in sync! */
    UVStretchAngle stretch;
    stretch.uv_angle0 = atan(norm_uv_edge0.y, norm_uv_edge0.x) * M_1_PI;
    stretch.uv_angle1 = atan(norm_uv_edge1.y, norm_uv_edge1.x) * M_1_PI;
    stretch.angle = angle_normalized_v3v3(norm_pos_edge0, norm_pos_edge1) * M_1_PI;

    uv_stretches[cur_loop_index] = stretch;
  }
}
