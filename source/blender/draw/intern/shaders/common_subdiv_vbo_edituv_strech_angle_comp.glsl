/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* To be compiled with common_subdiv_lib.glsl */

layout(std430, binding = 0) readonly buffer inputVerts
{
  PosNorLoop pos_nor[];
};

layout(std430, binding = 1) readonly buffer inputUVs
{
  vec2 uvs[];
};

/* Mirror of #UVStretchAngle in the C++ code, but using floats until proper data compression
 * is implemented for all subdivision data. */
struct UVStretchAngle {
  float angle;
  float uv_angle0;
  float uv_angle1;
};

layout(std430, binding = 2) writeonly buffer outputStretchAngles
{
  UVStretchAngle uv_stretches[];
};

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
  if (quad_index >= total_dispatch_size) {
    return;
  }

  uint start_loop_index = quad_index * 4;

  for (uint i = 0; i < 4; i++) {
    uint cur_loop_index = start_loop_index + i;
    uint next_loop_index = start_loop_index + (i + 1) % 4;
    uint prev_loop_index = start_loop_index + (i + 3) % 4;

    /* Compute 2d edge vectors from UVs. */
    vec2 cur_uv = uvs[src_offset + cur_loop_index];
    vec2 next_uv = uvs[src_offset + next_loop_index];
    vec2 prev_uv = uvs[src_offset + prev_loop_index];

    vec2 norm_uv_edge0 = normalize(prev_uv - cur_uv);
    vec2 norm_uv_edge1 = normalize(cur_uv - next_uv);

    /* Compute 3d edge vectors from positions. */
    vec3 cur_pos = get_vertex_pos(pos_nor[cur_loop_index]);
    vec3 next_pos = get_vertex_pos(pos_nor[next_loop_index]);
    vec3 prev_pos = get_vertex_pos(pos_nor[prev_loop_index]);

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
