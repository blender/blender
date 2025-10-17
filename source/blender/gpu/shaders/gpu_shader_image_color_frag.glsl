/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_3D_image_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_3D_image_color_scene_linear)

#include "gpu_shader_colorspace_lib.glsl"

void main()
{
  fragColor = texture(image, texCoord_interp) * color;
#ifdef BLENDER_SCENE_LINEAR_TO_REC709
  fragColor = blender_scene_linear_to_rec709_srgb(gpu_scene_linear_to_rec709, fragColor);
#endif
  fragColor = blender_rec709_srgb_to_output_space(fragColor);
}
