/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* To be compiled with common_subdiv_lib.glsl */

/* Generate triangles from subdivision quads indices. */

layout(std430, binding = 1) readonly restrict buffer extraCoarseFaceData
{
  uint extra_coarse_face_data[];
};

layout(std430, binding = 2) writeonly buffer outputTriangles
{
  uint output_tris[];
};

#ifndef SINGLE_MATERIAL
layout(std430, binding = 3) readonly buffer inputPolygonMatOffset
{
  int face_mat_offset[];
};
#endif

bool is_face_hidden(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & coarse_face_hidden_mask) != 0;
}

void main()
{
  uint quad_index = get_global_invocation_index();
  if (quad_index >= total_dispatch_size) {
    return;
  }

  uint loop_index = quad_index * 4;

  uint coarse_quad_index = coarse_face_index_from_subdiv_quad_index(quad_index, coarse_face_count);

#ifdef SINGLE_MATERIAL
  uint triangle_loop_index = quad_index * 6;
#else
  int mat_offset = face_mat_offset[coarse_quad_index];

  int triangle_loop_index = (int(quad_index) + mat_offset) * 6;
#endif

  if (use_hide && is_face_hidden(coarse_quad_index)) {
    output_tris[triangle_loop_index + 0] = 0xffffffff;
    output_tris[triangle_loop_index + 1] = 0xffffffff;
    output_tris[triangle_loop_index + 2] = 0xffffffff;
    output_tris[triangle_loop_index + 3] = 0xffffffff;
    output_tris[triangle_loop_index + 4] = 0xffffffff;
    output_tris[triangle_loop_index + 5] = 0xffffffff;
  }
  else {
    output_tris[triangle_loop_index + 0] = loop_index + 0;
    output_tris[triangle_loop_index + 1] = loop_index + 1;
    output_tris[triangle_loop_index + 2] = loop_index + 2;
    output_tris[triangle_loop_index + 3] = loop_index + 0;
    output_tris[triangle_loop_index + 4] = loop_index + 2;
    output_tris[triangle_loop_index + 5] = loop_index + 3;
  }
}
