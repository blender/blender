/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * For every surfel, compute the incoming radiance from both side.
 * For that, walk the ray surfel linked-list and gather the light from the neighbor surfels.
 * This shader is dispatched for a random ray in a uniform hemisphere as we evaluate the
 * radiance in both directions.
 *
 * Dispatched as 1 thread per surfel.
 */

#include "infos/eevee_lightprobe_volume_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_surfel_ray)

#include "draw_view_lib.glsl"
#include "eevee_lightprobe_sphere_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

float avg_albedo(float3 albedo)
{
  return saturate(dot(albedo, float3(1.0f / 3.0f)));
}

void radiance_transfer(inout Surfel surfel, float3 in_radiance, float in_visibility, float3 L)
{
  /* Clamped brightness. */
  float luma = max(1e-8f, reduce_max(in_radiance));
  in_radiance *= 1.0f - max(0.0f, luma - capture_info_buf.clamp_indirect) / luma;

  float NL = dot(surfel.normal, L);
  /* Lambertian BSDF. Albedo applied later depending on which side of the surfel was hit. */
  constexpr float bsdf = M_1_PI;
  /* From "Global Illumination using Parallel Global Ray-Bundles"
   * Eq. 3: Outgoing light */
  float transfert_fn = (M_TAU / capture_info_buf.sample_count) * bsdf * abs(NL);

  SurfelRadiance radiance = surfel.radiance_indirect[radiance_dst];

  float sample_weight = 1.0f / capture_info_buf.sample_count;
  bool front_facing = (NL > 0.0f);
  if (front_facing) {
    /* Store radiance normalized for spherical harmonic accumulation and for visualization. */
    radiance.front *= radiance.front_weight;
    radiance.front += float4(in_radiance, in_visibility) * transfert_fn *
                      float4(surfel.albedo_front, avg_albedo(surfel.albedo_front));
    radiance.front_weight += sample_weight;
    radiance.front /= radiance.front_weight;
  }
  else {
    /* Store radiance normalized for spherical harmonic accumulation and for visualization. */
    radiance.back *= radiance.back_weight;
    radiance.back += float4(in_radiance, in_visibility) * transfert_fn *
                     float4(surfel.albedo_back, avg_albedo(surfel.albedo_back));
    radiance.back_weight += sample_weight;
    radiance.back /= radiance.back_weight;
  }

  surfel.radiance_indirect[radiance_dst] = radiance;
}

void radiance_transfer_surfel(inout Surfel receiver, Surfel sender)
{
  float3 L = safe_normalize(sender.position - receiver.position);
  bool front_facing = dot(-L, sender.normal) > 0.0f;

  float4 radiance_vis;
  SurfelRadiance sender_radiance_indirect = sender.radiance_indirect[radiance_src];
  if (front_facing) {
    radiance_vis = sender.radiance_direct.front;
    radiance_vis += sender_radiance_indirect.front * sender_radiance_indirect.front_weight;
  }
  else {
    radiance_vis = sender.radiance_direct.back;
    radiance_vis += sender_radiance_indirect.back * sender_radiance_indirect.back_weight;
  }

  if (!capture_info_buf.capture_indirect) {
    radiance_vis.rgb = float3(0.0f);
  }

  radiance_transfer(receiver, radiance_vis.rgb, radiance_vis.a, L);
}

void radiance_transfer_world(inout Surfel receiver, float3 L)
{
  float3 radiance = float3(0.0f);
  float visibility = 0.0f;

  if (capture_info_buf.capture_world_indirect) {
    SphereProbeUvArea atlas_coord = capture_info_buf.world_atlas_coord;
    radiance = lightprobe_spheres_sample(L, 0.0f, atlas_coord).rgb;
  }

  if (capture_info_buf.capture_visibility_indirect) {
    visibility = 1.0f;
  }

  radiance_transfer(receiver, radiance, visibility, L);
}

void main()
{
  int surfel_index = int(gl_GlobalInvocationID.x);
  if (surfel_index >= int(capture_info_buf.surfel_len)) {
    return;
  }

  Surfel surfel = surfel_buf[surfel_index];

  float3 sky_L = drw_world_incident_vector(surfel.position);

  if (surfel.next > -1) {
    Surfel surfel_next = surfel_buf[surfel.next];
    radiance_transfer_surfel(surfel, surfel_next);
  }
  else {
    radiance_transfer_world(surfel, -sky_L);
  }

  if (surfel.prev > -1) {
    Surfel surfel_prev = surfel_buf[surfel.prev];
    radiance_transfer_surfel(surfel, surfel_prev);
  }
  else {
    radiance_transfer_world(surfel, sky_L);
  }

  surfel_buf[surfel_index] = surfel;
}
