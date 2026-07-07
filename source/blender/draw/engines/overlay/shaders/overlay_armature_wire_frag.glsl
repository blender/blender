/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_armature_wire)

#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

void main()
{
  line_output = pack_line_data(gl_FragCoord.xy, edge_start, edge_pos);
  frag_color = float4(final_color.rgb, final_color.a * alpha);
  select_id_output(select_id);
}
