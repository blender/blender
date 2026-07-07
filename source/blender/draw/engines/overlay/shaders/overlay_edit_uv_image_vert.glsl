/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_edit_uv_mask_image)

#include "draw_view_lib.glsl"

void main()
{
  /* `pos` contains the coordinates of a quad (-1..1). but we need the coordinates of an image
   * plane (0..1) */
  float3 image_pos = pos * 0.5f + 0.5f;
  gl_Position = drw_point_world_to_homogenous(
      float3(image_pos.xy * brush_scale + brush_offset, 0.0f));
  uvs = image_pos.xy;
}
