/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_lightprobe_volume_info.hh"

VERTEX_SHADER_CREATE_INFO(eevee_debug_surfels)

#include "draw_view_lib.glsl"
#include "gpu_shader_math_matrix_lib.glsl"

void main()
{
  surfel_index = gl_InstanceID;
  Surfel surfel = surfels_buf[surfel_index];

#if 0 /* Debug surfel lists. TODO allow in release build with a dedicated shader. */
  if (gl_VertexID == 0 && surfel.next > -1) {
    Surfel surfel_next = surfels_buf[surfel.next];
    vec4 line_color = (surfel.prev == -1)      ? vec4(1.0, 1.0, 0.0, 1.0) :
                      (surfel_next.next == -1) ? vec4(0.0, 1.0, 1.0, 1.0) :
                                                 vec4(0.0, 1.0, 0.0, 1.0);
    drw_debug_line(surfel_next.position, surfel.position, line_color);
  }
#endif

  vec3 lP;

  switch (gl_VertexID) {
    case 0:
      lP = vec3(-1, 1, 0);
      break;
    case 1:
      lP = vec3(-1, -1, 0);
      break;
    case 2:
      lP = vec3(1, 1, 0);
      break;
    case 3:
      lP = vec3(1, -1, 0);
      break;
  }

  mat3x3 TBN = from_up_axis(surfel.normal);

  mat4 model_matrix = mat4(vec4(TBN[0] * debug_surfel_radius, 0),
                           vec4(TBN[1] * debug_surfel_radius, 0),
                           vec4(TBN[2] * debug_surfel_radius, 0),
                           vec4(surfel.position, 1));

  P = (model_matrix * vec4(lP, 1)).xyz;

  gl_Position = drw_point_world_to_homogenous(P);
  gl_Position.z -= 2.5e-5;
}
