/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_colormanagement_lib.glsl)

#define Z_DEPTH_BORDER 1.0
#define Z_DEPTH_IMAGE 0.75

bool is_border(vec2 uv)
{
  return (uv.x < min_max_uv.x || uv.y < min_max_uv.y || uv.x >= min_max_uv.z ||
          uv.y >= min_max_uv.w);
}

void main()
{
  bool border = is_border(uv_image);
  gl_FragDepth = border ? Z_DEPTH_BORDER : Z_DEPTH_IMAGE;
}
