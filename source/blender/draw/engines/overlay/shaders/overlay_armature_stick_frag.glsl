/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_armature_stick)

#include "select_lib.glsl"

void main()
{
  float fac = smoothstep(1.0f, 0.2f, color_fac);
  frag_color.rgb = mix(final_inner_color.rgb, final_wire_color.rgb, fac);
  frag_color.a = alpha;
  line_output = float4(0.0f);
  select_id_output(select_id);
}
