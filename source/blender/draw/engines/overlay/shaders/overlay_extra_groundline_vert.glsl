/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

void main()
{
  select_id_set(in_select_buf[gl_InstanceID]);
  finalColor = colorLight;

  /* Relative to DPI scaling. Have constant screen size. */
  vec3 screen_pos = ViewMatrixInverse[0].xyz * pos.x + ViewMatrixInverse[1].xyz * pos.y;
  vec3 inst_pos = data_buf[gl_InstanceID].xyz;
  vec3 p = inst_pos;
  p.z *= (pos.z == 0.0) ? 0.0 : 1.0;
  float screen_size = mul_project_m4_v3_zfac(globalsBlock.pixel_fac, p) * sizePixel;
  vec3 world_pos = p + screen_pos * screen_size;

  gl_Position = drw_point_world_to_homogenous(world_pos);

  /* Convert to screen position [0..sizeVp]. */
  edgePos = edgeStart = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;

  view_clipping_distances(world_pos);
}
