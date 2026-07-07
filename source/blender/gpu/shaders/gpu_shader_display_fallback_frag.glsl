/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_image_overlays_merge_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_cycles_display_fallback)

void main()
{
  fragColor = texture(image_texture, texCoord_interp);
}
