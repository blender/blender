/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Implementation of Horizon Based Global Illumination and Ambient Occlusion.
 *
 * This mostly follows the paper:
 * "Screen Space Indirect Lighting with Visibility Bitmask"
 * by Olivier Therrien, Yannick Levesque, Guillaume Gilet
 *
 * Expects `screen_radiance_tx` and `screen_normal_tx` to be bound if `HORIZON_OCCLUSION` is not
 * defined.
 */

#pragma BLENDER_REQUIRE(common_shape_lib.glsl)
#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_horizon_scan_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ray_types_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)

#ifdef HORIZON_OCCLUSION
/* Do nothing. */
#elif defined(MAT_DEFERRED) || defined(MAT_FORWARD)
/* Enable AO node computation for material shaders. */
#  define HORIZON_OCCLUSION
#else
#  define HORIZON_CLOSURE
#endif

vec3 horizon_scan_sample_radiance(vec2 uv)
{
#ifndef HORIZON_OCCLUSION
  return texture(screen_radiance_tx, uv).rgb;
#else
  return vec3(0.0);
#endif
}

vec3 horizon_scan_sample_normal(vec2 uv)
{
#ifndef HORIZON_OCCLUSION
  return texture(screen_normal_tx, uv).rgb * 2.0 - 1.0;
#else
  return vec3(0.0);
#endif
}

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

struct HorizonScanResult {
#ifdef HORIZON_OCCLUSION
  float result;
#endif
#ifdef HORIZON_CLOSURE
  SphericalHarmonicL1 result;
#endif
};

/**
 * Scans the horizon in many directions and returns the indirect lighting radiance.
 * Returned lighting is stored inside the context in `_accum` members already normalized.
 * If `reversed` is set to true, the input normal must be negated.
 */
HorizonScanResult horizon_scan_eval(vec3 vP,
                                    vec3 vN,
                                    vec2 noise,
                                    vec2 pixel_size,
                                    float search_distance,
                                    float global_thickness,
                                    float angle_bias,
                                    const int sample_count,
                                    const bool reversed)
{
  vec3 vV = drw_view_incident_vector(vP);

  const int slice_len = 2;
  vec2 v_dir = sample_circle(noise.x * (0.5 / float(slice_len)));

  float weight_accum = 0.0;
  float occlusion_accum = 0.0;
  SphericalHarmonicL1 sh_accum = spherical_harmonics_L1_new();

#if defined(GPU_METAL) && defined(GPU_APPLE)
/* NOTE: Full loop unroll hint increases performance on Apple Silicon. */
#  pragma clang loop unroll(full)
#endif
  for (int slice = 0; slice < slice_len; slice++) {
#if 0 /* For debug purpose. For when slice_len is greater than 2. */
    vec2 v_dir = sample_circle(((float(slice) + noise.x) / float(slice_len)));
#endif

    /* Setup integration domain around V. */
    vec3 vB = normalize(cross(vV, vec3(v_dir, 0.0)));
    vec3 vT = cross(vB, vV);

    /* Bitmask representing the occluded sectors on the slice. */
    uint slice_bitmask = 0u;

    /* Angle between vN and the horizon slice plane. */
    float vN_angle;
    /* Length of vN projected onto the horizon slice plane. */
    float vN_length;

    horizon_scan_projected_normal_to_plane_angle_and_length(vN, vV, vT, vB, vN_length, vN_angle);

    SphericalHarmonicL1 sh_slice = spherical_harmonics_L1_new();
    float weight_slice;

    /* For both sides of the view vector. */
    for (int side = 0; side < 2; side++) {
      Ray ray;
      ray.origin = vP;
      ray.direction = vec3((side == 0) ? v_dir : -v_dir, 0.0);
      ray.max_time = search_distance;

      /* TODO(fclem): Could save some computation here by computing entry and exit point on the
       * screen at once and just scan through. */
      ScreenSpaceRay ssray = raytrace_screenspace_ray_create(ray, pixel_size);

#if defined(GPU_METAL) && defined(GPU_APPLE)
/* NOTE: Full loop unroll hint increases performance on Apple Silicon. */
#  pragma clang loop unroll(full)
#endif
      for (int j = 0; j < sample_count; j++) {
        /* Always cross at least one pixel. */
        float time = 1.0 + square((float(j) + noise.y) / float(sample_count)) * ssray.max_time;

        if (reversed) {
          /* We need to cross at least 2 pixels to avoid artifacts form the HiZ storing only the
           * max depth. The HiZ would need to contain the min depth instead to avoid this. */
          time += 1.0;
        }

        float lod = 1.0 + (float(j >> 2) / (1.0 + uniform_buf.ao.quality));

        vec2 sample_uv = ssray.origin.xy + ssray.direction.xy * time;
        float sample_depth = textureLod(hiz_tx, sample_uv * uniform_buf.hiz.uv_scale, lod).r;

        if (sample_depth == 1.0 && !reversed) {
          /* Skip background. Avoids making shadow on the geometry near the far plane. */
          continue;
        }

        /* Bias depth a bit to avoid self shadowing issues. */
        const float bias = 2.0 * 2.4e-7;
        sample_depth += reversed ? -bias : bias;

        vec3 vP_sample = drw_point_screen_to_view(vec3(sample_uv, sample_depth));
        vec3 vV_sample = drw_view_incident_vector(vP_sample);

        Ray ray;
        ray.origin = vP_sample;
        ray.direction = -vV_sample;
        ray.max_time = global_thickness;

        if (reversed) {
          /* Make the ray start above the surface and end exactly at the surface. */
          ray.max_time = 2.0 * distance(vP, vP_sample);
          ray.origin = vP_sample + vV_sample * ray.max_time;
          ray.direction = -vV_sample;
        }

        Sphere sphere = shape_sphere(vP, search_distance);

        vec3 vP_front = ray.origin, vP_back = ray.origin + ray.direction * ray.max_time;
        horizon_scan_occluder_intersection_ray_sphere_clip(ray, sphere, vP_front, vP_back);

        vec3 vL_front = normalize(vP_front - vP);
        vec3 vL_back = normalize(vP_back - vP);

        /* Ordered pair of angle. Minimum in X, Maximum in Y.
         * Front will always have the smallest angle here since it is the closest to the view. */
        vec2 theta = acos_fast(vec2(dot(vL_front, vV), dot(vL_back, vV)));
        /* If we are tracing backward, the angles are negative. Swizzle to keep correct order. */
        theta = (side == 0) ? theta.xy : -theta.yx;

        vec3 sample_radiance = horizon_scan_sample_radiance(sample_uv);
        /* Take emitter surface normal into consideration. */
        vec3 sample_normal = horizon_scan_sample_normal(sample_uv);
        /* Discard back-facing samples.
         * The 2 factor is to avoid loosing too much energy (which is something not
         * explained in the paper...). Likely to be wrong, but we need a soft falloff. */
        float facing_weight = saturate(-dot(sample_normal, vL_front) * 2.0);

        float sample_weight = facing_weight * bsdf_lambert(vN, vL_front);

        /* Angular bias shrinks the visibility bitmask around the projected normal. */
        vec2 biased_theta = (theta - vN_angle) * angle_bias;
        uint sample_bitmask = horizon_scan_angles_to_bitmask(biased_theta);
        float weight_bitmask = horizon_scan_bitmask_to_visibility_uniform(sample_bitmask &
                                                                          ~slice_bitmask);

        sample_radiance *= facing_weight * weight_bitmask;
        /* Encoding using front sample direction gives better result than
         * `normalize(vL_front + vL_back)` */
        spherical_harmonics_encode_signal_sample(
            vL_front, vec4(sample_radiance, weight_bitmask), sh_slice);

        slice_bitmask |= sample_bitmask;
      }
    }

    float occlusion_slice = horizon_scan_bitmask_to_occlusion_cosine(slice_bitmask);

    /* Correct normal not on plane (Eq. 8 of GTAO paper). */
    occlusion_accum += occlusion_slice * vN_length;
    /* Use uniform visibility since this is what we use for near field lighting. */
    sh_accum = spherical_harmonics_madd(sh_slice, vN_length, sh_accum);

    weight_accum += vN_length;

    /* Rotate 90 degrees. */
    v_dir = orthogonal(v_dir);
  }

  float weight_rcp = safe_rcp(weight_accum);

  HorizonScanResult res;
#ifdef HORIZON_OCCLUSION
  res.result = occlusion_accum * weight_rcp;
#endif
#ifdef HORIZON_CLOSURE
  /* Weight by area of the sphere. This is expected for correct SH evaluation. */
  res.result = spherical_harmonics_mul(sh_accum, weight_rcp * 4.0 * M_PI);
#endif
  return res;
}
