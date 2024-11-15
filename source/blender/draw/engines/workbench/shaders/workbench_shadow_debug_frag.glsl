/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_shadow_info.hh"

FRAGMENT_SHADER_CREATE_INFO(workbench_shadow_debug)

void main()
{
  const float a = 0.1;
#ifdef SHADOW_PASS
  out_debug_color.rgb = gl_FrontFacing ? vec3(a, -a, 0.0) : vec3(-a, a, 0.0);
#else
  out_debug_color.rgb = gl_FrontFacing ? vec3(a, a, -a) : vec3(-a, -a, a);
#endif
  out_debug_color.a = a;
}
