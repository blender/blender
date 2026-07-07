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

#include "draw_view_lib.glsl"
#include "eevee_bxdf_diffuse_lib.glsl"
#include "eevee_bxdf_microfacet_lib.glsl"
#include "eevee_ray_types_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "eevee_thickness_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_fast_lib.glsl"

/* Inputs expected to be in view-space. */
void raytrace_clip_ray_to_near_plane(Ray &ray)
{
  float near_dist = drw_view_near();
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
};

/**
 * Ray-trace against the given HIZ-buffer height-field.
 *
 * \param stride_rand: Random number in [0..1] range. Offset along the ray to avoid banding
 *                     artifact when steps are too large.
 * \param roughness: Determine how lower depth mipmaps are used to make the tracing faster. Lower
 *                   roughness will use lower mipmaps.
 * \param discard_backface: If true, ray-trace will return false if we hit a surface from behind.
 * \param allow_self_intersection: If false, ray-trace will return false if the ray is not covering
 *                                 at least one pixel.
 * \param ray: View-space ray. Direction pre-multiplied by maximum length.
 *
 * \return True if there is a valid intersection.
 */
ScreenTraceHitData raytrace_screen(RayTraceData rt_data,
                                   HiZData hiz_data,
                                   sampler2D hiz_tx,
                                   float stride_rand,
                                   float roughness,
                                   const bool discard_backface,
                                   const bool allow_self_intersection,
                                   Ray ray)
{
  /* Clip to near plane for perspective view where there is a singularity at the camera origin. */
  if (drw_view().winmat[3][3] == 0.0f) {
    raytrace_clip_ray_to_near_plane(ray);
  }

  /* NOTE: The 2.0 factor here is because we are applying it in NDC space. */
  ScreenSpaceRay ssray = raytrace_screenspace_ray_create(
      ray, 2.0f * rt_data.full_resolution_inv, rt_data.thickness);

  /* Avoid no iteration. */
  if (!allow_self_intersection && ssray.max_time < 1.1f) {
    /* Still output the clipped ray. */
    float3 hit_ssP = ssray.origin.xyz + ssray.direction.xyz * ssray.max_time;
    float3 hit_P = drw_point_screen_to_world(float3(hit_ssP.xy, saturate(hit_ssP.z)));
    ray.direction = hit_P - ray.origin;

    ScreenTraceHitData no_hit;
    no_hit.time = 0.0f;
    no_hit.valid = false;
    return no_hit;
  }

  ssray.max_time = max(1.1f, ssray.max_time);

  float prev_delta = 0.0f, prev_time = 0.0f;
  float depth_sample = drw_depth_view_to_screen(ray.origin.z);
  float delta = depth_sample - ssray.origin.z;

  float lod_fac = saturate(sqrt_fast(roughness) * 2.0f - 0.4f);

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

    float4 ss_p = ssray.origin + ssray.direction * time;
    depth_sample = textureLod(hiz_tx, ss_p.xy * hiz_data.uv_scale, floor(lod)).r;

    delta = depth_sample - ss_p.z;
    /* Check if the ray is below the surface ... */
    hit = (delta < 0.0f);
    /* ... and above it with the added thickness. */
    hit = hit && (delta > ss_p.z - ss_p.w || abs(delta) < abs(ssray.direction.z * stride * 2.0f));
  }
  /* Discard back-face hits. */
  hit = hit && !(discard_backface && prev_delta < 0.0f);
  /* Reject hit if background. */
  hit = hit && (depth_sample != 1.0f);
  /* Refine hit using intersection between the sampled height-field and the ray.
   * This simplifies nicely to this single line. */
  time = mix(prev_time, time, saturate(prev_delta / (prev_delta - delta)));

  ScreenTraceHitData result;
  result.ss_hit_P = ssray.origin.xyz + ssray.direction.xyz * time;
  result.v_hit_P = drw_point_screen_to_view(result.ss_hit_P);
  /* Convert to world space ray time. */
  result.time = length(result.v_hit_P - ray.origin) / length(ray.direction);
  /* Update the validity as ss_hit_P can point to a background sample. */
  result.valid = hit &&
                 (textureLod(hiz_tx, result.ss_hit_P.xy * hiz_data.uv_scale, 0.0f).r != 1.0f);

  return result;
}

#ifdef PLANAR_PROBES

ScreenTraceHitData raytrace_planar(RayTraceData rt_data,
                                   sampler2DArrayDepth planar_depth_tx,
                                   PlanarProbeData planar,
                                   float stride_rand,
                                   Ray ray)
{
  /* Clip to near plane for perspective view where there is a singularity at the camera origin. */
  if (drw_view().winmat[3][3] == 0.0f) {
    raytrace_clip_ray_to_near_plane(ray);
  }

  float2 inv_texture_size = 1.0f / float2(textureSize(planar_depth_tx, 0).xy);
  /* NOTE: The 2.0 factor here is because we are applying it in NDC space. */
  ScreenSpaceRay ssray = raytrace_screenspace_ray_create(
      ray, planar.winmat, 2.0f * inv_texture_size);

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
  result.v_hit_P = project_point(planar.wininv, drw_screen_to_ndc(result.ss_hit_P));
  /* Convert to world space ray time. */
  result.time = length(result.v_hit_P - ray.origin) / length(ray.direction);
  return result;
}

#endif

/* Modify the ray origin before tracing it. We must do this because ray origin is implicitly
 * reconstructed from gbuffer depth which we cannot modify. */
Ray raytrace_thickness_ray_amend(Ray ray, ClosureUndetermined cl, float3 V, float thickness)
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
 * Similar to raytrace_screen, but modified to fit the needs of the Raycast node:
 * - Improves the support for rays parallel or nearly parallel to the incoming direction.
 * - Supports discarding hits against other objects.
 * - Traverses every single pixel between start and end, unless the number of steps required is
 *   greater than max_steps, in that case the steps are evenly distributed across the full
 *   distance.
 * Expects vs_origin and vs_end to be already clipped to the view frustum (see clip_ray above).
 * Returns the hit distance, or -1 if no hit was found.
 */
float raytrace_screen_2(const float3 vs_origin,
                        const float3 vs_end,
                        const float3 vs_direction,
                        sampler2D hiz_tx,
                        const float thickness,
                        const int max_steps,
                        const float jitter,
                        usampler2D ob_id_tx,
                        const uint object_id,
                        float2 &r_hit_uv)
{
  /* Convert ray start and end into NDC for correct interpolation. */
  float4 start, end;
  start.xyz = drw_point_view_to_screen(vs_origin);
  end.xyz = drw_point_view_to_screen(vs_end);
  /* W stores Z - thickness (Note that view space forward is -Z). */
  start.w = drw_depth_view_to_screen(min(vs_origin.z + thickness, drw_view_near()));
  end.w = drw_depth_view_to_screen(min(vs_end.z + thickness, drw_view_near()));

#if 0
  /* TODO: This should be the correct code but it currently fails when rendering probes.
   * (The values are always the ones from the main View) */
  const float2 extent = float2(uniform_buf.film.render_extent);
  const float2 hiz_uv_scale = uniform_buf.hiz.uv_scale;
#else
  const float2 extent = float2(textureSize(ob_id_tx, 0).xy);
  const float2 hiz_uv_scale = extent / float2(textureSize(hiz_tx, 0));
#endif
  const float2 hiz_texel_to_uv = (float2(1.0f) / extent) * hiz_uv_scale;

  const float2 total_pixel_delta = abs(start.xy - end.xy) * extent;
  /* Number of steps required to trace a fully contiguous line. */
  int steps = int(max(total_pixel_delta.x, total_pixel_delta.y)) + 1;
  /* Limit to max steps. */
  steps = min(steps, max_steps);

  /* Per-step delta. */
  const float4 delta = (end - start) / float(steps);

  const float max_t = max(steps - 1, 1);
  const bool forward = end.z > start.z;
  float previous_step_z = start.z;

  /* Skip the first step to avoid self-occlusion. But iterate at least once. */
  for (int i = 1; i < steps || i == 1; i++) {
    /* Ensure we don't go past ray end. */
    const float step_t = min(float(i) + jitter, max_t);
    const float4 step = start + delta * step_t;

    const float2 texel = step.xy * extent;
    if (object_id != 0 && object_id != texelFetch(ob_id_tx, int2(texel), 0).r) {
      previous_step_z = step.z;
      continue;
    }

    /* Trick to prevent depth aliasing,
     * from "Rendering Tiny Glades With Entirely Too Much Ray Marching":
     * - Fetch depth using both point and linear sampling.
     * - Use the furthest one for intersection check.
     * - Use the closest one for thickness check. */
    const float hit_depth_point = texelFetch(hiz_tx, int2(texel), 0).r;
    const float2 gather_uv = round(texel) * hiz_texel_to_uv;
    const float4 depth4 = textureGather(hiz_tx, gather_uv);
    const float2 bilinear_coords = fract(texel - 0.5f);
    const float hit_depth_linear = mix(mix(depth4.w, depth4.z, bilinear_coords.x),
                                       mix(depth4.x, depth4.y, bilinear_coords.x),
                                       bilinear_coords.y);
    const float hit_min_z = min(hit_depth_point, hit_depth_linear);
    const float hit_max_z = max(hit_depth_point, hit_depth_linear);

    /* Ensure the allowed depth range is not lower than the step delta. */
    const float min_z = forward ? min(step.w, previous_step_z) : step.w;
    const float max_z = forward ? step.z : max(step.z, previous_step_z);

    // previous_step_z = step.z;
    /* Using step.z is more "correct", but step.w mitigates missed hits against planes
     * with normals symmetrical to the ray direction. */
    previous_step_z = forward ? step.w : step.z;

    if (max_z >= hit_max_z && min_z <= hit_min_z) {
      r_hit_uv = step.xy;
      /* We have a hit. Compute the distance. */
      const float3 vs_hit_point = drw_point_screen_to_view(float3(step.xy, hit_depth_point));
      /* Hit point projection along the ray. */
      return dot(vs_hit_point - vs_origin, vs_direction);
    }
  }

  /* No hit was found. Return -1 to signal the failure. */
  return -1.0f;
}
