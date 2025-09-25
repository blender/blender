/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_extra_grid_base)
VERTEX_SHADER_CREATE_INFO(draw_modelmat)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "select_lib.glsl"

float4 color_from_id(float color_id)
{
  if (is_transform) {
    return theme.colors.transform;
  }
  else if (color_id == 1.0f) {
    return theme.colors.active_object;
  }
  else /* 2.0f */ {
    return theme.colors.object_select;
  }

  return theme.colors.transform;
}

void main()
{
  select_id_set(drw_custom_id());
  float4x4 model_mat = grid_model_matrix;
  model_mat[0][3] = model_mat[1][3] = model_mat[2][3] = 0.0f;
  model_mat[3][3] = 1.0f;
  float color_id = grid_model_matrix[3].w;

  int3 grid_resolution = int3(
      grid_model_matrix[0].w, grid_model_matrix[1].w, grid_model_matrix[2].w);

  float3 ls_cell_location;
  /* Keep in sync with update_irradiance_probe */
  ls_cell_location.z = float(gl_VertexID % grid_resolution.z);
  ls_cell_location.y = float((gl_VertexID / grid_resolution.z) % grid_resolution.y);
  ls_cell_location.x = float(gl_VertexID / (grid_resolution.z * grid_resolution.y));

  ls_cell_location += 1.0f;
  ls_cell_location /= float3(grid_resolution + 1);
  ls_cell_location = ls_cell_location * 2.0f - 1.0f;

  float3 ws_cell_location = (model_mat * float4(ls_cell_location, 1.0f)).xyz;
  gl_Position = drw_point_world_to_homogenous(ws_cell_location);
  gl_PointSize = theme.sizes.vert * 2.0f;

  final_color = color_from_id(color_id);

  /* Shade occluded points differently. */
  float4 p = gl_Position / gl_Position.w;
  float z_depth = texture(depth_buffer, p.xy * 0.5f + 0.5f).r * 2.0f - 1.0f;
  float z_delta = p.z - z_depth;
  if (z_delta > 0.0f) {
    float fac = 1.0f - z_delta * 10000.0f;
    /* Smooth blend to avoid flickering. */
    final_color = mix(theme.colors.background, final_color, clamp(fac, 0.2f, 1.0f));
  }

  view_clipping_distances(ws_cell_location);
}
