/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  vec4 tex_color = textureLod(imageTexture, uvcoordsvar.xy, mip);
  fragColor = tex_color;
}
