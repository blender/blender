/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_lib.glsl"

#ifndef USE_GPU_SHADER_CREATE_INFO
in vec3 pos;
#endif

void main()
{
  /* `pos` contains the coordinates of a quad (-1..1). but we need the coordinates of an image
   * plane (0..1) */
  vec3 image_pos = pos * 0.5 + 0.5;
#ifdef OVERLAY_NEXT
  gl_Position = point_world_to_ndc(tile_scale * image_pos + tile_pos);
#else
  gl_Position = point_object_to_ndc(image_pos);
#endif
}
