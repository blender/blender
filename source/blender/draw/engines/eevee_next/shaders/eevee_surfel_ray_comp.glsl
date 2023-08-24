/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * For every surfel, compute the incomming radiance from both side.
 * For that, walk the ray surfel linked-list and gather the light from the neighbor surfels.
 * This shader is dispatched for a random ray in a uniform hemisphere as we evaluate the
 * radiance in both directions.
 *
 * Dispatched as 1 thread per surfel.
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_reflection_probe_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)

float avg_albedo(vec3 albedo)
{
  return saturate(dot(albedo, vec3(1.0 / 3.0)));
}

void radiance_transfer(inout Surfel surfel, vec3 in_radiance, float in_visibility, vec3 L)
{
  float NL = dot(surfel.normal, L);
  /* Lambertian BSDF. Albedo applied later depending on which side of the surfel was hit. */
  const float bsdf = M_1_PI;
  /* From "Global Illumination using Parallel Global Ray-Bundles"
   * Eq. 3: Outgoing light */
  float transfert_fn = (M_TAU / capture_info_buf.sample_count) * bsdf * abs(NL);

  SurfelRadiance radiance = surfel.radiance_indirect[radiance_dst];

  float sample_weight = 1.0 / capture_info_buf.sample_count;
  bool front_facing = (NL > 0.0);
  if (front_facing) {
    /* Store radiance normalized for spherical harmonic accumulation and for visualization. */
    radiance.front *= radiance.front_weight;
    radiance.front += vec4(in_radiance, in_visibility) * transfert_fn *
                      vec4(surfel.albedo_front, avg_albedo(surfel.albedo_front));
    radiance.front_weight += sample_weight;
    radiance.front /= radiance.front_weight;
  }
  else {
    /* Store radiance normalized for spherical harmonic accumulation and for visualization. */
    radiance.back *= radiance.back_weight;
    radiance.back += vec4(in_radiance, in_visibility) * transfert_fn *
                     vec4(surfel.albedo_back, avg_albedo(surfel.albedo_back));
    radiance.back_weight += sample_weight;
    radiance.back /= radiance.back_weight;
  }

  surfel.radiance_indirect[radiance_dst] = radiance;
}

void radiance_transfer_surfel(inout Surfel receiver, Surfel sender)
{
  vec3 L = safe_normalize(sender.position - receiver.position);
  bool front_facing = dot(-L, sender.normal) > 0.0;

  vec4 radiance_vis;
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
    radiance_vis.rgb = vec3(0.0);
  }

  radiance_transfer(receiver, radiance_vis.rgb, radiance_vis.a, L);
}

void radiance_transfer_world(inout Surfel receiver, vec3 L)
{
  vec3 radiance = vec3(0.0);
  float visibility = 0.0;

  if (capture_info_buf.capture_world_indirect) {
    radiance = reflection_probes_world_sample(L, 0.0).rgb;
  }

  if (capture_info_buf.capture_visibility_indirect) {
    visibility = 1.0;
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

  vec3 sky_L = cameraVec(surfel.position);

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
