/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  vec4 tex = texture(image, texCoord_interp);
  tex.rgb = ((0.3333333 * factor) * vec3(tex.r + tex.g + tex.b)) + (tex.rgb * (1.0 - factor));
  fragColor = tex * color;
}
