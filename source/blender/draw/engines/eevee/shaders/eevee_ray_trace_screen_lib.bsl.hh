/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Screen-space ray-tracing routine.
 *
 * Based on "Efficient GPU Screen-Space Ray Tracing"
 * by Morgan McGuire & Michael Mara
 * https://jcgt.org/published/0003/04/04/paper.pdf
 *
 * Many modifications were made for our own usage.
 */

#include "eevee_bxdf.bsl.hh"
#include "eevee_lightprobe_shared.hh"
#include "eevee_ray_types_lib.bsl.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_thickness_lib.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_fast_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"

/* Inputs expected to be in view-space. */
void raytrace_clip_ray_to_near_plane(ViewMatrices view, Ray &ray)
{
  float near_dist = view.near();
  if ((ray.origin.z + ray.direction.z * ray.max_time) > near_dist) {
    ray.max_time = abs((near_dist - ray.origin.z) / ray.direction.z);
  }
}

struct ScreenTraceHitData {
  /* Screen space hit position [0..1]. Last component is the ray depth, not the occluder depth. */
  float3 ss_hit_P;
  /* View space hit position. */
  float3 v_hit_P;
  /* Tracing time in world space. */
  float time;
  /* True if there was a valid intersection. False if went out of screen without intersection. */
  bool valid;
  /* True if ray was under a surface. */
  bool hit_backface;
};

/**
 * Ray-trace against the given HIZ-buffer height-field.
 *
 * \param stride_rand: Random number in [0..1] range. Offset along the ray to avoid banding
 *                     artifact when steps are too large.
 * \param roughness: Determine how lower depth mipmaps are used to make the tracing faster. Lower
 *                   roughness will use lower mipmaps.
 * \param allow_self_intersection: If false, ray-trace will return false if the ray is not covering
 *                                 at least one pixel.
 * \param ray: View-space ray. Direction pre-multiplied by maximum length.
 *
 * \return True if there is a valid intersection.
 */
ScreenTraceHitData raytrace_screen(ViewMatrices view,
                                   RayTraceData rt_data,
                                   HiZData hiz_data,
                                   sampler2D hiz_tx,
                                   float stride_rand,
                                   float roughness,
                                   const bool allow_self_intersection,
                                   Ray ray)
{
  /* Clip to near plane for perspective view where there is a singularity at the camera origin. */
  if (view.winmat[3][3] == 0.0f) {
    raytrace_clip_ray_to_near_plane(view, ray);
  }

  /* NOTE: The 2.0 factor here is because we are applying it in NDC space. */
  ScreenSpaceRay ssray = ScreenSpaceRay::create(view, ray, 2.0f * rt_data.full_resolution_inv);

  /* Avoid no iteration. */
  if (!allow_self_intersection && ssray.max_time < 1.1f) {
    /* Still output the clipped ray. */
    float3 hit_ssP = ssray.origin.xyz + ssray.direction.xyz * ssray.max_time;
    float3 hit_P = view.point_screen_to_world(float3(hit_ssP.xy, saturate(hit_ssP.z)));
    ray.direction = hit_P - ray.origin;

    ScreenTraceHitData no_hit;
    no_hit.time = 0.0f;
    no_hit.valid = false;
    return no_hit;
  }

  ssray.max_time = max(1.1f, ssray.max_time);

  float prev_delta = 0.0f, prev_time = 0.0f;
  float depth_sample = view.depth_view_to_screen(ray.origin.z);
  float delta = depth_sample - ssray.origin.z;

  float lod_fac = saturate(sqrt_fast(roughness) * 2.0f - 0.4f);

  ScreenThicknessEstimator thickness_estimator = ScreenThicknessEstimator::init(depth_sample);
  float prev_ray_z = ssray.origin.z;

  /* Cross at least one pixel. */
  float t = 1.001f, time = 1.001f;
  bool hit = false;
  constexpr int max_steps = 255;
  for (int iter = 1; !hit && (time < ssray.max_time) && (iter < max_steps); iter++) {
    float stride = 1.0f + float(iter) * rt_data.quality;
    float lod = log2(stride) * lod_fac;

    prev_time = time;
    prev_delta = delta;

    time = min(t + stride * stride_rand, ssray.max_time);
    t += stride;

    float3 ss_ray_P = ssray.screen_position_at(time);
    depth_sample = textureLod(hiz_tx, ss_ray_P.xy * hiz_data.uv_scale, floor(lod)).r;

    float sample_view_Z = view.depth_screen_to_view(depth_sample);
    float sample_ndc_min_thickness = rt_data.ray_thickness.pixel_depth_thickness_at(sample_view_Z);

    delta = depth_sample - ss_ray_P.z;
    hit = thickness_estimator.intersect(
        depth_sample, time, sample_ndc_min_thickness, ss_ray_P.z, prev_ray_z);
    prev_ray_z = ss_ray_P.z;
  }
  /* Reject hit if background. */
  hit = hit && (depth_sample != 1.0f);
  /* Refine hit using intersection between the sampled height-field and the ray.
   * This simplifies nicely to this single line. */
  time = mix(prev_time, time, saturate(prev_delta / (prev_delta - delta)));

  ScreenTraceHitData result;
  /* We can only hit a backface if the ray was under the surface.
   * Fast math optimizations can lead to deltas slightly below zero. */
  const float backface_epsilon = -1e-7f;
  result.hit_backface = prev_delta < backface_epsilon;
  result.ss_hit_P = ssray.origin.xyz + ssray.direction.xyz * time;
  result.v_hit_P = view.point_screen_to_view(result.ss_hit_P);
  /* Convert to world space ray time. */
  result.time = length(result.v_hit_P - ray.origin) / length(ray.direction);
  /* Update the validity as ss_hit_P can point to a background sample. */
  result.valid = hit &&
                 (textureLod(hiz_tx, result.ss_hit_P.xy * hiz_data.uv_scale, 0.0f).r != 1.0f);

  return result;
}

ScreenTraceHitData raytrace_planar(ViewMatrices view,
                                   RayTraceData rt_data,
                                   sampler2DArrayDepth planar_depth_tx,
                                   PlanarProbeData planar,
                                   float stride_rand,
                                   Ray ray)
{
  /* Clip to near plane for perspective view where there is a singularity at the camera origin. */
  if (view.winmat[3][3] == 0.0f) {
    raytrace_clip_ray_to_near_plane(view, ray);
  }

  float2 inv_texture_size = 1.0f / float2(textureSize(planar_depth_tx, 0).xy);
  /* NOTE: The 2.0 factor here is because we are applying it in NDC space. */
  ScreenSpaceRay ssray = ScreenSpaceRay::create(ray, planar.winmat, 2.0f * inv_texture_size);

  float prev_delta = 0.0f, prev_time = 0.0f;
  float depth_sample = reverse_z::read(
      texture(planar_depth_tx, float3(ssray.origin.xy, planar.layer_id)).r);
  float delta = depth_sample - ssray.origin.z;

  float t = 0.0f, time = 0.0f;
  bool hit = false;
  constexpr int max_steps = 32;
  for (int iter = 1; !hit && (time < ssray.max_time) && (iter < max_steps); iter++) {
    float stride = 1.0f + float(iter) * rt_data.quality;

    prev_time = time;
    prev_delta = delta;

    time = min(t + stride * stride_rand, ssray.max_time);
    t += stride;

    float4 ss_ray = ssray.origin + ssray.direction * time;

    depth_sample = reverse_z::read(texture(planar_depth_tx, float3(ss_ray.xy, planar.layer_id)).r);

    delta = depth_sample - ss_ray.z;
    /* Check if the ray is below the surface. */
    hit = (delta < 0.0f);
  }
  /* Reject hit if background. */
  hit = hit && (depth_sample != 1.0f);
  /* Refine hit using intersection between the sampled height-field and the ray.
   * This simplifies nicely to this single line. */
  time = mix(prev_time, time, saturate(prev_delta / (prev_delta - delta)));

  ScreenTraceHitData result;
  result.ss_hit_P = ssray.origin.xyz + ssray.direction.xyz * time;
  /* Update the validity as ss_hit_P can point to a not loaded sample. */
  result.valid =
      hit &&
      textureLod(planar_depth_tx, float3(result.ss_hit_P.xy, planar.layer_id), 0.0f).r != 0.0;

  /* NOTE: v_hit_P is in planar reflected view space. */
  result.v_hit_P = project_point(planar.wininv, view.screen_to_ndc(result.ss_hit_P));
  /* Convert to world space ray time. */
  result.time = length(result.v_hit_P - ray.origin) / length(ray.direction);
  return result;
}

/* Modify the ray origin before tracing it. We must do this because ray origin is implicitly
 * reconstructed from gbuffer depth which we cannot modify. */
Ray raytrace_thickness_ray_amend(Ray ray, ClosureUndetermined cl, float3 V, Thickness thickness)
{
  switch (cl.type) {
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      return bxdf_ggx_ray_amend_transmission(cl, V, ray, thickness);
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return bxdf_translucent_ray_amend(cl, V, ray, thickness);
    case CLOSURE_NONE_ID:
    case CLOSURE_BSDF_DIFFUSE_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID:
      break;
  }
  return ray;
}

bool clip_ray(float3 &start,
              float3 &end,
              const float3 direction,
              const float max_distance,
              const float4 frustum_planes[6])
{
  float min_t = 0.0f;
  float max_t = max_distance;

  for (int i = 0; i < 6; i++) {
    /* Make normals point outwards.
     * This way xyz * w represents a point in the plane surface. */
    const float3 plane_normal = -frustum_planes[i].xyz;
    const float plane_distance = frustum_planes[i].w;

    const float NoR = dot(plane_normal, direction);
    const float NoS = dot(plane_normal, start);

    if (abs(NoR) < 1e-6f) {
      /* Parallel ray. */
      if (NoS > plane_distance) {
        /* Fully outside the Frustum. */
        return false;
      }
      continue;
    }

    const float plane_t = (plane_distance - NoS) / NoR;

    if (NoR > 0.0f) {
      /* Ray going outside. */
      max_t = min(max_t, plane_t);
    }
    else {
      /* Ray going inside. */
      min_t = max(min_t, plane_t);
    }
  }

  end = start + direction * max_t;
  start = start + direction * min_t;

  return max_t > min_t;
}

/*
 * Similar to `raytrace_screen`, but modified to fit the needs of the Ray-cast node:
 * - Improves the support for rays parallel or nearly parallel to the incoming direction.
 * - Supports discarding hits against other objects.
 * - Traverses every single pixel between start and end, unless the number of steps required is
 *   greater than max_steps, in that case the steps are evenly distributed across the full
 *   distance.
 * Expects vs_origin and vs_end to be already clipped to the view frustum (see clip_ray above).
 * Returns the hit distance, or -1 if no hit was found.
 */
float raytrace_screen_2(ViewMatrices view,
                        const float3 vs_origin,
                        const float3 vs_end,
                        const float3 vs_direction,
                        sampler2D depth_tx,
                        const RayTraceData rt_data,
                        const int max_steps,
                        const float jitter,
                        usampler2D ob_id_tx,
                        const uint object_id,
                        float2 &r_hit_uv)
{
  /* Convert ray start and end into NDC for correct interpolation. */
  float3 start, end;
  start.xyz = view.point_view_to_screen(vs_origin);
  end.xyz = view.point_view_to_screen(vs_end);

  const float2 extent = float2(textureSize(ob_id_tx, 0).xy);
  const float2 texel_to_uv = (float2(1.0f) / extent);

  const float2 total_pixel_delta = abs(start.xy - end.xy) * extent;
  /* Number of steps required to trace a fully contiguous line. */
  int steps = int(max(total_pixel_delta.x, total_pixel_delta.y)) + 1;
  /* Limit to max steps. */
  steps = min(steps, max_steps);

  /* Per-step delta. */
  const float3 delta = (end - start) / float(steps);

  const float max_t = max(steps - 1, 1);
  float previous_step_z = start.z;

  ScreenThicknessEstimator thickness_estimator = ScreenThicknessEstimator::init(start.z);

  /* Skip the first step to avoid self-occlusion. But iterate at least once. */
  for (int i = 1; i < steps || i == 1; i++) {
    /* Ensure we don't go past ray end. */
    const float step_t = min(float(i) + jitter, max_t);
    const float3 step = start + delta * step_t;

    const float2 texel = step.xy * extent;
    if (object_id != 0 && object_id != texelFetch(ob_id_tx, int2(texel), 0).r) {
      thickness_estimator.thickness(1.0f, step_t, 0.0f);
      previous_step_z = step.z;
      continue;
    }

    /* Trick to prevent depth aliasing,
     * from "Rendering Tiny Glades With Entirely Too Much Ray Marching":
     * - Fetch depth using both point and linear sampling.
     * - Use the furthest one for intersection check.
     * - Use the closest one for thickness check. */
    const float2 gather_uv = round(texel) * texel_to_uv;
    const float2 bilinear_coords = fract(texel - 0.5f);
    const float4 depth4 = reverse_z::read(textureGather(depth_tx, gather_uv));
    const float hit_depth_point = mix(mix(depth4.w, depth4.z, bilinear_coords.x > 0.5f),
                                      mix(depth4.x, depth4.y, bilinear_coords.x > 0.5f),
                                      bilinear_coords.y > 0.5f);
    const float hit_depth_linear = mix(mix(depth4.w, depth4.z, bilinear_coords.x),
                                       mix(depth4.x, depth4.y, bilinear_coords.x),
                                       bilinear_coords.y);
    const float hit_min_z = min(hit_depth_point, hit_depth_linear);
    const float hit_max_z = max(hit_depth_point, hit_depth_linear);

    const float sample_view_Z = view.depth_screen_to_view(hit_depth_point);
    const float sample_ndc_min_thickness = rt_data.ray_thickness.pixel_depth_thickness_at(
        sample_view_Z);

    /* Equivalent to #thickness_estimator.intersect() but applies the Tiny Glade fix. */
    float sample_thickness = thickness_estimator.thickness(
        hit_depth_point, step_t, sample_ndc_min_thickness);
    /* We want to test the intersection between the surface estimated AABB and the ray step AABB.
     * This is equivalent to adding the step delta to the surface thickness and doing an AABB vs
     * point test. */
    sample_thickness += abs(step.z - previous_step_z);

    const float sample_min = hit_max_z;
    const float sample_max = hit_min_z + sample_thickness;
    const bool hit = step.z >= sample_min && step.z <= sample_max;

    previous_step_z = step.z;

    if (hit) {
      r_hit_uv = step.xy;
      /* We have a hit. Compute the distance. */
      const float3 vs_hit_point = view.point_screen_to_view(float3(step.xy, hit_depth_point));
      /* Hit point projection along the ray. */
      return dot(vs_hit_point - vs_origin, vs_direction);
    }
  }

  /* No hit was found. Return -1 to signal the failure. */
  return -1.0f;
}

/**
 * Sample the given full-screen frame-buffer at the given hit location.
 * Does a small contact aware blur on incoming radiance.
 */
float3 raytrace_sample_screen(ViewMatrices view,
                              sampler2D radiance_tx,
                              RayTraceData raytrace,
                              ScreenTraceHitData hit,
                              float roughness,
                              float2 ss_hit_P)
{
  /* We do not need a plausible cone value here.
   * Just compute some factor to avoid blurring contact points. */
  float shading_cone_tangent = saturate(roughness * 0.05);
  float vs_footprint = shading_cone_tangent * hit.time;
  /* Convert from view space cone to screen space. */
  float4 hs_hit_P = view.winmat * float4(hit.v_hit_P, 1.0f);
  float ndc_footprint = (vs_footprint * view.winmat[0][0]) / hs_hit_P.w;
  float pixel_footprint = ndc_footprint * raytrace.full_resolution.x;

  float3 radiance;
  /* Fetch radiance at hit-point. */
  if (pixel_footprint < 1.0f) {
    radiance = textureLod(radiance_tx, ss_hit_P, 0.0f).rgb;
  }
  else {
    float kernel_radius = saturate(pixel_footprint - 1.0f);
    float4 ofs = float2(kernel_radius, -kernel_radius).xxyy * raytrace.full_resolution_inv.xyxy;
    /* 4x4 box filter kernel for rough rays at the hit point.
     * Reduces variance of noisy reflected objects.
     * Use squared space to reduce fireflies at the cost of losing energy. */
    radiance = log2(1.0f + textureLod(radiance_tx, ss_hit_P + ofs.xy, 0.0f).rgb);
    radiance += log2(1.0f + textureLod(radiance_tx, ss_hit_P + ofs.xw, 0.0f).rgb);
    radiance += log2(1.0f + textureLod(radiance_tx, ss_hit_P + ofs.zy, 0.0f).rgb);
    radiance += log2(1.0f + textureLod(radiance_tx, ss_hit_P + ofs.zw, 0.0f).rgb);
    radiance *= 0.25f;
    radiance = exp2(radiance) - 1.0f;
  }
  return radiance;
}

/**
 * Sample the given full-screen frame-buffer at the given hit location.
 * Does a small contact aware blur on incoming radiance.
 */
float3 raytrace_sample_screen(ViewMatrices view,
                              sampler2DArray radiance_tx,
                              RayTraceData raytrace,
                              ScreenTraceHitData hit,
                              float roughness,
                              float2 ss_hit_P,
                              int layer)
{
  /* We do not need a plausible cone value here.
   * Just compute some factor to avoid blurring contact points. */
  float shading_cone_tangent = saturate(roughness * 0.05);
  float vs_footprint = shading_cone_tangent * hit.time;
  /* Convert from view space cone to screen space. */
  float4 hs_hit_P = view.winmat * float4(hit.v_hit_P, 1.0f);
  float ndc_footprint = (vs_footprint * view.winmat[0][0]) / hs_hit_P.w;
  float pixel_footprint = ndc_footprint * raytrace.full_resolution.x;

  float3 radiance;
  /* Fetch radiance at hit-point. */
  if (pixel_footprint < 1.0f) {
    radiance = textureLod(radiance_tx, float3(ss_hit_P, layer), 0.0f).rgb;
  }
  else {
    float kernel_radius = saturate(pixel_footprint - 1.0f);
    float4 ofs = float2(kernel_radius, -kernel_radius).xxyy * raytrace.full_resolution_inv.xyxy;
    /* 4x4 box filter kernel for rough rays at the hit point.
     * Reduces variance of noisy reflected objects.
     * Use squared space to reduce fireflies at the cost of losing energy. */
    radiance = log2(1.0f + textureLod(radiance_tx, float3(ss_hit_P + ofs.xy, layer), 0.0f).rgb);
    radiance += log2(1.0f + textureLod(radiance_tx, float3(ss_hit_P + ofs.xw, layer), 0.0f).rgb);
    radiance += log2(1.0f + textureLod(radiance_tx, float3(ss_hit_P + ofs.zy, layer), 0.0f).rgb);
    radiance += log2(1.0f + textureLod(radiance_tx, float3(ss_hit_P + ofs.zw, layer), 0.0f).rgb);
    radiance *= 0.25f;
    radiance = exp2(radiance) - 1.0f;
  }
  return radiance;
}
