/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Usage tagging
 *
 * Shadow pages are only allocated if they are visible.
 * This pass scans all volume froxels and tags tiles needed for shadowing.
 */

#pragma BLENDER_REQUIRE(eevee_volume_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_tag_usage_lib.glsl)

void main()
{
  ivec3 froxel = ivec3(gl_GlobalInvocationID);

  if (any(greaterThanEqual(froxel, uniform_buf.volumes.tex_size))) {
    return;
  }

  vec3 extinction = imageLoad(in_extinction_img, froxel).rgb;
  vec3 scattering = imageLoad(in_scattering_img, froxel).rgb;

  if (is_zero(extinction) || is_zero(scattering)) {
    return;
  }

  float offset = sampling_rng_1D_get(SAMPLING_VOLUME_W);
  float jitter = interlieved_gradient_noise(vec2(froxel.xy), 0.0, offset);

  vec3 uvw = (vec3(froxel) + vec3(0.5, 0.5, jitter)) * uniform_buf.volumes.inv_tex_size;
  vec3 ss_P = volume_resolve_to_screen(uvw);
  vec3 vP = drw_point_screen_to_view(vec3(ss_P.xy, ss_P.z));
  vec3 P = drw_point_view_to_world(vP);

  float depth = texelFetch(hiz_tx, froxel.xy, uniform_buf.volumes.tile_size_lod).r;
  if (depth < ss_P.z) {
    return;
  }

  vec2 pixel = ((vec2(froxel.xy) + 0.5) * uniform_buf.volumes.inv_tex_size.xy) *
               uniform_buf.volumes.main_view_extent;

  int bias = uniform_buf.volumes.tile_size_lod;
  shadow_tag_usage(vP, P, drw_world_incident_vector(P), 0.01, pixel, bias);
}
