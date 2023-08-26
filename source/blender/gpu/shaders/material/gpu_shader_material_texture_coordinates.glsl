/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  normal = normal_world_to_object(g_data.N);
  uv = attr_uv.xyz;
  object = transform_point((obmatinv[3][3] == 0.0) ? ModelMatrixInverse : obmatinv, g_data.P);
  camera = coordinate_camera(g_data.P);
  window = coordinate_screen(g_data.P);
  reflection = coordinate_reflect(g_data.P, g_data.N);
}
