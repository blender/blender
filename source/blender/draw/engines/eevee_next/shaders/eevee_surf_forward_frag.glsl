/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Forward lighting evaluation: Lighting is evaluated during the geometry rasterization.
 *
 * This is used by alpha blended materials and materials using Shader to RGB nodes.
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ambient_occlusion_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_volume_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_forward_lib.glsl)
#pragma BLENDER_REQUIRE(common_hair_lib.glsl)

/* Global thickness because it is needed for closure_to_rgba. */
float g_thickness;

vec4 closure_to_rgba(Closure cl_unused)
{
  vec3 radiance, transmittance;
  forward_lighting_eval(g_thickness, radiance, transmittance);

  /* Reset for the next closure tree. */
  closure_weights_reset();

  return vec4(radiance, saturate(1.0 - average(transmittance)));
}

void main()
{
  /* Clear AOVs first. In case the material renders to them. */
  clear_aovs();

  init_globals();

  float noise = utility_tx_fetch(utility_tx, gl_FragCoord.xy, UTIL_BLUE_NOISE_LAYER).r;
  g_closure_rand = fract(noise + sampling_rng_1D_get(SAMPLING_CLOSURE));

  fragment_displacement();

  g_thickness = max(0.0, nodetree_thickness());

  nodetree_surface();

  vec3 radiance, transmittance;
  forward_lighting_eval(g_thickness, radiance, transmittance);

  /* Volumetric resolve and compositing. */
  vec2 uvs = gl_FragCoord.xy * uniform_buf.volumes.viewport_size_inv;
  VolumeResolveSample vol = volume_resolve(
      vec3(uvs, gl_FragCoord.z), volume_transmittance_tx, volume_scattering_tx);
  /* Removes the part of the volume scattering that has
   * already been added to the destination pixels by the opaque resolve.
   * Since we do that using the blending pipeline we need to account for material transmittance. */
  vol.scattering -= vol.scattering * g_transmittance;
  radiance = radiance * vol.transmittance + vol.scattering;

  radiance *= 1.0 - saturate(g_holdout);

  out_radiance = vec4(radiance, 0.0);
  out_transmittance = vec4(transmittance, saturate(average(transmittance)));
}
