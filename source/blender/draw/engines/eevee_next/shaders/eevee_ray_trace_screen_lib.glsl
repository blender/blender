/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Screen-space raytracing routine.
 *
 * Based on "Efficient GPU Screen-Space Ray Tracing"
 * by Morgan McGuire & Michael Mara
 * https://jcgt.org/published/0003/04/04/paper.pdf
 *
 * Many modifications were made for our own usage.
 */

#pragma BLENDER_REQUIRE(eevee_ray_types_lib.glsl)

/* Inputs expected to be in view-space. */
void raytrace_clip_ray_to_near_plane(inout Ray ray)
{
  float near_dist = get_view_z_from_depth(0.0);
  if ((ray.origin.z + ray.direction.z) > near_dist) {
    ray.direction *= abs((near_dist - ray.origin.z) / ray.direction.z);
  }
}

/**
 * Raytrace against the given HIZ-buffer height-field.
 *
 * \param stride_rand: Random number in [0..1] range. Offset along the ray to avoid banding
 *                     artifact when steps are too large.
 * \param roughness: Determine how lower depth mipmaps are used to make the tracing faster. Lower
 *                   roughness will use lower mipmaps.
 * \param discard_backface: If true, raytrace will return false  if we hit a surface from behind.
 * \param allow_self_intersection: If false, raytrace will return false if the ray is not covering
 *                                 at least one pixel.
 * \param ray: View-space ray. Direction premultiplied by maximum length.
 *
 * \return True if there is a valid intersection.
 */
#ifdef METAL_AMD_RAYTRACE_WORKAROUND
__attribute__((noinline))
#endif
bool raytrace_screen(RayTraceData rt_data,
                     HiZData hiz_data,
                     sampler2D hiz_tx,
                     float stride_rand,
                     float roughness,
                     const bool discard_backface,
                     const bool allow_self_intersection,
                     inout Ray ray)
{
  /* Clip to near plane for perspective view where there is a singularity at the camera origin. */
  if (ProjectionMatrix[3][3] == 0.0) {
    raytrace_clip_ray_to_near_plane(ray);
  }

  /*  NOTE: The 2.0 factor here is because we are applying it in. */
  ScreenSpaceRay ssray = raytrace_screenspace_ray_create(
      ray, 2.0 * rt_data.full_resolution_inv, rt_data.thickness);

  /* Avoid no iteration. */
  if (!allow_self_intersection && ssray.max_time < 1.1) {
    /* Still output the clipped ray. */
    vec3 hit_ssP = ssray.origin.xyz + ssray.direction.xyz * ssray.max_time;
    vec3 hit_P = get_world_space_from_depth(hit_ssP.xy, saturate(hit_ssP.z));
    ray.direction = hit_P - ray.origin;
    return false;
  }

  ssray.max_time = max(1.1, ssray.max_time);

  float prev_delta = 0.0, prev_time = 0.0;
  float depth_sample = get_depth_from_view_z(ray.origin.z);
  float delta = depth_sample - ssray.origin.z;

  float lod_fac = saturate(fast_sqrt(roughness) * 2.0 - 0.4);

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
    /* For workaround, perform discard backface and background check only within
     * the iteration where the first successful ray intersection is registered.
     * We flag failures to discard ray hits later. */
    bool hit_valid = !(discard_backface && prev_delta < 0.0) && (depth_sample != 1.0);
    if (hit && !hit_valid) {
      hit_failsafe = false;
    }
#endif
  }
#ifndef METAL_AMD_RAYTRACE_WORKAROUND
  /* Discard backface hits. */
  hit = hit && !(discard_backface && prev_delta < 0.0);
  /* Reject hit if background. */
  hit = hit && (depth_sample != 1.0);
#endif
  /* Refine hit using intersection between the sampled height-field and the ray.
   * This simplifies nicely to this single line. */
  time = mix(prev_time, time, saturate(prev_delta / (prev_delta - delta)));

  vec3 hit_ssP = ssray.origin.xyz + ssray.direction.xyz * time;
  /* Set ray to where tracing ended. */
  vec3 hit_P = get_world_space_from_depth(hit_ssP.xy, saturate(hit_ssP.z));
  ray.direction = hit_P - ray.origin;

#ifdef METAL_AMD_RAYTRACE_WORKAROUND
  /* Check failed ray flag to discard bad hits. */
  if (!hit_failsafe) {
    return false;
  }
#endif
  return hit;
}
