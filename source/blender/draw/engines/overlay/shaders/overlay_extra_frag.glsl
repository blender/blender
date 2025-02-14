/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

void main()
{
  fragColor = finalColor;
#ifdef IS_SPOT_CONE
  lineOutput = vec4(0.0);
#else
  lineOutput = pack_line_data(gl_FragCoord.xy, edgeStart, edgePos);
  select_id_output(select_id);
#endif
}
