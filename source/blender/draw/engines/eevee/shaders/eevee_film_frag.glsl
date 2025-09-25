/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_film_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_film_frag)

#include "eevee_film_lib.glsl"

void main()
{
  int2 texel_film = int2(gl_FragCoord.xy) - uniform_buf.film.offset;
  float out_depth;

  if (uniform_buf.film.display_only) {
    out_depth = imageLoadFast(depth_img, texel_film).r;

    if (display_id == -1) {
      out_color = texelFetch(in_combined_tx, texel_film, 0);
    }
    else if (uniform_buf.film.display_storage_type == PASS_STORAGE_VALUE) {
      out_color.rgb = imageLoadFast(value_accum_img, int3(texel_film, display_id)).rrr;
      out_color.a = 1.0f;
    }
    else if (uniform_buf.film.display_storage_type == PASS_STORAGE_COLOR) {
      out_color = imageLoadFast(color_accum_img, int3(texel_film, display_id));
    }
    else /* PASS_STORAGE_CRYPTOMATTE */ {
      out_color = cryptomatte_false_color(
          imageLoadFast(cryptomatte_img, int3(texel_film, display_id)).r);
    }
  }
  else {
    film_process_data(texel_film, out_color, out_depth);
  }

  gl_FragDepth = drw_depth_view_to_screen(-out_depth);

  gl_FragDepth = film_display_depth_amend(texel_film, gl_FragDepth);
}
