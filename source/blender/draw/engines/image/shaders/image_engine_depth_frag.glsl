/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_colormanagement_lib.glsl"

#define Z_DEPTH_BORDER 1.0f
#define Z_DEPTH_IMAGE 0.75f

bool is_border(float2 uv)
{
  return (uv.x < min_max_uv.x || uv.y < min_max_uv.y || uv.x >= min_max_uv.z ||
          uv.y >= min_max_uv.w);
}

void main()
{
  bool border = is_border(uv_image);
  gl_FragDepth = border ? Z_DEPTH_BORDER : Z_DEPTH_IMAGE;
}
