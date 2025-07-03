/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_image_rect_color_info.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_image_rect_color)

#include "gpu_shader_colorspace_lib.glsl"

void main()
{
  fragColor = texture(image, texCoord_interp) * color;
  fragColor = blender_srgb_to_framebuffer_space(fragColor);
}
