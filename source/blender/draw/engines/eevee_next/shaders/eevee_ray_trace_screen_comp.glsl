/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Use screen space tracing against depth buffer to find intersection with the scene.
 */

#pragma BLENDER_REQUIRE(eevee_reflection_probe_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ray_types_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ray_trace_screen_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)

void main()
{
  const uint tile_size = RAYTRACE_GROUP_SIZE;
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  ivec2 texel = ivec2(gl_LocalInvocationID.xy + tile_coord * tile_size);

  ivec2 texel_fullres = texel * uniform_buf.raytrace.resolution_scale +
                        uniform_buf.raytrace.resolution_bias;

  float depth = texelFetch(depth_tx, texel_fullres, 0).r;
  vec2 uv = (vec2(texel_fullres) + 0.5) * uniform_buf.raytrace.full_resolution_inv;

  vec4 ray_data = imageLoad(ray_data_img, texel);
  float ray_pdf_inv = ray_data.w;

  if (ray_pdf_inv == 0.0) {
    /* Invalid ray or pixels without ray. Do not trace. */
    imageStore(ray_time_img, texel, vec4(0.0));
    imageStore(ray_radiance_img, texel, vec4(0.0));
    return;
  }

  vec3 P = get_world_space_from_depth(uv, depth);
  Ray ray;
  ray.origin = P;
  ray.direction = ray_data.xyz;

  vec3 radiance = vec3(0.0);
  float noise_offset = sampling_rng_1D_get(SAMPLING_RAYTRACE_W);
  float rand_trace = interlieved_gradient_noise(vec2(texel), 5.0, noise_offset);

#if defined(RAYTRACE_REFLECT)
  const bool discard_backface = true;
  const bool allow_self_intersection = false;
#elif defined(RAYTRACE_REFRACT)
  const bool discard_backface = false;
  const bool allow_self_intersection = true;
#endif

  /* TODO(fclem): Take IOR into account in the roughness LOD bias. */
  /* TODO(fclem): pdf to roughness mapping is a crude approximation. Find something better. */
  float roughness = saturate(sample_pdf_uniform_hemisphere() / ray_pdf_inv);

  /* Transform the ray into view-space. */
  Ray ray_view;
  ray_view.origin = transform_point(drw_view.viewmat, ray.origin);
  ray_view.direction = transform_direction(drw_view.viewmat, ray.direction);
  /* Extend the ray to cover the whole view. */
  ray_view.max_time = 1000.0;

  ScreenTraceHitData hit = raytrace_screen(uniform_buf.raytrace,
                                           uniform_buf.hiz,
                                           hiz_tx,
                                           rand_trace,
                                           roughness,
                                           discard_backface,
                                           allow_self_intersection,
                                           ray_view);

  if (hit.valid) {
    vec3 hit_P = transform_point(drw_view.viewinv, hit.v_hit_P);
    /* TODO(fclem): Split matrix mult for precision. */
    vec3 history_ndc_hit_P = project_point(uniform_buf.raytrace.radiance_persmat, hit_P);
    vec3 history_ss_hit_P = history_ndc_hit_P * 0.5 + 0.5;
    /* Evaluate radiance at hit-point. */
    radiance = textureLod(screen_radiance_tx, history_ss_hit_P.xy, 0.0).rgb;

    /* Transmit twice if thickness is set and ray is longer than thickness. */
    // if (thickness > 0.0 && length(ray_data.xyz) > thickness) {
    //   ray_radiance.rgb *= color;
    // }
  }
  else {
    /* Fallback to nearest light-probe. */
    int closest_probe_id = reflection_probes_find_closest(P);
    ReflectionProbeData probe = reflection_probe_buf[closest_probe_id];
    radiance = reflection_probes_sample(ray.direction, 0.0, probe).rgb;
    /* Set point really far for correct reprojection of background. */
    hit.time = 10000.0;
  }

  float luma = max(1e-8, max_v3(radiance));
  radiance *= 1.0 - max(0.0, luma - uniform_buf.raytrace.brightness_clamp) / luma;

  imageStore(ray_time_img, texel, vec4(hit.time));
  imageStore(ray_radiance_img, texel, vec4(radiance, 0.0));
}
