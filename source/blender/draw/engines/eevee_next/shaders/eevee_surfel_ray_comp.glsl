
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

void radiance_transfer(inout Surfel surfel, vec3 in_radiance, vec3 L)
{
  float NL = dot(surfel.normal, L);
  /* Lambertian BSDF. Albedo applied later depending on which side of the surfel was hit. */
  float bsdf = M_1_PI;
  /* From "Global Illumination using Parallel Global Ray-Bundles"
   * Eq. 3: Outgoing light */
  vec3 out_radiance = (M_TAU / capture_info_buf.sample_count) * bsdf * in_radiance * abs(NL);

  SurfelRadiance surfel_radiance_indirect = surfel.radiance_indirect[radiance_dst];

  bool front_facing = (NL > 0.0);
  if (front_facing) {
    /* Store radiance normalized for spherical harmonic accumulation and for visualization. */
    surfel_radiance_indirect.front.rgb *= surfel_radiance_indirect.front.w;
    surfel_radiance_indirect.front += vec4(out_radiance * surfel.albedo_front,
                                           1.0 / capture_info_buf.sample_count);
    surfel_radiance_indirect.front.rgb /= surfel_radiance_indirect.front.w;
  }
  else {
    /* Store radiance normalized for spherical harmonic accumulation and for visualization. */
    surfel_radiance_indirect.back.rgb *= surfel_radiance_indirect.back.w;
    surfel_radiance_indirect.back += vec4(out_radiance * surfel.albedo_back,
                                          1.0 / capture_info_buf.sample_count);
    surfel_radiance_indirect.back.rgb /= surfel_radiance_indirect.back.w;
  }

  surfel.radiance_indirect[radiance_dst] = surfel_radiance_indirect;
}

void radiance_transfer_surfel(inout Surfel receiver, Surfel sender)
{
  vec3 L = safe_normalize(sender.position - receiver.position);
  bool front_facing = dot(-L, sender.normal) > 0.0;

  vec3 radiance;
  SurfelRadiance sender_radiance_indirect = sender.radiance_indirect[radiance_src];
  if (front_facing) {
    radiance = sender.radiance_direct.front.rgb;
    radiance += sender_radiance_indirect.front.rgb * sender_radiance_indirect.front.w;
  }
  else {
    radiance = sender.radiance_direct.back.rgb;
    radiance += sender_radiance_indirect.back.rgb * sender_radiance_indirect.back.w;
  }

  radiance_transfer(receiver, radiance, L);
}

vec3 radiance_sky_sample(vec3 R)
{
  return reflection_probes_world_sample(R, 0.0).rgb;
}

void radiance_transfer_world(inout Surfel receiver, vec3 sky_L)
{
  vec3 radiance = radiance_sky_sample(-sky_L);
  radiance_transfer(receiver, radiance, -sky_L);
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
    radiance_transfer_world(surfel, sky_L);
  }

  if (surfel.prev > -1) {
    Surfel surfel_prev = surfel_buf[surfel.prev];
    radiance_transfer_surfel(surfel, surfel_prev);
  }
  else {
    radiance_transfer_world(surfel, -sky_L);
  }

  surfel_buf[surfel_index] = surfel;
}
