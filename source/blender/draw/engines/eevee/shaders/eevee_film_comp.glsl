/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_film_info.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_film_comp)

#include "eevee_film_lib.glsl"

void main()
{
  ivec2 texel_film = ivec2(gl_GlobalInvocationID.xy);
  /* Not used. */
  vec4 out_color;
  float out_depth;

  if (any(greaterThanEqual(texel_film, uniform_buf.film.extent))) {
    return;
  }

  film_process_data(texel_film, out_color, out_depth);
}
