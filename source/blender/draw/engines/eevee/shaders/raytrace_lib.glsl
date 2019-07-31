#define MAX_STEP 256

float sample_depth(vec2 uv, int index, float lod)
{
#ifdef PLANAR_PROBE_RAYTRACE
  if (index > -1) {
    return textureLod(planarDepth, vec3(uv, index), 0.0).r;
  }
  else {
#endif
    /* Correct UVs for mipmaping mis-alignment */
    uv *= mipRatio[int(lod) + hizMipOffset];
    return textureLod(maxzBuffer, uv, lod).r;
#ifdef PLANAR_PROBE_RAYTRACE
  }
#endif
}

vec4 sample_depth_grouped(vec4 uv1, vec4 uv2, int index, float lod)
{
  vec4 depths;
#ifdef PLANAR_PROBE_RAYTRACE
  if (index > -1) {
    depths.x = textureLod(planarDepth, vec3(uv1.xy, index), 0.0).r;
    depths.y = textureLod(planarDepth, vec3(uv1.zw, index), 0.0).r;
    depths.z = textureLod(planarDepth, vec3(uv2.xy, index), 0.0).r;
    depths.w = textureLod(planarDepth, vec3(uv2.zw, index), 0.0).r;
  }
  else {
#endif
    depths.x = textureLod(maxzBuffer, uv1.xy, lod).r;
    depths.y = textureLod(maxzBuffer, uv1.zw, lod).r;
    depths.z = textureLod(maxzBuffer, uv2.xy, lod).r;
    depths.w = textureLod(maxzBuffer, uv2.zw, lod).r;
#ifdef PLANAR_PROBE_RAYTRACE
  }
#endif
  return depths;
}

float refine_isect(float prev_delta, float curr_delta)
{
  /**
   * Simplification of 2D intersection :
   * r0 = (0.0, prev_ss_ray.z);
   * r1 = (1.0, curr_ss_ray.z);
   * d0 = (0.0, prev_hit_depth_sample);
   * d1 = (1.0, curr_hit_depth_sample);
   * vec2 r = r1 - r0;
   * vec2 d = d1 - d0;
   * vec2 isect = ((d * cross(r1, r0)) - (r * cross(d1, d0))) / cross(r,d);
   *
   * We only want isect.x to know how much stride we need. So it simplifies :
   *
   * isect_x = (cross(r1, r0) - cross(d1, d0)) / cross(r,d);
   * isect_x = (prev_ss_ray.z - prev_hit_depth_sample.z) / cross(r,d);
   */
  return saturate(prev_delta / (prev_delta - curr_delta));
}

void prepare_raycast(vec3 ray_origin,
                     vec3 ray_dir,
                     float thickness,
                     int index,
                     out vec4 ss_step,
                     out vec4 ss_ray,
                     out float max_time)
{
  /* Negate the ray direction if it goes towards the camera.
   * This way we don't need to care if the projected point
   * is behind the near plane. */
  float z_sign = -sign(ray_dir.z);
  vec3 ray_end = ray_origin + z_sign * ray_dir;

  /* Project into screen space. */
  vec4 ss_start, ss_end;
  ss_start.xyz = project_point(ProjectionMatrix, ray_origin);
  ss_end.xyz = project_point(ProjectionMatrix, ray_end);

  /* We interpolate the ray Z + thickness values to check if depth is within threshold. */
  ray_origin.z -= thickness;
  ray_end.z -= thickness;
  ss_start.w = project_point(ProjectionMatrix, ray_origin).z;
  ss_end.w = project_point(ProjectionMatrix, ray_end).z;

  /* XXX This is a hack. A better method is welcome! */
  /* We take the delta between the offseted depth and the depth and substract it from the ray
   * depth. This will change the world space thickness appearance a bit but we can have negative
   * values without worries. We cannot do this in viewspace because of the perspective division. */
  ss_start.w = 2.0 * ss_start.z - ss_start.w;
  ss_end.w = 2.0 * ss_end.z - ss_end.w;

  ss_step = ss_end - ss_start;
  max_time = length(ss_step.xyz);
  ss_step = z_sign * ss_step / length(ss_step.xyz);

  /* If the line is degenerate, make it cover at least one pixel
   * to not have to handle zero-pixel extent as a special case later */
  ss_step.xy += vec2((dot(ss_step.xy, ss_step.xy) < 0.00001) ? 0.001 : 0.0);

  /* Make ss_step cover one pixel. */
  ss_step /= max(abs(ss_step.x), abs(ss_step.y));
  ss_step *= (abs(ss_step.x) > abs(ss_step.y)) ? ssrPixelSize.x : ssrPixelSize.y;

  /* Clip to segment's end. */
  max_time /= length(ss_step.xyz);

  /* Clipping to frustum sides. */
  max_time = min(max_time, line_unit_box_intersect_dist(ss_start.xyz, ss_step.xyz));

  /* Convert to texture coords. Z component included
   * since this is how it's stored in the depth buffer.
   * 4th component how far we are on the ray */
#ifdef PLANAR_PROBE_RAYTRACE
  /* Planar Reflections have X mirrored. */
  vec2 m = (index > -1) ? vec2(-0.5, 0.5) : vec2(0.5);
#else
  const vec2 m = vec2(0.5);
#endif
  ss_ray = ss_start * m.xyyy + 0.5;
  ss_step *= m.xyyy;

  ss_ray.xy += m * ssrPixelSize * 2.0; /* take the center of the texel. * 2 because halfres. */
}

/* See times_and_deltas. */
#define curr_time times_and_deltas.x
#define prev_time times_and_deltas.y
#define curr_delta times_and_deltas.z
#define prev_delta times_and_deltas.w

// #define GROUPED_FETCHES /* is still slower, need to see where is the bottleneck. */
/* Return the hit position, and negate the z component (making it positive) if not hit occurred. */
/* __ray_dir__ is the ray direction premultiplied by it's maximum length */
vec3 raycast(int index,
             vec3 ray_origin,
             vec3 ray_dir,
             float thickness,
             float ray_jitter,
             float trace_quality,
             float roughness,
             const bool discard_backface)
{
  vec4 ss_step, ss_start;
  float max_time;
  prepare_raycast(ray_origin, ray_dir, thickness, index, ss_step, ss_start, max_time);

  float max_trace_time = max(0.01, max_time - 0.01);

#ifdef GROUPED_FETCHES
  ray_jitter *= 0.25;
#endif

  /* x : current_time, y: previous_time, z: current_delta, w: previous_delta */
  vec4 times_and_deltas = vec4(0.0);

  float ray_time = 0.0;
  float depth_sample = sample_depth(ss_start.xy, index, 0.0);
  curr_delta = depth_sample - ss_start.z;

  float lod_fac = saturate(fast_sqrt(roughness) * 2.0 - 0.4);
  bool hit = false;
  float iter;
  for (iter = 1.0; !hit && (ray_time < max_time) && (iter < MAX_STEP); iter++) {
    /* Minimum stride of 2 because we are using half res minmax zbuffer. */
    float stride = max(1.0, iter * trace_quality) * 2.0;
    float lod = log2(stride * 0.5 * trace_quality) * lod_fac;
    ray_time += stride;

    /* Save previous values. */
    times_and_deltas.xyzw = times_and_deltas.yxwz;

#ifdef GROUPED_FETCHES
    stride *= 4.0;
    vec4 jit_stride = mix(vec4(2.0), vec4(stride), vec4(0.0, 0.25, 0.5, 0.75) + ray_jitter);

    vec4 times = min(vec4(ray_time) + jit_stride, vec4(max_trace_time));

    vec4 uv1 = ss_start.xyxy + ss_step.xyxy * times.xxyy;
    vec4 uv2 = ss_start.xyxy + ss_step.xyxy * times.zzww;

    vec4 depth_samples = sample_depth_grouped(uv1, uv2, index, lod);

    vec4 ray_z = ss_start.zzzz + ss_step.zzzz * times.xyzw;
    vec4 ray_w = ss_start.wwww + ss_step.wwww * vec4(prev_time, times.xyz);

    vec4 deltas = depth_samples - ray_z;
    /* Same as component wise (curr_delta <= 0.0) && (prev_w <= depth_sample). */
    bvec4 test = equal(step(deltas, vec4(0.0)) * step(ray_w, depth_samples), vec4(1.0));
    hit = any(test);

    if (hit) {
      vec2 m = vec2(1.0, 0.0); /* Mask */

      vec4 ret_times_and_deltas = times.wzzz * m.xxyy + deltas.wwwz * m.yyxx;
      ret_times_and_deltas = (test.z) ? times.zyyy * m.xxyy + deltas.zzzy * m.yyxx :
                                        ret_times_and_deltas;
      ret_times_and_deltas = (test.y) ? times.yxxx * m.xxyy + deltas.yyyx * m.yyxx :
                                        ret_times_and_deltas;
      times_and_deltas = (test.x) ? times.xxxx * m.xyyy + deltas.xxxx * m.yyxy +
                                        times_and_deltas.yyww * m.yxyx :
                                    ret_times_and_deltas;

      depth_sample = depth_samples.w;
      depth_sample = (test.z) ? depth_samples.z : depth_sample;
      depth_sample = (test.y) ? depth_samples.y : depth_sample;
      depth_sample = (test.x) ? depth_samples.x : depth_sample;
    }
    else {
      curr_time = times.w;
      curr_delta = deltas.w;
    }
#else
    float jit_stride = mix(2.0, stride, ray_jitter);

    curr_time = min(ray_time + jit_stride, max_trace_time);
    vec4 ss_ray = ss_start + ss_step * curr_time;

    depth_sample = sample_depth(ss_ray.xy, index, lod);

    float prev_w = ss_start.w + ss_step.w * prev_time;
    curr_delta = depth_sample - ss_ray.z;
    hit = (curr_delta <= 0.0) && (prev_w <= depth_sample);
#endif
  }

  if (discard_backface) {
    /* Discard backface hits */
    hit = hit && (prev_delta > 0.0);
  }

  /* Reject hit if background. */
  hit = hit && (depth_sample != 1.0);

  curr_time = (hit) ? mix(prev_time, curr_time, refine_isect(prev_delta, curr_delta)) : curr_time;
  ray_time = (hit) ? curr_time : ray_time;

  /* Clip to frustum. */
  ray_time = max(0.001, min(ray_time, max_time - 1.5));

  vec4 ss_ray = ss_start + ss_step * ray_time;

  /* Tag Z if ray failed. */
  ss_ray.z *= (hit) ? 1.0 : -1.0;
  return ss_ray.xyz;
}

float screen_border_mask(vec2 hit_co)
{
  const float margin = 0.003;
  float atten = ssrBorderFac + margin; /* Screen percentage */
  hit_co = smoothstep(margin, atten, hit_co) * (1 - smoothstep(1.0 - atten, 1.0 - margin, hit_co));

  float screenfade = hit_co.x * hit_co.y;

  return screenfade;
}
