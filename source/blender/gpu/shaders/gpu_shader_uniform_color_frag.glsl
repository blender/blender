/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_colorspace_lib.glsl)

void main()
{
  fragColor = blender_srgb_to_framebuffer_space(color);
}
