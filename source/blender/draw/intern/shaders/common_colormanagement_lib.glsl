/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

float linearrgb_to_srgb(float c)
{
  if (c < 0.0031308) {
    return (c < 0.0) ? 0.0 : c * 12.92;
  }
  else {
    return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
  }
}

vec4 texture_read_as_linearrgb(sampler2D tex, bool premultiplied, vec2 co)
{
  /* By convention image textures return scene linear colors, but
   * overlays still assume srgb. */
  vec4 col = texture(tex, co);
  /* Unpremultiply if stored multiplied, since straight alpha is expected by shaders. */
  if (premultiplied && !(col.a == 0.0 || col.a == 1.0)) {
    col.rgb = col.rgb / col.a;
  }
  return col;
}

vec4 texture_read_as_srgb(sampler2D tex, bool premultiplied, vec2 co)
{
  vec4 col = texture_read_as_linearrgb(tex, premultiplied, co);
  col.r = linearrgb_to_srgb(col.r);
  col.g = linearrgb_to_srgb(col.g);
  col.b = linearrgb_to_srgb(col.b);
  return col;
}
