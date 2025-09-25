/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_extra_groundline)

#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

void main()
{
  frag_color = final_color;
#ifdef IS_SPOT_CONE
  line_output = float4(0.0f);
#else
  line_output = pack_line_data(gl_FragCoord.xy, edge_start, edge_pos);
  select_id_output(select_id);
#endif
}
