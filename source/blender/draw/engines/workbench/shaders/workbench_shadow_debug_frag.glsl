/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_shadow_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(workbench_shadow_debug)

void main()
{
  constexpr float a = 0.1f;
#ifdef SHADOW_PASS
  out_debug_color.rgb = gl_FrontFacing ? float3(a, -a, 0.0f) : float3(-a, a, 0.0f);
#else
  out_debug_color.rgb = gl_FrontFacing ? float3(a, a, -a) : float3(-a, -a, a);
#endif
  out_debug_color.a = a;
}
