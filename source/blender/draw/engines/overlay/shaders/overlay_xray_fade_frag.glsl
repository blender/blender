/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_antialiasing_info.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_xray_fade)

void main()
{
  /* TODO(fclem): Cleanup naming. Here the xray depth mean the scene depth (from workbench) and
   * simple depth is the overlay depth. */
  float depth_infront = textureLod(depthTexInfront, screen_uv, 0.0f).r;
  float depth_xray_infront = textureLod(xrayDepthTexInfront, screen_uv, 0.0f).r;
  if (depth_infront != 1.0f) {
    if (depth_xray_infront < depth_infront) {
      fragColor = float4(opacity);
      return;
    }

    discard;
    return;
  }

  float depth = textureLod(depthTex, screen_uv, 0.0f).r;
  float depth_xray = textureLod(xrayDepthTex, screen_uv, 0.0f).r;
  /* Merge infront depth. */
  if (depth_xray_infront != 1.0f) {
    depth_xray = 0.0f;
  }

  if (depth_xray < depth) {
    fragColor = float4(opacity);
    return;
  }

  discard;
  return;
}
