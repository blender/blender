/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_volume_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_volume_velocity_mac)

#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

void main()
{
  frag_color = final_color;
#if defined(LINE_OUTPUT_NO_DUMMY)
  line_output = pack_line_data(gl_FragCoord.xy, edge_start, edge_pos);
#elif defined(LINE_OUTPUT)
  line_output = float4(0.0f);
#endif
  select_id_output(select_id);
}
