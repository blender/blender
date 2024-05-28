/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Use screen space tracing against depth buffer to find intersection with the scene.
 */

#pragma BLENDER_REQUIRE(eevee_lightprobe_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_colorspace_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ray_types_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ray_trace_screen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_closure_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)

void main()
{
  const uint tile_size = RAYTRACE_GROUP_SIZE;
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  ivec2 texel = ivec2(gl_LocalInvocationID.xy + tile_coord * tile_size);

  /* Check whether texel is out of bounds for all cases, so we can utilize fast
   * texture functions and early exit if not. */
  if (any(greaterThanEqual(texel, imageSize(ray_data_img).xy)) || any(lessThan(texel, ivec2(0)))) {
    return;
  }

  vec4 ray_data_im = imageLoadFast(ray_data_img, texel);
  float ray_pdf_inv = ray_data_im.w;

  if (ray_pdf_inv < 0.0) {
    /* Ray destined to planar trace. */
    return;
  }

  if (ray_pdf_inv == 0.0) {
    /* Invalid ray or pixels without ray. Do not trace. */
    imageStoreFast(ray_time_img, texel, vec4(0.0));
    imageStoreFast(ray_radiance_img, texel, vec4(0.0));
    return;
  }

  ivec2 texel_fullres = texel * uniform_buf.raytrace.resolution_scale +
                        uniform_buf.raytrace.resolution_bias;

  uint gbuf_header = texelFetch(gbuf_header_tx, texel_fullres, 0).r;
  ClosureType closure_type = gbuffer_closure_type_get_by_bin(gbuf_header, closure_index);

  bool is_reflection = true;
  if ((closure_type == CLOSURE_BSDF_TRANSLUCENT_ID) ||
      (closure_type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID))
  {
    is_reflection = false;
  }

  float depth = texelFetch(depth_tx, texel_fullres, 0).r;
  vec2 uv = (vec2(texel_fullres) + 0.5) * uniform_buf.raytrace.full_resolution_inv;

  vec3 P = drw_point_screen_to_world(vec3(uv, depth));
  vec3 V = drw_world_incident_vector(P);
  Ray ray;
  ray.origin = P;
  ray.direction = ray_data_im.xyz;

  /* Only closure 0 can be a transmission closure. */
  if (closure_index == 0) {
    float thickness = gbuffer_read_thickness(gbuf_header, gbuf_normal_tx, texel_fullres);
    if (thickness != 0.0) {
      ClosureUndetermined cl = gbuffer_read_bin(
          gbuf_header, gbuf_closure_tx, gbuf_normal_tx, texel_fullres, closure_index);
      ray = raytrace_thickness_ray_ammend(ray, cl, V, thickness);
    }
  }

  vec3 radiance = vec3(0.0);
  float noise_offset = sampling_rng_1D_get(SAMPLING_RAYTRACE_W);
  float rand_trace = interlieved_gradient_noise(vec2(texel), 5.0, noise_offset);

  ClosureUndetermined cl = gbuffer_read_bin(
      gbuf_header, gbuf_closure_tx, gbuf_normal_tx, texel_fullres, closure_index);
  float roughness = closure_apparent_roughness_get(cl);

  /* Transform the ray into view-space. */
  Ray ray_view;
  ray_view.origin = transform_point(drw_view.viewmat, ray.origin);
  ray_view.direction = transform_direction(drw_view.viewmat, ray.direction);
  /* Extend the ray to cover the whole view. */
  ray_view.max_time = 1000.0;

  ScreenTraceHitData hit;
  hit.valid = false;
  /* This huge branch is likely to be a huge issue for performance.
   * We could split the shader but that would mean to dispatch some area twice for the same closure
   * index. Another idea is to put both HiZ buffer int he same texture and dynamically access one
   * or the other. But that might also impact performance. */
  if (is_reflection) {
    hit = raytrace_screen(uniform_buf.raytrace,
                          uniform_buf.hiz,
                          hiz_front_tx,
                          rand_trace,
                          roughness,
                          true,  /* discard_backface */
                          false, /* allow_self_intersection */
                          ray_view);

    if (hit.valid) {
      vec3 hit_P = transform_point(drw_view.viewinv, hit.v_hit_P);
      /* TODO(@fclem): Split matrix multiply for precision. */
      vec3 history_ndc_hit_P = project_point(uniform_buf.raytrace.radiance_persmat, hit_P);
      vec3 history_ss_hit_P = history_ndc_hit_P * 0.5 + 0.5;
      /* Fetch radiance at hit-point. */
      radiance = textureLod(radiance_front_tx, history_ss_hit_P.xy, 0.0).rgb;
    }
  }
  else if (trace_refraction) {
    hit = raytrace_screen(uniform_buf.raytrace,
                          uniform_buf.hiz,
                          hiz_back_tx,
                          rand_trace,
                          roughness,
                          false, /* discard_backface */
                          true,  /* allow_self_intersection */
                          ray_view);

    if (hit.valid) {
      radiance = textureLod(radiance_back_tx, hit.ss_hit_P.xy, 0.0).rgb;
    }
  }

  if (!hit.valid) {
    /* Using ray direction as geometric normal to bias the sampling position.
     * This is faster than loading the gbuffer again and averages between reflected and normal
     * direction over many rays. */
    vec3 Ng = ray.direction;
    /* Fallback to nearest light-probe. */
    LightProbeSample samp = lightprobe_load(ray.origin, Ng, V);
    /* Clamp SH to have parity with forward evaluation. */
    float clamp_indirect = uniform_buf.clamp.surface_indirect;
    samp.volume_irradiance = spherical_harmonics_clamp(samp.volume_irradiance, clamp_indirect);

    radiance = lightprobe_eval_direction(samp, ray.origin, ray.direction, safe_rcp(ray_pdf_inv));
    /* Set point really far for correct reprojection of background. */
    hit.time = 10000.0;
  }

  radiance = colorspace_brightness_clamp_max(radiance, uniform_buf.clamp.surface_indirect);

  imageStoreFast(ray_time_img, texel, vec4(hit.time));
  imageStoreFast(ray_radiance_img, texel, vec4(radiance, 0.0));
}
