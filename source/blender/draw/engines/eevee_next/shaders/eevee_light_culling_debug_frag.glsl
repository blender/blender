/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Debug Shader outputting a gradient of orange - white - blue to mark culling hotspots.
 * Green pixels are error pixels that are missing lights from the culling pass (i.e: when culling
 * pass is not conservative enough).
 */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_iter_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  float depth = texelFetch(hiz_tx, texel, 0).r;
  float vP_z = get_view_z_from_depth(depth);
  vec3 P = get_world_space_from_depth(uvcoordsvar.xy, depth);

  float light_count = 0.0;
  uint light_cull = 0u;
  vec2 px = gl_FragCoord.xy;
  LIGHT_FOREACH_BEGIN_LOCAL (light_cull_buf, light_zbin_buf, light_tile_buf, px, vP_z, l_idx) {
    LightData light = light_buf[l_idx];
    light_cull |= 1u << l_idx;
    light_count += 1.0;
  }
  LIGHT_FOREACH_END

  uint light_nocull = 0u;
  LIGHT_FOREACH_BEGIN_LOCAL_NO_CULL(light_cull_buf, l_idx)
  {
    LightData light = light_buf[l_idx];
    vec3 L;
    float dist;
    light_vector_get(light, P, L, dist);
    if (light_attenuation(light, L, dist) > 0.0) {
      light_nocull |= 1u << l_idx;
    }
  }
  LIGHT_FOREACH_END

  vec4 color = vec4(heatmap_gradient(light_count / 4.0), 1.0);

  if ((light_cull & light_nocull) != light_nocull) {
    /* ERROR. Some lights were culled incorrectly. */
    color = vec4(0.0, 1.0, 0.0, 1.0);
  }

  out_debug_color_add = vec4(color.rgb, 0.0) * 0.2;
  out_debug_color_mul = color;
}
