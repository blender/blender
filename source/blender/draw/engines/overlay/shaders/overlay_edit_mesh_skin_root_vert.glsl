/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_mesh_skin_root)
#ifdef GLSL_CPP_STUBS
#  define VERTEX_PULL
#endif

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_math_constants_lib.glsl"

void main()
{
  float3x3 imat = to_float3x3(drw_modelinv());
  float3 right = normalize(imat * drw_view().viewinv[0].xyz);
  float3 up = normalize(imat * drw_view().viewinv[1].xyz);
#ifdef VERTEX_PULL
  int instance_id = gl_VertexID / 64;
  int vert_id = gl_VertexID % 64;
  /* TODO(fclem): Use correct vertex format. For now we read the format manually. */
  float circle_size = size[instance_id * 4];
  float3 lP = float3(
      size[instance_id * 4 + 1], size[instance_id * 4 + 2], size[instance_id * 4 + 3]);

  float theta = M_TAU * (float(vert_id) / 63.0f);
  float3 circle_P = float3(cos(theta), 0.0f, sin(theta));
  final_color = theme.colors.skinroot;
#else
  float3 lP = local_pos;
  float circle_size = size;
  float3 circle_P = pos;
  /* Manual stipple: one segment out of 2 is transparent. */
  final_color = ((gl_VertexID & 1) == 0) ? theme.colors.skinroot : float4(0.0f);
#endif
  float3 screen_pos = (right * circle_P.x + up * circle_P.z) * circle_size;
  float4 pos_4d = drw_modelmat() * float4(lP + screen_pos, 1.0f);
  gl_Position = drw_view().winmat * (drw_view().viewmat * pos_4d);

  view_clipping_distances(pos_4d.xyz);
}
