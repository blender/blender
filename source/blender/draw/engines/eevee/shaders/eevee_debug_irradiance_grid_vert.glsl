/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_lightprobe_volume_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_debug_irradiance_grid)

#include "draw_view_lib.glsl"
#include "eevee_lightprobe_lib.glsl"
#include "gpu_shader_math_vector_compare_lib.glsl"

void main()
{
  int3 grid_resolution = textureSize(debug_data_tx, 0);
  int3 grid_sample;
  int sample_id = 0;
  if (debug_mode == DEBUG_IRRADIANCE_CACHE_VALIDITY) {
    /* Points. */
    sample_id = gl_VertexID;
  }
  else if (debug_mode == DEBUG_IRRADIANCE_CACHE_VIRTUAL_OFFSET) {
    /* Lines. */
    sample_id = gl_VertexID / 2;
  }

  grid_sample.x = (sample_id % grid_resolution.x);
  grid_sample.y = (sample_id / grid_resolution.x) % grid_resolution.y;
  grid_sample.z = (sample_id / (grid_resolution.x * grid_resolution.y));

  float3 P = lightprobe_volume_grid_sample_position(grid_mat, grid_resolution, grid_sample);

  float4 debug_data = texelFetch(debug_data_tx, grid_sample, 0);
  if (debug_mode == DEBUG_IRRADIANCE_CACHE_VALIDITY) {
    interp_color = float4(1.0f - debug_data.r, debug_data.r, 0.0f, 0.0f);
    gl_PointSize = 3.0f;
    if (debug_data.r > debug_value) {
      /* Only render points that are below threshold. */
      gl_Position = float4(0.0f);
      gl_PointSize = 0.0f;
      return;
    }
  }
  else if (debug_mode == DEBUG_IRRADIANCE_CACHE_VIRTUAL_OFFSET) {
    if (is_zero(debug_data.xyz)) {
      /* Only render points that have offset. */
      gl_Position = float4(0.0f);
      gl_PointSize = 0.0f;
      return;
    }

    if ((gl_VertexID & 1) == 1) {
      P += debug_data.xyz;
    }
  }

  gl_Position = drw_point_world_to_homogenous(P);
  gl_Position.z -= 2.5e-5f;
  gl_PointSize = 3.0f;
}
