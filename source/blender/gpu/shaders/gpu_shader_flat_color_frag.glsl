/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_3D_flat_color_infos.hh"

#include "gpu_shader_colorspace_lib.glsl"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_3D_flat_color)

void main()
{
  fragColor = finalColor;
  fragColor = blender_srgb_to_framebuffer_space(fragColor);
}
