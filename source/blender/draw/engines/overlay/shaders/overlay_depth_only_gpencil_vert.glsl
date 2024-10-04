/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_gpencil_lib.glsl"
#include "common_view_clipping_lib.glsl"
#include "common_view_lib.glsl"
#include "select_lib.glsl"

void main()
{
  vec3 world_pos;
  vec3 unused_N;
  vec4 unused_color;
  float unused_strength;
  vec2 unused_uv;

  gl_Position = gpencil_vertex(vec4(sizeViewport, sizeViewportInv),
                               world_pos,
                               unused_N,
                               unused_color,
                               unused_strength,
                               unused_uv,
                               gp_interp_flat.sspos,
                               gp_interp_flat.aspect,
                               gp_interp_noperspective.thickness,
                               gp_interp_noperspective.hardness);

  /* Small bias to always be on top of the geom. */
  gl_Position.z -= 1e-3;

  view_clipping_distances(world_pos);

  select_id_set(drw_CustomID);
}
