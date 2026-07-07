/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_checker_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_checker)

void main()
{
  float2 phase = mod(gl_FragCoord.xy, (size * 2));

  if ((phase.x > size && phase.y < size) || (phase.x < size && phase.y > size)) {
    fragColor = color1;
  }
  else {
    fragColor = color2;
  }
}
