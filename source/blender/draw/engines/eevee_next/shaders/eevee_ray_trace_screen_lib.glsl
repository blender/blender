/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Screen-space ray-tracing routine.
 *
 * Based on "Efficient GPU Screen-Space Ray Tracing"
 * by Morgan McGuire & Michael Mara
 * https://jcgt.org/published/0003/04/04/paper.pdf
 *
 * Many modifications were made for our own usage.
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_fast_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ray_types_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_thickness_lib.glsl)

/* Inputs expected to be in view-space. */
void raytrace_clip_ray_to_near_plane(inout Ray ray)
{
  float near_dist = drw_view_near();
  if ((ray.origin.z + ray.direction.z * ray.max_time) > near_dist) {
    ray.max_time = abs((near_dist - ray.origin.z) / ray.direction.z);
  }
}

#ifdef METAL_AMD_RAYTRACE_WORKAROUND
#  define METAL_ATTR __attribute__((noinline))
#else
#  define METAL_ATTR
#endif

struct ScreenTraceHitData {
  /* Screen space hit position [0..1]. Last component is the ray depth, not the occluder depth. */
  vec3 ss_hit_P;
  /* View space hit position. */
  vec3 v_hit_P;
  /* Tracing time in world space. */
  float time;
  /* True if there was a valid intersection. False if went out of screen without intersection. */
  bool valid;
};

/**
 * Ray-trace against the given HIZ-buffer height-field.
 *
 * \param stride_rand: Random number in [0..1] range. Offset along the ray to avoid banding
 *                     artifact when steps are too large.
 * \param roughness: Determine how lower depth mipmaps are used to make the tracing faster. Lower
 *                   roughness will use lower mipmaps.
 * \param discard_backface: If true, ray-trace will return false  if we hit a surface from behind.
 * \param allow_self_intersection: If false, ray-trace will return false if the ray is not covering
 *                                 at least one pixel.
 * \param ray: View-space ray. Direction pre-multiplied by maximum length.
 *
 * \return True if there is a valid intersection.
 */
METAL_ATTR ScreenTraceHitData raytrace_screen(RayTraceData rt_data,
                                              HiZData hiz_data,
                                              sampler2D hiz_tx,
                                              float stride_rand,
                                              float roughness,
                                              const bool discard_backface,
                                              const bool allow_self_intersection,
                                              Ray ray)
{
  /* Clip to near plane for perspective view where there is a singularity at the camera origin. */
  if (ProjectionMatrix[3][3] == 0.0) {
    raytrace_clip_ray_to_near_plane(ray);
  }

  /* NOTE: The 2.0 factor here is because we are applying it in NDC space. */
  ScreenSpaceRay ssray = raytrace_screenspace_ray_create(
      ray, 2.0 * rt_data.full_resolution_inv, rt_data.thickness);

  /* Avoid no iteration. */
  if (!allow_self_intersection && ssray.max_time < 1.1) {
    /* Still output the clipped ray. */
    vec3 hit_ssP = ssray.origin.xyz + ssray.direction.xyz * ssray.max_time;
    vec3 hit_P = drw_point_screen_to_world(vec3(hit_ssP.xy, saturate(hit_ssP.z)));
    ray.direction = hit_P - ray.origin;

    ScreenTraceHitData no_hit;
    no_hit.time = 0.0;
    no_hit.valid = false;
    return no_hit;
  }

  ssray.max_time = max(1.1, ssray.max_time);

  float prev_delta = 0.0, prev_time = 0.0;
  float depth_sample = drw_depth_view_to_screen(ray.origin.z);
  float delta = depth_sample - ssray.origin.z;

  float lod_fac = saturate(sqrt_fast(roughness) * 2.0 - 0.4);

  /* Cross at least one pixel. */
  float t = 1.001, time = 1.001;
  bool hit = false;
#ifdef METAL_AMD_RAYTRACE_WORKAROUND
  bool hit_failsafe = true;
#endif
  const int max_steps = 255;
  for (int iter = 1; !hit && (time < ssray.max_time) && (iter < max_steps); iter++) {
    float stride = 1.0 + float(iter) * rt_data.quality;
    float lod = log2(stride) * lod_fac;

    prev_time = time;
    prev_delta = delta;

    time = min(t + stride * stride_rand, ssray.max_time);
    t += stride;

    vec4 ss_p = ssray.origin + ssray.direction * time;
    depth_sample = textureLod(hiz_tx, ss_p.xy * hiz_data.uv_scale, floor(lod)).r;

    delta = depth_sample - ss_p.z;
    /* Check if the ray is below the surface ... */
    hit = (delta < 0.0);
    /* ... and above it with the added thickness. */
    hit = hit && (delta > ss_p.z - ss_p.w || abs(delta) < abs(ssray.direction.z * stride * 2.0));

#ifdef METAL_AMD_RAYTRACE_WORKAROUND
    /* For workaround, perform discard back-face and background check only within
     * the iteration where the first successful ray intersection is registered.
     * We flag failures to discard ray hits later. */
    bool hit_valid = !(discard_backface && prev_delta < 0.0) && (depth_sample != 1.0);
    if (hit && !hit_valid) {
      hit_failsafe = false;
    }
#endif
  }
#ifndef METAL_AMD_RAYTRACE_WORKAROUND
  /* Discard back-face hits. */
  hit = hit && !(discard_backface && prev_delta < 0.0);
  /* Reject hit if background. */
  hit = hit && (depth_sample != 1.0);
#endif
  /* Refine hit using intersection between the sampled height-field and the ray.
   * This simplifies nicely to this single line. */
  time = mix(prev_time, time, saturate(prev_delta / (prev_delta - delta)));

  ScreenTraceHitData result;
  result.valid = hit;
  result.ss_hit_P = ssray.origin.xyz + ssray.direction.xyz * time;
  result.v_hit_P = drw_point_screen_to_view(result.ss_hit_P);
  /* Convert to world space ray time. */
  result.time = length(result.v_hit_P - ray.origin) / length(ray.direction);

#ifdef METAL_AMD_RAYTRACE_WORKAROUND
  /* Check failed ray flag to discard bad hits. */
  if (!hit_failsafe) {
    result.valid = false;
  }
#endif
  return result;
}

#undef METAL_ATTR

#ifdef PLANAR_PROBES

ScreenTraceHitData raytrace_planar(RayTraceData rt_data,
                                   depth2DArray planar_depth_tx,
                                   PlanarProbeData planar,
                                   float stride_rand,
                                   Ray ray)
{
  /* Clip to near plane for perspective view where there is a singularity at the camera origin. */
  if (ProjectionMatrix[3][3] == 0.0) {
    raytrace_clip_ray_to_near_plane(ray);
  }

  vec2 inv_texture_size = 1.0 / vec2(textureSize(planar_depth_tx, 0).xy);
  /* NOTE: The 2.0 factor here is because we are applying it in NDC space. */
  /* TODO(@fclem): This uses the main view's projection matrix, not the planar's one.
   * This works fine for reflection, but this prevent the use of any other projection capture. */
  ScreenSpaceRay ssray = raytrace_screenspace_ray_create(ray, 2.0 * inv_texture_size);

  float prev_delta = 0.0, prev_time = 0.0;
  float depth_sample = texture(planar_depth_tx, vec3(ssray.origin.xy, planar.layer_id)).r;
  float delta = depth_sample - ssray.origin.z;

  float t = 0.0, time = 0.0;
  bool hit = false;
  const int max_steps = 32;
  for (int iter = 1; !hit && (time < ssray.max_time) && (iter < max_steps); iter++) {
    float stride = 1.0 + float(iter) * rt_data.quality;

    prev_time = time;
    prev_delta = delta;

    time = min(t + stride * stride_rand, ssray.max_time);
    t += stride;

    vec4 ss_ray = ssray.origin + ssray.direction * time;

    depth_sample = texture(planar_depth_tx, vec3(ss_ray.xy, planar.layer_id)).r;

    delta = depth_sample - ss_ray.z;
    /* Check if the ray is below the surface. */
    hit = (delta < 0.0);
  }
  /* Reject hit if background. */
  hit = hit && (depth_sample != 1.0);
  /* Refine hit using intersection between the sampled height-field and the ray.
   * This simplifies nicely to this single line. */
  time = mix(prev_time, time, saturate(prev_delta / (prev_delta - delta)));

  ScreenTraceHitData result;
  result.valid = hit;
  result.ss_hit_P = ssray.origin.xyz + ssray.direction.xyz * time;
  /* TODO(@fclem): This uses the main view's projection matrix, not the planar's one.
   * This works fine for reflection, but this prevent the use of any other projection capture. */
  result.v_hit_P = drw_point_screen_to_view(result.ss_hit_P);
  /* Convert to world space ray time. */
  result.time = length(result.v_hit_P - ray.origin) / length(ray.direction);
  return result;
}

#endif

/* Modify the ray origin before tracing it. We must do this because ray origin is implicitly
 * reconstructed from from gbuffer depth which we cannot modify. */
Ray raytrace_thickness_ray_ammend(
    Ray ray, ClosureUndetermined cl, vec3 V, vec3 surface_N, float thickness)
{
  switch (cl.type) {
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID: {
      ClosureRefraction cl_refr = to_closure_refraction(cl);
      float apparent_roughness = refraction_roughness_remapping(cl_refr.roughness, cl_refr.ior);
      vec3 L = refraction_dominant_dir(cl_refr.N, V, cl_refr.ior, apparent_roughness);
      /* The ray direction was generated using the same 2 transmission events assumption.
       * Only change its origin. Skip the volume inside the object. */
      ray.origin += thickness_sphere_intersect(thickness, surface_N, L).hit_P;
      break;
    }
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      /* Ray direction is distributed on the whole sphere.
       * Move the ray origin to the sphere surface (with bias to avoid self-intersection). */
      ray.origin += (ray.direction - surface_N) * thickness * 0.505;
      break;
    default:
      break;
  }
  return ray;
}
