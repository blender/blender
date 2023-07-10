
/**
 * For every irradiance probe sample, compute the incomming radiance from both side.
 * This is the same as the surfel ray but we do not actually transport the light, we only capture
 * the irradiance as spherical harmonic coefficients.
 *
 * Dispatched as 1 thread per irradiance probe sample.
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surfel_list_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_reflection_probe_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)

void irradiance_capture(vec3 L, vec3 irradiance, inout SphericalHarmonicL1 sh)
{
  vec3 lL = transform_direction(capture_info_buf.irradiance_grid_world_to_local_rotation, L);

  /* Spherical harmonics need to be weighted by sphere area. */
  irradiance *= 4.0 * M_PI;

  spherical_harmonics_encode_signal_sample(lL, vec4(irradiance, 1.0), sh);
}

void irradiance_capture(Surfel surfel, vec3 P, inout SphericalHarmonicL1 sh)
{
  vec3 L = safe_normalize(surfel.position - P);
  bool facing = dot(-L, surfel.normal) > 0.0;
  SurfelRadiance surfel_radiance_indirect = surfel.radiance_indirect[radiance_src];

  vec3 irradiance = vec3(0.0);
  irradiance += facing ? surfel.radiance_direct.front.rgb : surfel.radiance_direct.back.rgb;
  /* NOTE: The indirect radiance is already normalized and this is wanted, because we are not
   * integrating the same signal and we would have the SH lagging behind the surfel integration
   * otherwise. */
  irradiance += facing ? surfel_radiance_indirect.front.rgb : surfel_radiance_indirect.back.rgb;

  irradiance_capture(L, irradiance, sh);
}

vec3 irradiance_sky_sample(vec3 R)
{
  return reflection_probes_world_sample(R, 0.0).rgb;
}

void main()
{
  ivec3 grid_coord = ivec3(gl_GlobalInvocationID);

  if (any(greaterThanEqual(grid_coord, capture_info_buf.irradiance_grid_size))) {
    return;
  }

  vec3 P = lightprobe_irradiance_grid_sample_position(
      capture_info_buf.irradiance_grid_local_to_world,
      capture_info_buf.irradiance_grid_size,
      grid_coord);

  /* Project to get ray linked list. */
  float irradiance_sample_ray_distance;
  int list_index = surfel_list_index_get(P, irradiance_sample_ray_distance);

  /* Walk the ray to get which surfels the irradiance sample is between. */
  int surfel_prev = -1;
  int surfel_next = list_start_buf[list_index];
  for (; surfel_next > -1; surfel_next = surfel_buf[surfel_next].next) {
    /* Reminder: List is sorted with highest value first. */
    if (surfel_buf[surfel_next].ray_distance < irradiance_sample_ray_distance) {
      break;
    }
    surfel_prev = surfel_next;
  }

  vec3 sky_L = cameraVec(P);

  SphericalHarmonicL1 sh;
  sh.L0.M0 = imageLoad(irradiance_L0_img, grid_coord);
  sh.L1.Mn1 = imageLoad(irradiance_L1_a_img, grid_coord);
  sh.L1.M0 = imageLoad(irradiance_L1_b_img, grid_coord);
  sh.L1.Mp1 = imageLoad(irradiance_L1_c_img, grid_coord);

  /* Un-normalize for accumulation. */
  float weight_captured = capture_info_buf.sample_index * 2.0;
  sh.L0.M0 *= weight_captured;
  sh.L1.Mn1 *= weight_captured;
  sh.L1.M0 *= weight_captured;
  sh.L1.Mp1 *= weight_captured;

  if (surfel_next > -1) {
    Surfel surfel = surfel_buf[surfel_next];
    irradiance_capture(surfel, P, sh);
  }
  else {
    vec3 world_radiance = irradiance_sky_sample(-sky_L);
    irradiance_capture(sky_L, world_radiance, sh);
  }

  if (surfel_prev > -1) {
    Surfel surfel = surfel_buf[surfel_prev];
    irradiance_capture(surfel, P, sh);
  }
  else {
    vec3 world_radiance = irradiance_sky_sample(sky_L);
    irradiance_capture(-sky_L, world_radiance, sh);
  }

  /* Normalize for storage. We accumulated 2 samples. */
  weight_captured += 2.0;
  sh.L0.M0 /= weight_captured;
  sh.L1.Mn1 /= weight_captured;
  sh.L1.M0 /= weight_captured;
  sh.L1.Mp1 /= weight_captured;

  imageStore(irradiance_L0_img, grid_coord, sh.L0.M0);
  imageStore(irradiance_L1_a_img, grid_coord, sh.L1.Mn1);
  imageStore(irradiance_L1_b_img, grid_coord, sh.L1.M0);
  imageStore(irradiance_L1_c_img, grid_coord, sh.L1.Mp1);
}
