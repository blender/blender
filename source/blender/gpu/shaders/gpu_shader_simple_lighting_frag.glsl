/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_simple_lighting_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_simple_lighting)

void main()
{
  fragColor = simple_lighting_data.l_color;
  fragColor.xyz *= clamp(dot(normalize(normal), simple_lighting_data.light), 0.0f, 1.0f);
}
