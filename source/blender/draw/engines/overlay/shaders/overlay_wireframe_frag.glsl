/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_wireframe_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_wireframe_base)

#include "gpu_shader_utildefines_lib.glsl"
#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

void main()
{
#if !defined(POINTS) && !defined(CURVES)
  /* Needed only because of wireframe slider.
   * If we could get rid of it would be nice because of performance drain of discard. */
  if (edge_start.r == -1.0f) {
    gpu_discard_fragment();
    return;
  }
#endif

  line_output = float4(0.0f);

#if defined(POINTS)
  float2 centered = abs(gl_PointCoord - float2(0.5f));
  float dist = max(centered.x, centered.y);

  float fac = dist * dist * 4.0f;
  /* Create a small gradient so that dense objects have a small fresnel effect. */
  /* Non linear blend. */
  float3 rim_col = sqrt(final_color_inner.rgb);
  float3 wire_col = sqrt(final_color.rgb);
  float3 final_front_col = mix(rim_col, wire_col, 0.35f);
  frag_color = float4(mix(final_front_col, rim_col, saturate(fac)), 1.0f);
  frag_color *= frag_color;

#elif !defined(SELECT_ENABLE)
  line_output = pack_line_data(gl_FragCoord.xy, edge_start, edge_pos);
  frag_color = final_color;

#  if !defined(CURVES)
  gl_FragDepth = gl_FragCoord.z;
  if (use_custom_depth_bias) {
    float2 dir = line_output.xy * 2.0f - 1.0f;
    bool dir_horiz = abs(dir.x) > abs(dir.y);

    float2 uv = gl_FragCoord.xy * uniform_buf.size_viewport_inv;
    float depth_occluder = texture(depth_tx, uv).r;
    float depth_min = depth_occluder;
    float2 uv_offset = uniform_buf.size_viewport_inv;
    if (dir_horiz) {
      uv_offset.y = 0.0f;
    }
    else {
      uv_offset.x = 0.0f;
    }

    depth_min = min(depth_min, texture(depth_tx, uv - uv_offset).r);
    depth_min = min(depth_min, texture(depth_tx, uv + uv_offset).r);

    float delta = abs(depth_occluder - depth_min);

#    ifndef SELECT_ENABLE
    if (gl_FragCoord.z < (depth_occluder + delta) && gl_FragCoord.z > depth_occluder) {
      gl_FragDepth = depth_occluder;
    }
#    endif
  }
#  endif
#endif

  select_id_output(select_id);
}
