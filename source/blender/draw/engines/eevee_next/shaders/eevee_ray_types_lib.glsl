
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)

/**
 * Screen-Space Raytracing functions.
 */

struct Ray {
  vec3 origin;
  /* Ray direction premultiplied by its maximum length. */
  vec3 direction;
};

/* Screenspace ray ([0..1] "uv" range) where direction is normalize to be as small as one
 * full-resolution pixel. The ray is also clipped to all frustum sides.
 */
struct ScreenSpaceRay {
  vec4 origin;
  vec4 direction;
  float max_time;
};

void raytrace_screenspace_ray_finalize(inout ScreenSpaceRay ray, vec2 pixel_size)
{
  /* Constant bias (due to depth buffer precision). Helps with self intersection. */
  /* Magic numbers for 24bits of precision.
   * From http://terathon.com/gdc07_lengyel.pdf (slide 26) */
  const float bias = -2.4e-7 * 2.0;
  ray.origin.zw += bias;
  ray.direction.zw += bias;

  ray.direction -= ray.origin;
  /* If the line is degenerate, make it cover at least one pixel
   * to not have to handle zero-pixel extent as a special case later */
  if (len_squared(ray.direction.xy) < 0.00001) {
    ray.direction.xy = vec2(0.0, 0.00001);
  }
  float ray_len_sqr = len_squared(ray.direction.xyz);
  /* Make ray.direction cover one pixel. */
  bool is_more_vertical = abs(ray.direction.x / pixel_size.x) <
                          abs(ray.direction.y / pixel_size.y);
  ray.direction /= (is_more_vertical) ? abs(ray.direction.y) : abs(ray.direction.x);
  ray.direction *= (is_more_vertical) ? pixel_size.y : pixel_size.x;
  /* Clip to segment's end. */
  ray.max_time = sqrt(ray_len_sqr * safe_rcp(len_squared(ray.direction.xyz)));
  /* Clipping to frustum sides. */
  float clip_dist = line_unit_box_intersect_dist_safe(ray.origin.xyz, ray.direction.xyz);
  ray.max_time = min(ray.max_time, clip_dist);
  /* Convert to texture coords [0..1] range. */
  ray.origin = ray.origin * 0.5 + 0.5;
  ray.direction *= 0.5;
}

ScreenSpaceRay raytrace_screenspace_ray_create(Ray ray, vec2 pixel_size)
{
  ScreenSpaceRay ssray;
  ssray.origin.xyz = project_point(ProjectionMatrix, ray.origin);
  ssray.direction.xyz = project_point(ProjectionMatrix, ray.origin + ray.direction);

  raytrace_screenspace_ray_finalize(ssray, pixel_size);
  return ssray;
}
