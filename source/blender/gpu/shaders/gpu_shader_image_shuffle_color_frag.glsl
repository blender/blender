/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_image_shuffle_color_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_image_shuffle_color)

void main()
{
  float4 sampled_color = texture(image, texCoord_interp);
  fragColor = float4(sampled_color.r * shuffle.r + sampled_color.g * shuffle.g +
                     sampled_color.b * shuffle.b + sampled_color.a * shuffle.a) *
              color;
}
