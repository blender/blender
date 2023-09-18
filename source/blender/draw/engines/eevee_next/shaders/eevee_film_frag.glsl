/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_film_lib.glsl)

void main()
{
  ivec2 texel_film = ivec2(gl_FragCoord.xy) - uniform_buf.film.offset;
  float out_depth;

  if (uniform_buf.film.display_only) {
    out_depth = imageLoad(depth_img, texel_film).r;

    if (uniform_buf.film.display_id == -1) {
      out_color = texelFetch(in_combined_tx, texel_film, 0);
    }
    else if (uniform_buf.film.display_storage_type == PASS_STORAGE_VALUE) {
      out_color.rgb =
          imageLoad(value_accum_img, ivec3(texel_film, uniform_buf.film.display_id)).rrr;
      out_color.a = 1.0;
    }
    else if (uniform_buf.film.display_storage_type == PASS_STORAGE_COLOR) {
      out_color = imageLoad(color_accum_img, ivec3(texel_film, uniform_buf.film.display_id));
    }
    else /* PASS_STORAGE_CRYPTOMATTE */ {
      out_color = cryptomatte_false_color(
          imageLoad(cryptomatte_img, ivec3(texel_film, uniform_buf.film.display_id)).r);
    }
  }
  else {
    film_process_data(texel_film, out_color, out_depth);
  }

  gl_FragDepth = get_depth_from_view_z(-out_depth);

  gl_FragDepth = film_display_depth_ammend(texel_film, gl_FragDepth);
}
