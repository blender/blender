/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_diag_stripes_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_diag_stripes)

void main()
{
  float phase = mod((gl_FragCoord.x + gl_FragCoord.y), float(size1 + size2));

  if (phase < size1) {
    fragColor = color1;
  }
  else {
    fragColor = color2;
  }
}
