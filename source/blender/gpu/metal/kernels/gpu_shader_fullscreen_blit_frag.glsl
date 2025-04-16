/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  float4 tex_color = textureLod(imageTexture, screen_uv, mip);
  fragColor = tex_color;
}
