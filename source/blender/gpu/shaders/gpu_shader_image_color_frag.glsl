/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_image_rect_color_info.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_image_rect_color)

void main()
{
  fragColor = texture(image, texCoord_interp) * color;
}
