/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_extra_wire_base)

#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

void main()
{
  frag_color = final_color;

  /* Stipple */
  constexpr float dash_width = 6.0f;
  constexpr float dash_factor = 0.5f;

  line_output = pack_line_data(gl_FragCoord.xy, stipple_start, stipple_coord);

  float dist = distance(stipple_start, stipple_coord);

  if (frag_color.a == 0.0f) {
    /* Disable stippling. */
    dist = 0.0f;
  }

  frag_color.a = 1.0f;

#ifndef SELECT_ENABLE
  /* Discarding inside the selection will create some undefined behavior.
   * This is because we force the early depth test to only output the front most fragment.
   * Discarding would expose us to race condition depending on rasterization order. */
  if (fract(dist / dash_width) > dash_factor) {
    gpu_discard_fragment();
  }
#endif

  select_id_output(select_id);
}
