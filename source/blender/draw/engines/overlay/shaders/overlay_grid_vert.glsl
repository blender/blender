/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_grid_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_grid_next)

/**
 * Infinite grid:
 * Draw anti-aliased grid and axes of different sizes with smooth blending between Level of
 * details. We draw multiple triangles to avoid float precision issues due to perspective
 * interpolation.
 */

#include "draw_view_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

void main()
{
  float3 vert_pos;

  /* Project camera pos to the needed plane */
  if (flag_test(grid_flag, PLANE_XY)) {
    vert_pos = float3(pos.x, pos.y, 0.0f);
  }
  else if (flag_test(grid_flag, PLANE_XZ)) {
    vert_pos = float3(pos.x, 0.0f, pos.y);
  }
  else if (flag_test(grid_flag, PLANE_YZ)) {
    vert_pos = float3(0.0f, pos.x, pos.y);
  }
  else /* PLANE_IMAGE */ {
    vert_pos = float3(pos.xy * 0.5f + 0.5f, 0.0f);
  }

  local_pos = vert_pos;

  float3 real_pos = drw_view_position() * plane_axes + vert_pos * grid_buf.size.xyz;

  /* Used for additional Z axis */
  if (flag_test(grid_flag, CLIP_ZPOS)) {
    real_pos.z = clamp(real_pos.z, 0.0f, 1e30f);
    local_pos.z = clamp(local_pos.z, 0.0f, 1.0f);
  }
  if (flag_test(grid_flag, CLIP_ZNEG)) {
    real_pos.z = clamp(real_pos.z, -1e30f, 0.0f);
    local_pos.z = clamp(local_pos.z, -1.0f, 0.0f);
  }

  gl_Position = drw_view().winmat * (drw_view().viewmat * float4(real_pos, 1.0f));
}
