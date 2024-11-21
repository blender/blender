/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "common_view_lib.glsl"

/* project to screen space */
vec2 proj(vec4 pos)
{
  return (0.5 * (pos.xy / pos.w) + 0.5) * sizeViewport.xy;
}

struct VertIn {
  vec3 l_P;
  mat4 inst_matrix;
};

struct VertOut {
  vec3 vPos;
  vec4 pPos;
  vec2 ssPos;
  vec4 vColSize;
  int inverted;
};

VertOut vertex_main(VertIn v_in)
{
  vec4 bone_color, state_color;
  mat4 model_mat = extract_matrix_packed_data(v_in.inst_matrix, state_color, bone_color);

  vec4 world_pos = model_mat * vec4(v_in.l_P, 1.0);
  vec4 view_pos = drw_view.viewmat * world_pos;

  /* This is slow and run per vertex, but it's still faster than
   * doing it per instance on CPU and sending it on via instance attribute. */
  mat3 normal_mat = transpose(inverse(to_float3x3(model_mat)));

  VertOut v_out;
  v_out.vPos = view_pos.xyz;
  v_out.pPos = drw_view.winmat * view_pos;
  v_out.inverted = int(dot(cross(model_mat[0].xyz, model_mat[1].xyz), model_mat[2].xyz) < 0.0);
  v_out.ssPos = proj(v_out.pPos);
  v_out.vColSize = bone_color;

  view_clipping_distances(world_pos.xyz);

  return v_out;
}

#ifndef NO_GEOM
/* Legacy Path */
void main()
{
  VertIn v_in;
  v_in.l_P = pos;
  v_in.inst_matrix = inst_obmat;

  VertOut v_out = vertex_main(v_in);

  geom_in.vPos = v_out.vPos;
  geom_in.pPos = v_out.pPos;
  geom_in.ssPos = v_out.ssPos;
  geom_in.vColSize = v_out.vColSize;
  geom_flat_in.inverted = v_out.inverted;
}

#else

void main()
{
  /* TODO */
}

#endif
