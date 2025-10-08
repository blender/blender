/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * For every irradiance probe sample, compute the incoming radiance from both side.
 * This is the same as the surfel ray but we do not actually transport the light, we only capture
 * the irradiance as spherical harmonic coefficients.
 *
 * Dispatched as 1 thread per irradiance probe sample.
 */

#include "infos/eevee_lightprobe_volume_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_lightprobe_volume_ray)

#include "eevee_lightprobe_lib.glsl"
#include "eevee_lightprobe_sphere_lib.glsl"
#include "eevee_spherical_harmonics_lib.glsl"
#include "eevee_surfel_list_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

void irradiance_capture(float3 L,
                        float3 irradiance,
                        float visibility,
                        inout SphericalHarmonicL1 sh)
{
  float3 lL = transform_direction(capture_info_buf.irradiance_grid_world_to_local_rotation, L);

  /* Spherical harmonics need to be weighted by sphere area. */
  irradiance *= 4.0f * M_PI;
  visibility *= 4.0f * M_PI;

  spherical_harmonics_encode_signal_sample(lL, float4(irradiance, visibility), sh);
}

void irradiance_capture_surfel(Surfel surfel, float3 P, inout SphericalHarmonicL1 sh)
{
  float3 L = safe_normalize(surfel.position - P);
  bool facing = dot(-L, surfel.normal) > 0.0f;
  SurfelRadiance surfel_radiance_indirect = surfel.radiance_indirect[radiance_src];

  float4 irradiance_vis = float4(0.0f);
  irradiance_vis += facing ? surfel.radiance_direct.front : surfel.radiance_direct.back;

  /* Clamped brightness. */
  float luma = max(1e-8f, reduce_max(irradiance_vis.rgb));
  irradiance_vis.rgb *= 1.0f - max(0.0f, luma - capture_info_buf.clamp_direct) / luma;

  /* NOTE: The indirect radiance is already normalized and this is wanted, because we are not
   * integrating the same signal and we would have the SH lagging behind the surfel integration
   * otherwise. */
  irradiance_vis += facing ? surfel_radiance_indirect.front : surfel_radiance_indirect.back;

  irradiance_capture(L, irradiance_vis.rgb, irradiance_vis.a, sh);
}

void validity_capture_surfel(Surfel surfel, float3 P, inout float validity)
{
  float3 L = safe_normalize(surfel.position - P);
  bool facing = surfel.double_sided || dot(-L, surfel.normal) > 0.0f;
  validity += float(facing);
}

void validity_capture_world(float3 L, inout float validity)
{
  validity += 1.0f;
}

void irradiance_capture_world(float3 L, inout SphericalHarmonicL1 sh)
{
  float3 radiance = float3(0.0f);
  float visibility = 0.0f;

  if (capture_info_buf.capture_world_direct) {
    SphereProbeUvArea atlas_coord = capture_info_buf.world_atlas_coord;
    radiance = lightprobe_spheres_sample(L, 0.0f, atlas_coord).rgb;

    /* Clamped brightness. */
    float luma = max(1e-8f, reduce_max(radiance));
    radiance *= 1.0f - max(0.0f, luma - capture_info_buf.clamp_direct) / luma;
  }

  if (capture_info_buf.capture_visibility_direct) {
    visibility = 1.0f;
  }

  irradiance_capture(L, radiance, visibility, sh);
}

void main()
{
  int3 grid_coord = int3(gl_GlobalInvocationID);

  if (any(greaterThanEqual(grid_coord, capture_info_buf.irradiance_grid_size))) {
    return;
  }

  float3 P = lightprobe_volume_grid_sample_position(
      capture_info_buf.irradiance_grid_local_to_world,
      capture_info_buf.irradiance_grid_size,
      grid_coord);

  /* Add virtual offset to avoid baking inside of geometry as much as possible. */
  P += imageLoadFast(virtual_offset_img, grid_coord).xyz;

  /* Project to get ray linked list. */
  float irradiance_sample_ray_distance;
  int list_index = surfel_list_index_get(
      list_info_buf.ray_grid_size, P, irradiance_sample_ray_distance);

  /* Walk the ray to get which surfels the irradiance sample is between. */
  int surfel_prev = -1;
  int surfel_next = list_start_buf[list_index];
  /* Avoid spinning for eternity. */
  for (int i = 0; i < 9999; i++) {
    if (surfel_next <= -1) {
      break;
    }
    /* Reminder: List is sorted with highest value first. */
    if (surfel_buf[surfel_next].ray_distance < irradiance_sample_ray_distance) {
      break;
    }
    surfel_prev = surfel_next;
    surfel_next = surfel_buf[surfel_next].next;
    assert(surfel_prev != surfel_next);
  }

  float3 sky_L = drw_world_incident_vector(P);

  SphericalHarmonicL1 sh;
  sh.L0.M0 = imageLoadFast(irradiance_L0_img, grid_coord);
  sh.L1.Mn1 = imageLoadFast(irradiance_L1_a_img, grid_coord);
  sh.L1.M0 = imageLoadFast(irradiance_L1_b_img, grid_coord);
  sh.L1.Mp1 = imageLoadFast(irradiance_L1_c_img, grid_coord);
  float validity = imageLoadFast(validity_img, grid_coord).r;

  /* Un-normalize for accumulation. */
  float weight_captured = capture_info_buf.sample_index * 2.0f;
  sh.L0.M0 *= weight_captured;
  sh.L1.Mn1 *= weight_captured;
  sh.L1.M0 *= weight_captured;
  sh.L1.Mp1 *= weight_captured;
  validity *= weight_captured;

  if (surfel_next > -1) {
    Surfel surfel = surfel_buf[surfel_next];
    irradiance_capture_surfel(surfel, P, sh);
    validity_capture_surfel(surfel, P, validity);
#if 0 /* For debugging the volume rays list. */
    drw_debug_line(surfel.position, P, float4(0, 1, 0, 1), drw_debug_persistent_lifetime);
#endif
  }
  else {
    irradiance_capture_world(-sky_L, sh);
    validity_capture_world(-sky_L, validity);
#if 0 /* For debugging the volume rays list. */
    drw_debug_line(P - sky_L, P, float4(0, 1, 1, 1), drw_debug_persistent_lifetime);
#endif
  }

  if (surfel_prev > -1) {
    Surfel surfel = surfel_buf[surfel_prev];
    irradiance_capture_surfel(surfel, P, sh);
    validity_capture_surfel(surfel, P, validity);
#if 0 /* For debugging the volume rays list. */
    drw_debug_line(surfel.position, P, float4(1, 0, 1, 1), drw_debug_persistent_lifetime);
#endif
  }
  else {
    irradiance_capture_world(sky_L, sh);
    validity_capture_world(sky_L, validity);
#if 0 /* For debugging the volume rays list. */
    drw_debug_line(P + sky_L, P, float4(1, 1, 0, 1), drw_debug_persistent_lifetime);
#endif
  }

  /* Normalize for storage. We accumulated 2 samples. */
  weight_captured += 2.0f;
  sh.L0.M0 /= weight_captured;
  sh.L1.Mn1 /= weight_captured;
  sh.L1.M0 /= weight_captured;
  sh.L1.Mp1 /= weight_captured;
  validity /= weight_captured;

  imageStoreFast(irradiance_L0_img, grid_coord, sh.L0.M0);
  imageStoreFast(irradiance_L1_a_img, grid_coord, sh.L1.Mn1);
  imageStoreFast(irradiance_L1_b_img, grid_coord, sh.L1.M0);
  imageStoreFast(irradiance_L1_c_img, grid_coord, sh.L1.Mp1);
  imageStoreFast(validity_img, grid_coord, float4(validity));
}
