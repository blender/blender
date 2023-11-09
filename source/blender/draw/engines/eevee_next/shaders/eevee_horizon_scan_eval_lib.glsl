/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Implementation of Horizon Based Global Illumination and Ambient Occlusion.
 *
 * This mostly follows the paper:
 * "Screen Space Indirect Lighting with Visibility Bitmask"
 * by Olivier Therrien, Yannick Levesque, Guillaume Gilet
 */

#pragma BLENDER_REQUIRE(common_shape_lib.glsl)
#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_horizon_scan_lib.glsl)

/**
 * Returns the start and end point of a ray clipped to its intersection
 * with a sphere.
 */
void horizon_scan_occluder_intersection_ray_sphere_clip(Ray ray,
                                                        Sphere sphere,
                                                        out vec3 P_entry,
                                                        out vec3 P_exit)
{
  vec3 f = ray.origin - sphere.center;
  float a = length_squared(ray.direction);
  float b = 2.0 * dot(ray.direction, f);
  float c = length_squared(f) - square(sphere.radius);
  float determinant = b * b - 4.0 * a * c;
  if (determinant <= 0.0) {
    /* No intersection. Return null segment. */
    P_entry = P_exit = ray.origin;
    return;
  }
  /* Using fast sqrt_fast doesn't seem to cause artifact here. */
  float t_min = (-b - sqrt_fast(determinant)) / (2.0 * a);
  float t_max = (-b + sqrt_fast(determinant)) / (2.0 * a);
  /* Clip segment to the intersection range. */
  float t_entry = clamp(0.0, t_min, t_max);
  float t_exit = clamp(ray.max_time, t_min, t_max);

  P_entry = ray.origin + ray.direction * t_entry;
  P_exit = ray.origin + ray.direction * t_exit;
}

/**
 * Scans the horizon in many directions and returns the indirect lighting radiance.
 * Returned lighting depends on configuration.
 */
vec3 horizon_scan_eval(vec3 vP,
                       vec3 vN,
                       sampler2D depth_tx,
                       vec2 noise,
                       vec2 pixel_size,
                       float search_distance,
                       float global_thickness,
                       float angle_bias,
                       const int sample_count)
{
  vec3 vV = drw_view_incident_vector(vP);

  /* Only a quarter of a turn because we integrate using 2 slices.
   * We use this instead of using full circle noise to improve cache hits
   * since all tracing direction will be in the same quadrant. */
  vec2 v_dir = sample_circle(noise.x * 0.25);

  vec3 accum_light = vec3(0.0);
  float accum_weight = 0.0;

  for (int i = 0; i < 2; i++) {
    /* Setup integration domain around V. */
    vec3 vB = normalize(cross(vV, vec3(v_dir, 0.0)));
    vec3 vT = cross(vB, vV);
    /* Projected view normal onto the integration plane. */
    float vN_proj_len;
    vec3 vN_proj = normalize_and_get_length(vN - vB * dot(vN, vB), vN_proj_len);

    float vN_sin = dot(vN_proj, vT);
    float vN_cos = saturate(dot(vN_proj, vV));
    /* Angle between normalized projected normal and view vector. */
    float vN_angle = sign(vN_sin) * acos_fast(vN_cos);

    vec3 slice_light = vec3(0.0);
    uint slice_bitmask = 0u;

    /* For both sides of the view vector. */
    for (int side = 0; side < 2; side++) {
      Ray ray;
      ray.origin = vP;
      ray.direction = vec3((side == 0) ? v_dir : -v_dir, 0.0);
      ray.max_time = search_distance;

      /* TODO(fclem): Could save some computation here by computing entry and exit point on the
       * screen at once and just scan through. */
      ScreenSpaceRay ssray = raytrace_screenspace_ray_create(ray, pixel_size);

      for (int j = 0; j < sample_count; j++) {
        /* Always cross at least one pixel. */
        float time = 1.0 + square((float(j) + noise.y) / float(sample_count)) * ssray.max_time;

        float lod = float(j >> 2) / (1.0 + uniform_buf.ao.quality);

        vec2 sample_uv = ssray.origin.xy + ssray.direction.xy * time;
        float sample_depth =
            textureLod(depth_tx, sample_uv * uniform_buf.hiz.uv_scale, floor(lod)).r;

        if (sample_depth == 1.0) {
          /* Skip background. Avoids making shadow on the geometry near the far plane. */
          continue;
        }

        bool front_facing = vN.z > 0.0;

        /* Bias depth a bit to avoid self shadowing issues. */
        const float bias = 2.0 * 2.4e-7;
        sample_depth += front_facing ? bias : -bias;

        vec3 vP_sample = drw_point_screen_to_view(vec3(sample_uv, sample_depth));

        Ray ray;
        ray.origin = vP_sample;
        ray.direction = -vV;
        ray.max_time = global_thickness;

        Sphere sphere = shape_sphere(vP, search_distance);

        vec3 vP_front, vP_back;
        horizon_scan_occluder_intersection_ray_sphere_clip(ray, sphere, vP_front, vP_back);

        vec3 vL_front = normalize(vP_front - vP);
        vec3 vL_back = normalize(vP_back - vP);

        /* Ordered pair of angle. Minimum in X, Maximum in Y.
         * Front will always have the smallest angle here since it is the closest to the view. */
        vec2 theta = acos_fast(vec2(dot(vL_front, vV), dot(vL_back, vV)));
        /* If we are tracing backward, the angles are negative. Swizzle to keep correct order. */
        theta = (side == 0) ? theta.xy : -theta.yx;
        theta -= vN_angle;
        /* Angular bias. Shrink the visibility bitmask around the projected normal. */
        theta *= angle_bias;

        uint sample_bitmask = horizon_scan_angles_to_bitmask(theta);
#ifdef USE_RADIANCE_ACCUMULATION
        float sample_visibility = horizon_scan_bitmask_to_visibility_uniform(sample_bitmask &
                                                                             ~slice_bitmask);
        if (sample_visibility > 0.0) {
          vec3 sample_radiance = horizon_scan_sample_radiance(sample_uv);
#  ifdef USE_NORMAL_MASKING
          vec3 sample_normal = horizon_scan_sample_normal(sample_uv);
          sample_visibility *= dot(sample_normal, -vL_front);
#  endif
          slice_light += sample_radiance * (bsdf_eval(vN, vL_front) * sample_visibility);
        }
#endif
        slice_bitmask |= sample_bitmask;
      }
    }

    /* Add distant lighting. */
    slice_light = vec3(horizon_scan_bitmask_to_occlusion_cosine(slice_bitmask));
    /* Correct normal not on plane (Eq. 8 of GTAO paper). */
    accum_light += slice_light * vN_proj_len;
    accum_weight += vN_proj_len;

    /* Rotate 90 degrees. */
    v_dir = orthogonal(v_dir);
  }
  return accum_light * safe_rcp(accum_weight);
}
