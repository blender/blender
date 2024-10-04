/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_material_transform_utils.glsl"

void node_tex_coord_position(out vec3 out_pos)
{
  out_pos = g_data.P;
}

void node_tex_coord(mat4 obmatinv,
                    vec3 attr_orco,
                    vec4 attr_uv,
                    out vec3 generated,
                    out vec3 normal,
                    out vec3 uv,
                    out vec3 object,
                    out vec3 camera,
                    out vec3 window,
                    out vec3 reflection)
{
  generated = attr_orco;
  normal_transform_world_to_object(g_data.N, normal);
  uv = attr_uv.xyz;
  bool valid_mat = (obmatinv[3][3] != 0.0);
  if (valid_mat) {
    object = (obmatinv * vec4(g_data.P, 1.0)).xyz;
  }
  else {
    point_transform_world_to_object(g_data.P, object);
  }
  camera = coordinate_camera(g_data.P);
  window = coordinate_screen(g_data.P);
  reflection = coordinate_reflect(g_data.P, g_data.N);
}
