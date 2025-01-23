/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_utildefines_lib.glsl"
#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

void main()
{
#if !defined(POINTS) && !defined(CURVES)
  /* Needed only because of wireframe slider.
   * If we could get rid of it would be nice because of performance drain of discard. */
  if (edgeStart.r == -1.0) {
    discard;
    return;
  }
#endif

  lineOutput = vec4(0.0);

#if defined(POINTS)
  vec2 centered = abs(gl_PointCoord - vec2(0.5));
  float dist = max(centered.x, centered.y);

  float fac = dist * dist * 4.0;
  /* Create a small gradient so that dense objects have a small fresnel effect. */
  /* Non linear blend. */
  vec3 rim_col = sqrt(finalColorInner.rgb);
  vec3 wire_col = sqrt(finalColor.rgb);
  vec3 final_front_col = mix(rim_col, wire_col, 0.35);
  fragColor = vec4(mix(final_front_col, rim_col, saturate(fac)), 1.0);
  fragColor *= fragColor;

#elif !defined(SELECT_ENABLE)
  lineOutput = pack_line_data(gl_FragCoord.xy, edgeStart, edgePos);
  fragColor = finalColor;

#  ifndef CUSTOM_DEPTH_BIAS_CONST
/* TODO(fclem): Cleanup after overlay next. */
#    ifndef CUSTOM_DEPTH_BIAS
  const bool use_custom_depth_bias = false;
#    else
  const bool use_custom_depth_bias = true;
#    endif
#  endif

#  if !defined(CURVES)
  if (use_custom_depth_bias) {
    vec2 dir = lineOutput.xy * 2.0 - 1.0;
    bool dir_horiz = abs(dir.x) > abs(dir.y);

    vec2 uv = gl_FragCoord.xy * sizeViewportInv;
    float depth_occluder = texture(depthTex, uv).r;
    float depth_min = depth_occluder;
    vec2 uv_offset = sizeViewportInv;
    if (dir_horiz) {
      uv_offset.y = 0.0;
    }
    else {
      uv_offset.x = 0.0;
    }

    depth_min = min(depth_min, texture(depthTex, uv - uv_offset).r);
    depth_min = min(depth_min, texture(depthTex, uv + uv_offset).r);

    float delta = abs(depth_occluder - depth_min);

#    ifndef SELECT_ENABLE
    if (gl_FragCoord.z < (depth_occluder + delta) && gl_FragCoord.z > depth_occluder) {
      gl_FragDepth = depth_occluder;
    }
    else {
      gl_FragDepth = gl_FragCoord.z;
    }
#    endif
  }
#  endif
#endif

  select_id_output(select_id);
}
