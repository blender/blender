/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  float depth = texture(depthTex, uvcoordsvar.xy).r;
  float depth_xray = texture(xrayDepthTex, uvcoordsvar.xy).r;
  fragColor = vec4((depth < 1.0 && depth > depth_xray) ? opacity : 1.0);
}
