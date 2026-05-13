/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_math_geom_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"
#include "gpu_shader_ray_lib.glsl"

#if 0
/* Screen-space ray ([0..1] "uv" range) where direction is normalize to be as small as one
 * full-resolution pixel. The ray is also clipped to all frustum sides.
 * Z component is device normalized Z (aka. depth buffer value).
 * W component is device normalized Z + Thickness.
 */
struct ScreenSpaceRay {
  float4 origin;
  float4 direction;
  float max_time;

};
#endif

/* Screen-space ray ([0..1] "uv" range) where direction is normalize to be as small as one
 * full-resolution pixel. The ray is also clipped to all frustum sides.
 * Z component is device normalized Z (aka. depth buffer value).
 * W component is device normalized Z + Thickness.
 */
struct ScreenSpaceRay {
  float4 origin;
  float4 direction;
  float max_time;

  static ScreenSpaceRay from_start_end(float4 hs_start, float4 hs_end, float2 pixel_size)
  {
    /* Constant bias (due to depth buffer precision). Helps with self intersection. */
    /* Magic numbers for 24bits of precision.
     * From http://terathon.com/gdc07_lengyel.pdf (slide 26) */
    constexpr float bias = -2.4e-7f * 2.0f;
    hs_start.z += bias;
    hs_end.z += bias;

    hs_start.xyz /= hs_start.w;
    hs_end.xyz /= hs_end.w;

    ScreenSpaceRay ray;
    ray.direction = hs_end - hs_start;
    ray.origin = hs_start;
    /* If the line is degenerate, make it cover at least one pixel
     * to not have to handle zero-pixel extent as a special case later */
    if (length_squared(ray.direction.xy) < 0.00001f) {
      ray.direction.xy = float2(0.0f, 0.00001f);
    }
    float ray_len_sqr = length_squared(ray.direction.xyz);
    /* Make direction cover one pixel. */
    bool is_more_vertical = abs(ray.direction.x / pixel_size.x) <
                            abs(ray.direction.y / pixel_size.y);
    ray.direction /= (is_more_vertical) ? abs(ray.direction.y) : abs(ray.direction.x);
    ray.direction *= (is_more_vertical) ? pixel_size.y : pixel_size.x;
    /* Clip to segment's end. */
    ray.max_time = sqrt(ray_len_sqr * safe_rcp(length_squared(ray.direction.xyz)));
    /* Clipping to frustum sides. */
    float clip_dist = line_unit_box_intersect_dist_safe(ray.origin.xyz, ray.direction.xyz);
    ray.max_time = min(ray.max_time, clip_dist);
    /* Convert to texture coords [0..1] range. */
    ray.origin.xyz = ray.origin.xyz * 0.5f + 0.5f;
    ray.direction.xyz *= 0.5f;
    return ray;
  }

  static ScreenSpaceRay create(Ray ray, float2 pixel_size)
  {
    float4 start = drw_point_view_to_homogenous(ray.origin);
    float4 end = drw_point_view_to_homogenous(ray.origin + ray.direction * ray.max_time);

    return ScreenSpaceRay::from_start_end(start, end, pixel_size);
  }

  static ScreenSpaceRay create(Ray ray, float4x4 winmat, float2 pixel_size)
  {
    float4 start = winmat * float4(ray.origin, 1.0f);
    float4 end = winmat * float4(ray.origin + ray.direction * ray.max_time, 1.0f);

    return ScreenSpaceRay::from_start_end(start, end, pixel_size);
  }

  float3 screen_position_at(float t)
  {
    return origin.xyz + direction.xyz * t;
  }
};

/* Estimates the thickness of an occluder pixel along a screen ray. */
struct ScreenThicknessEstimator {
  /* Z slope of the N previous steps (n-1, n-2). */
  float2 prev_ss_z_slope;
  /* Depth buffer value of the previous sample. */
  float prev_ss_z;
  /* Ray T of the previous sample. */
  float prev_t;

  static ScreenThicknessEstimator init(float start_z)
  {
    ScreenThicknessEstimator estimator;
    estimator.prev_ss_z_slope = float2(0.0f);
    estimator.prev_ss_z = start_z;
    estimator.prev_t = 0.0f;
    return estimator;
  }

  /**
   * Return the screen thickness (in depth buffer unit) of a sample and update the internal state.
   *
   * \param sample_ss_z: The depth buffer sample.
   * \param sample_ss_t: The screen space ray t in pixel.
   * \param sample_min_thickness: The minimum thickness to consider this sample. Ideally, it should
   * be equal to the radius of the pixel at the sample depth.
   */
  float thickness(float sample_ss_z, float sample_ss_t, float sample_min_thickness)
  {
    float delta_t = sample_ss_t - prev_t;
    /* Slope is negative if getting closer to the camera and positive if going away. */
    float delta_z = sample_ss_z - prev_ss_z;
    float slope = delta_z / delta_t;
    /* Treat this sample as a segment between the previous sample and this one.
     * This avoids loosing the surface when ray is parallel to the view plane. */
    float thickness = abs(delta_z);
    /* Treat abrupt change of slope as different objects.
     * Increasing this number reduces the porosity of surfaces almost parallel to the view but
     * might introduce connection between very thin objects and their background. */
    if (any(greaterThan(abs(prev_ss_z_slope - slope), float2(abs(prev_ss_z_slope))))) {
      thickness = 0.0f; /* Disconnect from previous sample. */
    }

    /* Minimum thickness. */
    thickness = max(thickness, sample_min_thickness);
    /* Update state. */
    prev_ss_z_slope.y = prev_ss_z_slope.x;
    prev_ss_z_slope.x = slope;
    prev_t = sample_ss_t;
    prev_ss_z = sample_ss_z;
    return thickness;
  }

  /**
   * Check ray-sample intersection and update the internal state.
   *
   * \param sample_ss_z: The depth buffer sample.
   * \param sample_ss_t: The screen space ray t in pixel.
   * \param sample_min_thickness: The minimum thickness to consider this sample. Ideally, it should
   * be equal to the radius of the pixel at the sample depth.
   * \param ray_ss_z: The ray Z depth in depth buffer unit.
   * \param ray_ss_z_prev: The ray Z depth in depth buffer unit of the previous iteration.
   */
  bool intersect(float sample_ss_z,
                 float sample_ss_t,
                 float sample_min_thickness,
                 float ray_ss_z,
                 float ray_ss_z_prev)
  {
    float sample_thickness = thickness(sample_ss_z, sample_ss_t, sample_min_thickness);
    /* We want to test the intersection between the surface estimated AABB and the ray step AABB.
     * This is equivalent to adding the step delta to the surface thickness and doing an AABB vs
     * point test. */
    sample_thickness += abs(ray_ss_z - ray_ss_z_prev);

    float sample_min = sample_ss_z;
    float sample_max = sample_ss_z + sample_thickness;
    return ray_ss_z >= sample_min && ray_ss_z <= sample_max;
  }
};
