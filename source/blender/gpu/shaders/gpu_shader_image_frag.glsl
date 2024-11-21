/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_image_info.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_image_common)

void main()
{
  fragColor = texture(image, texCoord_interp);
}
