
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_uniforms_lib.glsl)

/**
 * Screen-Space Raytracing functions.
 */

uniform sampler2D maxzBuffer;
uniform sampler2DArray planarDepth;

struct Ray {
  vec3 origin;
  vec3 direction;
};

/* Inputs expected to be in viewspace. */
void raytrace_clip_ray_to_near_plane(inout Ray ray)
{
  float near_dist = get_view_z_from_depth(0.0);
  if ((ray.origin.z + ray.direction.z) > near_dist) {
    ray.direction *= abs((near_dist - ray.origin.z) / ray.direction.z);
  }
}

/* Screenspace ray ([0..1] "uv" range) where direction is normalize to be as small as one
 * full-resolution pixel. The ray is also clipped to all frustum sides.
 */
struct ScreenSpaceRay {
  vec4 origin;
  vec4 direction;
  float max_time;
};

void raytrace_screenspace_ray_finalize(inout ScreenSpaceRay ray)
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
    ray.direction.xy = vec2(0.0, 0.01);
  }
  /* Avoid divide by 0 error in line_unit_box_intersect_dist, leading to undefined behavior
   * (see T86429). */
  if (ray.direction.z == 0.0) {
    ray.direction.z = 0.0001;
  }
  float ray_len_sqr = len_squared(ray.direction.xyz);
  /* Make ray.direction cover one pixel. */
  bool is_more_vertical = abs(ray.direction.x) < abs(ray.direction.y);
  ray.direction /= (is_more_vertical) ? abs(ray.direction.y) : abs(ray.direction.x);
  ray.direction *= (is_more_vertical) ? ssrPixelSize.y : ssrPixelSize.x;
  /* Clip to segment's end. */
  ray.max_time = sqrt(ray_len_sqr * safe_rcp(len_squared(ray.direction.xyz)));
  /* Clipping to frustum sides. */
  float clip_dist = line_unit_box_intersect_dist(ray.origin.xyz, ray.direction.xyz);
  ray.max_time = min(ray.max_time, clip_dist);
  /* Convert to texture coords [0..1] range. */
  ray.origin = ray.origin * 0.5 + 0.5;
  ray.direction *= 0.5;
}

ScreenSpaceRay raytrace_screenspace_ray_create(Ray ray)
{
  ScreenSpaceRay ssray;
  ssray.origin.xyz = project_point(ProjectionMatrix, ray.origin);
  ssray.direction.xyz = project_point(ProjectionMatrix, ray.origin + ray.direction);

  raytrace_screenspace_ray_finalize(ssray);
  return ssray;
}

ScreenSpaceRay raytrace_screenspace_ray_create(Ray ray, float thickness)
{
  ScreenSpaceRay ssray;
  ssray.origin.xyz = project_point(ProjectionMatrix, ray.origin);
  ssray.direction.xyz = project_point(ProjectionMatrix, ray.origin + ray.direction);
  /* Interpolate thickness in screen space.
   * Calculate thickness further away to avoid near plane clipping issues. */
  ssray.origin.w = get_depth_from_view_z(ray.origin.z - thickness) * 2.0 - 1.0;
  ssray.direction.w = get_depth_from_view_z(ray.origin.z + ray.direction.z - thickness) * 2.0 -
                      1.0;

  raytrace_screenspace_ray_finalize(ssray);
  return ssray;
}

struct RayTraceParameters {
  /** ViewSpace thickness the objects  */
  float thickness;
  /** Jitter along the ray to avoid banding artifact when steps are too large. */
  float jitter;
  /** Determine how fast the sample steps are getting bigger. */
  float trace_quality;
  /** Determine how we can use lower depth mipmaps to make the tracing faster. */
  float roughness;
};

/* Return true on hit. */
/* __ray_dir__ is the ray direction premultiplied by its maximum length */
/* TODO fclem remove the backface check and do it the SSR resolve code. */
bool raytrace(Ray ray,
              RayTraceParameters params,
              const bool discard_backface,
              out vec3 hit_position)
{
  /* Clip to near plane for perspective view where there is a singularity at the camera origin. */
  if (ProjectionMatrix[3][3] == 0.0) {
    raytrace_clip_ray_to_near_plane(ray);
  }

  ScreenSpaceRay ssray = raytrace_screenspace_ray_create(ray, params.thickness);
  /* Avoid no iteration. */
  ssray.max_time = max(ssray.max_time, 1.1);

  float prev_delta = 0.0, prev_time = 0.0;
  float depth_sample = get_depth_from_view_z(ray.origin.z);
  float delta = depth_sample - ssray.origin.z;

  float lod_fac = saturate(fast_sqrt(params.roughness) * 2.0 - 0.4);

  /* Cross at least one pixel. */
  float t = 1.001, time = 1.001;
  bool hit = false;
  const float max_steps = 255.0;
  for (float iter = 1.0; !hit && (time < ssray.max_time) && (iter < max_steps); iter++) {
    float stride = 1.0 + iter * params.trace_quality;
    float lod = log2(stride) * lod_fac;

    prev_time = time;
    prev_delta = delta;

    time = min(t + stride * params.jitter, ssray.max_time);
    t += stride;

    vec4 ss_p = ssray.origin + ssray.direction * time;
    depth_sample = textureLod(maxzBuffer, ss_p.xy * hizUvScale.xy, floor(lod)).r;

    delta = depth_sample - ss_p.z;
    /* Check if the ray is below the surface ... */
    hit = (delta < 0.0);
    /* ... and above it with the added thickness. */
    hit = hit && (delta > ss_p.z - ss_p.w || abs(delta) < abs(ssray.direction.z * stride));
  }
  /* Discard backface hits. */
  hit = hit && !(discard_backface && prev_delta < 0.0);
  /* Reject hit if background. */
  hit = hit && (depth_sample != 1.0);
  /* Refine hit using intersection between the sampled heightfield and the ray.
   * This simplifies nicely to this single line. */
  time = mix(prev_time, time, saturate(prev_delta / (prev_delta - delta)));

  hit_position = ssray.origin.xyz + ssray.direction.xyz * time;

  return hit;
}

bool raytrace_planar(Ray ray, RayTraceParameters params, int planar_ref_id, out vec3 hit_position)
{
  /* Clip to near plane for perspective view where there is a singularity at the camera origin. */
  if (ProjectionMatrix[3][3] == 0.0) {
    raytrace_clip_ray_to_near_plane(ray);
  }

  ScreenSpaceRay ssray = raytrace_screenspace_ray_create(ray);

  /* Planar Reflections have X mirrored. */
  ssray.origin.x = 1.0 - ssray.origin.x;
  ssray.direction.x = -ssray.direction.x;

  float prev_delta = 0.0, prev_time = 0.0;
  float depth_sample = get_depth_from_view_z(ray.origin.z);
  float delta = depth_sample - ssray.origin.z;

  float t = 0.0, time = 0.0;
  /* On very sharp reflections, the ray can be perfectly aligned with the view direction
   * making the tracing useless. Bypass tracing in this case. */
  bool hit = (ssray.max_time < 1.0);
  const float max_steps = 255.0;
  for (float iter = 1.0; !hit && (time < ssray.max_time) && (iter < max_steps); iter++) {
    float stride = 1.0 + iter * params.trace_quality;

    prev_time = time;
    prev_delta = delta;

    time = min(t + stride * params.jitter, ssray.max_time);
    t += stride;

    vec4 ss_ray = ssray.origin + ssray.direction * time;

    depth_sample = texture(planarDepth, vec3(ss_ray.xy, planar_ref_id)).r;

    delta = depth_sample - ss_ray.z;
    /* Check if the ray is below the surface. */
    hit = (delta < 0.0);
  }
  /* Reject hit if background. */
  hit = hit && (depth_sample != 1.0);
  /* Refine hit using intersection between the sampled heightfield and the ray.
   * This simplifies nicely to this single line. */
  time = mix(prev_time, time, saturate(prev_delta / (prev_delta - delta)));

  hit_position = ssray.origin.xyz + ssray.direction.xyz * time;
  /* Planar Reflections have X mirrored. */
  hit_position.x = 1.0 - hit_position.x;

  return hit;
}

float screen_border_mask(vec2 hit_co)
{
  const float margin = 0.003;
  float atten = ssrBorderFac + margin; /* Screen percentage */
  hit_co = smoothstep(0.0, atten, hit_co) * (1.0 - smoothstep(1.0 - atten, 1.0, hit_co));

  float screenfade = hit_co.x * hit_co.y;

  return screenfade;
}
