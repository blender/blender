/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  fragColor = ucolor;
  float scene_depth = texelFetch(depthBuffer, ivec2(gl_FragCoord.xy), 0).r;
  fragColor.a = (scene_depth == 1.0) ? 1.0 : 0.0;
}
