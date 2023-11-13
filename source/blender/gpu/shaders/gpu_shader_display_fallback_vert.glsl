/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

vec2 normalize_coordinates()
{
  return (vec2(2.0) * (pos / fullscreen)) - vec2(1.0);
}

void main()
{
  gl_Position = vec4(normalize_coordinates(), 0.0, 1.0);
  texCoord_interp = texCoord;
}
