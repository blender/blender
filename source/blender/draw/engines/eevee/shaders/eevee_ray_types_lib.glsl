/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_math_geom_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"
#include "gpu_shader_ray_lib.glsl"

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

void raytrace_screenspace_ray_finalize(inout ScreenSpaceRay ray, float2 pixel_size)

{
  /* Constant bias (due to depth buffer precision). Helps with self intersection. */
  /* Magic numbers for 24bits of precision.
   * From http://terathon.com/gdc07_lengyel.pdf (slide 26) */
  constexpr float bias = -2.4e-7f * 2.0f;
  ray.origin.zw += bias;
  ray.direction.zw += bias;

  ray.direction -= ray.origin;
  /* If the line is degenerate, make it cover at least one pixel
   * to not have to handle zero-pixel extent as a special case later */
  if (length_squared(ray.direction.xy) < 0.00001f) {
    ray.direction.xy = float2(0.0f, 0.00001f);
  }
  float ray_len_sqr = length_squared(ray.direction.xyz);
  /* Make ray.direction cover one pixel. */
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
  ray.origin = ray.origin * 0.5f + 0.5f;
  ray.direction *= 0.5f;
}

ScreenSpaceRay raytrace_screenspace_ray_create(Ray ray, float2 pixel_size)
{
  ScreenSpaceRay ssray;
  ssray.origin.xyz = drw_point_view_to_ndc(ray.origin);
  ssray.direction.xyz = drw_point_view_to_ndc(ray.origin + ray.direction * ray.max_time);

  raytrace_screenspace_ray_finalize(ssray, pixel_size);
  return ssray;
}

ScreenSpaceRay raytrace_screenspace_ray_create(Ray ray, float4x4 winmat, float2 pixel_size)
{
  ScreenSpaceRay ssray;
  ssray.origin.xyz = project_point(winmat, ray.origin);
  ssray.direction.xyz = project_point(winmat, ray.origin + ray.direction * ray.max_time);

  raytrace_screenspace_ray_finalize(ssray, pixel_size);
  return ssray;
}

ScreenSpaceRay raytrace_screenspace_ray_create(Ray ray, float2 pixel_size, float thickness)
{
  ScreenSpaceRay ssray;
  ssray.origin.xyz = drw_point_view_to_ndc(ray.origin);
  ssray.direction.xyz = drw_point_view_to_ndc(ray.origin + ray.direction * ray.max_time);
  /* Interpolate thickness in screen space.
   * Calculate thickness further away to avoid near plane clipping issues. */
  ssray.origin.w = drw_depth_view_to_screen(ray.origin.z - thickness);
  ssray.direction.w = drw_depth_view_to_screen(ray.origin.z + ray.direction.z - thickness);
  ssray.origin.w = ssray.origin.w * 2.0f - 1.0f;
  ssray.direction.w = ssray.direction.w * 2.0f - 1.0f;

  raytrace_screenspace_ray_finalize(ssray, pixel_size);
  return ssray;
}
