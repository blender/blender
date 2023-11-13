/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_colormanagement_lib.glsl)

void main()
{
  vec2 uvs_clamped = clamp(uvs, 0.0, 1.0);
  vec4 tex_color;
  tex_color = texture_read_as_linearrgb(imgTexture, imgPremultiplied, uvs_clamped);

  fragColor = tex_color * ucolor;

  if (!imgAlphaBlend) {
    /* Arbitrary discard anything below 5% opacity.
     * Note that this could be exposed to the User. */
    if (tex_color.a < 0.05) {
      discard;
    }
    else {
      fragColor.a = 1.0;
    }
  }

  /* Pre-multiplied blending. */
  fragColor.rgb *= fragColor.a;
}
