/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_uv_tiled_image_borders)

#ifdef GLSL_CPP_STUBS
/* TODO(fclem): Simplify this. There is no reason for tile_scale and tile_pos to be a mix of
 * defines and push constant. Have to add the define manually for the CPP compilation. */
#  define tile_scale float3(0.0f)
#endif

#include "draw_view_lib.glsl"

void main()
{
  /* `pos` contains the coordinates of a quad (-1..1). but we need the coordinates of an image
   * plane (0..1) */
  float3 image_pos = pos * 0.5f + 0.5f;
  gl_Position = drw_point_world_to_homogenous(tile_scale * image_pos + tile_pos);
}
