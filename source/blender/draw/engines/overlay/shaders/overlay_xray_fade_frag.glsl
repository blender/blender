/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  /* TODO(fclem): Cleanup naming. Here the xray depth mean the scene depth (from workbench) and
   * simple depth is the overlay depth. */
  float depth_infront = textureLod(depthTexInfront, uvcoordsvar.xy, 0.0).r;
  float depth_xray_infront = textureLod(xrayDepthTexInfront, uvcoordsvar.xy, 0.0).r;
  if (depth_infront != 1.0) {
    if (depth_xray_infront < depth_infront) {
      fragColor = vec4(opacity);
      return;
    }

    discard;
    return;
  }

  float depth = textureLod(depthTex, uvcoordsvar.xy, 0.0).r;
  float depth_xray = textureLod(xrayDepthTex, uvcoordsvar.xy, 0.0).r;
  /* Merge infront depth. */
  if (depth_xray_infront != 1.0) {
    depth_xray = 0.0;
  }

  if (depth_xray < depth) {
    fragColor = vec4(opacity);
    return;
  }

  discard;
  return;
}
