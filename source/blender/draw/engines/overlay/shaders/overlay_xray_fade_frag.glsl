/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  float depth = texture(depthTex, uvcoordsvar.xy).r;
  float depth_xray = texture(xrayDepthTex, uvcoordsvar.xy).r;
#ifdef OVERLAY_NEXT
  float depth_xray_infront = texture(xrayDepthTexInfront, uvcoordsvar.xy).r;
  if (((depth_xray_infront == 1.0) && (depth > depth_xray)) || (depth > depth_xray_infront)) {
    fragColor = vec4(opacity);
  }
  else {
    discard;
    return;
  }
#else
  fragColor = vec4((depth < 1.0 && depth > depth_xray) ? opacity : 1.0);
#endif
}
