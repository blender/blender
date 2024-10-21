/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  const vec2 center = vec2(0, 0.5);
  vec4 tColor = interp.mColor;
  /* if alpha < 0, then encap */
  if (tColor.a < 0) {
    tColor.a = tColor.a * -1.0;
    float dist = length(interp.mTexCoord - center);
    if (dist > 0.25) {
      discard;
    }
  }
  /* Solid */
  fragColor = tColor;
}
