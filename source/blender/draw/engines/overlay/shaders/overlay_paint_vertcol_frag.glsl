/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

vec3 linear_to_srgb_attr(vec3 c)
{
  c = max(c, vec3(0.0));
  vec3 c1 = c * 12.92;
  vec3 c2 = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
  return mix(c1, c2, step(vec3(0.0031308), c));
}

void main()
{
  vec3 color = linear_to_srgb_attr(finalColor);

  if (useAlphaBlend) {
    fragColor = vec4(color, opacity);
  }
  else {
    /* mix with 1.0 -> is like opacity when using multiply blend mode */
    fragColor = vec4(mix(vec3(1.0), color, opacity), 1.0);
  }
}
