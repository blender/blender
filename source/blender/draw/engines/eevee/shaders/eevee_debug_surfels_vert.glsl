/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_lightprobe_volume_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_debug_surfels)

#include "draw_view_lib.glsl"
#include "gpu_shader_math_matrix_construct_lib.glsl"

void main()
{
  surfel_index = gl_InstanceID;
  Surfel surfel = surfels_buf[surfel_index];

#if 0 /* Debug surfel lists. TODO allow in release build with a dedicated shader. */
  if (gl_VertexID == 0 && surfel.next > -1) {
    Surfel surfel_next = surfels_buf[surfel.next];
    float4 line_color = (surfel.prev == -1)      ? float4(1.0f, 1.0f, 0.0f, 1.0f) :
                      (surfel_next.next == -1) ? float4(0.0f, 1.0f, 1.0f, 1.0f) :
                                                 float4(0.0f, 1.0f, 0.0f, 1.0f);
    /* WORKAROUND: Avoid compilation error because this gets parsed before dead code removal. */
    drw_ debug_line(surfel_next.position, surfel.position, line_color);
  }
#endif

  float3 lP;

  switch (gl_VertexID) {
    case 0:
      lP = float3(-1, 1, 0);
      break;
    case 1:
      lP = float3(-1, -1, 0);
      break;
    case 2:
      lP = float3(1, 1, 0);
      break;
    case 3:
      lP = float3(1, -1, 0);
      break;
  }

  float3x3 TBN = from_up_axis(surfel.normal);

  float4x4 model_matrix = float4x4(float4(TBN[0] * debug_surfel_radius, 0),
                                   float4(TBN[1] * debug_surfel_radius, 0),
                                   float4(TBN[2] * debug_surfel_radius, 0),
                                   float4(surfel.position, 1));

  P = (model_matrix * float4(lP, 1)).xyz;

  gl_Position = drw_point_world_to_homogenous(P);
  gl_Position.z -= 2.5e-5f;
}
