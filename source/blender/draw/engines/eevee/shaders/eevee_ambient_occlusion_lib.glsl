/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"
#include "infos/eevee_uniform_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_global_ubo)
SHADER_LIBRARY_CREATE_INFO(eevee_hiz_data)

#include "draw_view_lib.glsl"
#include "eevee_ray_types_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_utility_tx_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_fast_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

/* TODO(Miguel Pozo): Move this function somewhere else. */
/* Return a fitted cone angle given the input roughness */
float ambient_occlusion_cone_cosine(float r)
{
  /* Using phong gloss
   * roughness = sqrt(2/(gloss+2)) */
  // float gloss = -2 + 2 / (r * r);
  /* Drobot 2014 in GPUPro5 */
  // return cos(2.0f * sqrt(2.0f / (gloss + 2)));
  /* Uludag 2014 in GPUPro5 */
  // return pow(0.244f, 1 / (gloss + 1));
  /* Jimenez 2016 in Practical Realtime Strategies for Accurate Indirect Occlusion. */
  return exp2(-3.32193f * r * r);
}

/* Based on Practical Realtime Strategies for Accurate Indirect Occlusion
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pptx
 */

#define AO_BENT_NORMALS true
#define AO_MULTI_BOUNCE true

struct OcclusionData {
  /* 4 horizon angles, one in each direction around the view vector to form a cross pattern. */
  float4 horizons;
  /* Custom large scale occlusion. */
  float custom_occlusion;
};

OcclusionData ambient_occlusion_data(float4 horizons, float custom_occlusion)
{
  OcclusionData data;
  data.horizons = horizons;
  data.custom_occlusion = custom_occlusion;
  return data;
}

/* No Occlusion Data. */
OcclusionData ambient_occlusion_disabled_data()
{
  return ambient_occlusion_data(float4(M_PI, -M_PI, M_PI, -M_PI), 1.0f);
}

float4 ambient_occlusion_pack_data(OcclusionData data)
{
  return float4(1.0f - data.horizons * float4(1, -1, 1, -1) * M_1_PI);
}

OcclusionData ambient_occlusion_unpack_data(float4 v)
{
  return ambient_occlusion_data((1.0f - v) * float4(1, -1, 1, -1) * M_PI, 0.0f);
}

float2 ambient_occlusion_get_noise(int2 texel)
{
  auto &tx = sampler_get(eevee_utility_texture, utility_tx);
  float2 noise = utility_tx_fetch(tx, float2(texel), UTIL_BLUE_NOISE_LAYER).xy;
  return fract(noise + sampling_rng_2D_get(SAMPLING_AO_U));
}

float2 ambient_occlusion_get_dir(float jitter)
{
  /* Only a quarter of a turn because we integrate using 2 slices.
   * We use this instead of using utiltex circle noise to improve cache hits
   * since all tracing direction will be in the same quadrant. */
  jitter *= M_PI_2;
  return float2(cos(jitter), sin(jitter));
}

/* Return horizon angle cosine. */
float ambient_ambient_occlusion_search_horizon(float3 vI,
                                               float3 vP,
                                               float noise,
                                               ScreenSpaceRay ssray,
                                               sampler2D depth_tx,
                                               const float inverted,
                                               float radius,
                                               const float sample_count)
{
  /* Init at cos(M_PI). */
  float h = (inverted != 0.0f) ? 1.0f : -1.0f;

  ssray.max_time -= 1.0f;

  if (ssray.max_time <= 2.0f) {
    /* Produces self shadowing under this threshold. */
    return acos_fast(h);
  }

  float prev_time, time = 0.0f;
  for (float iter = 0.0f; time < ssray.max_time && iter < sample_count; iter++) {
    prev_time = time;
    /* Gives us good precision at center and ensure we cross at least one pixel per iteration. */
    time = 1.0f + iter + square((iter + noise) / sample_count) * ssray.max_time;
    float stride = time - prev_time;
    float lod = (log2(stride) - noise) * uniform_buf.ao.lod_factor_ao;

    float2 uv = ssray.origin.xy + ssray.direction.xy * time;
    float depth = textureLod(depth_tx, uv * uniform_buf.hiz.uv_scale, floor(lod)).r;

    if (depth == 1.0f && inverted == 0.0f) {
      /* Skip background. Avoids making shadow on the geometry near the far plane. */
      continue;
    }

    /* Bias depth a bit to avoid self shadowing issues. */
    constexpr float bias = 2.0f * 2.4e-7f;
    depth += (inverted != 0.0f) ? -bias : bias;

    float3 s = drw_point_screen_to_view(float3(uv, depth));
    float3 omega_s = s - vP;
    float len = length(omega_s);
    /* Sample's horizon angle cosine. */
    float s_h = dot(vI, omega_s / len);
    /* Blend weight to fade artifacts. */
    float dist_ratio = abs(len) / radius;
    /* Sphere falloff. */
    float dist_fac = square(saturate(dist_ratio));
    /* Unbiased, gives too much hard cut behind objects */
    // float dist_fac = step(0.999f, dist_ratio);

    if (inverted != 0.0f) {
      h = min(h, s_h);
    }
    else {
      h = mix(max(h, s_h), h, dist_fac);
    }
  }
  return acos_fast(h);
}

OcclusionData ambient_occlusion_search(float3 vP,
                                       sampler2D depth_tx,
                                       int2 texel,
                                       float radius,
                                       const float inverted,
                                       const float dir_sample_count)
{
  float2 noise = ambient_occlusion_get_noise(texel);
  float2 dir = ambient_occlusion_get_dir(noise.x);
  float3 vI = (drw_view_is_perspective() ? normalize(-vP) : float3(0.0f, 0.0f, 1.0f));

  OcclusionData data = (inverted != 0.0f) ? ambient_occlusion_data(float4(0, 0, 0, 0), 1.0f) :
                                            ambient_occlusion_disabled_data();

  for (int i = 0; i < 2; i++) {
    Ray ray;
    ray.origin = vP;
    ray.direction = float3(dir, 0.0f);
    ray.max_time = radius;

    ScreenSpaceRay ssray;

    ssray = raytrace_screenspace_ray_create(ray, uniform_buf.ao.pixel_size);
    data.horizons[0 + i * 2] = ambient_ambient_occlusion_search_horizon(
        vI, vP, noise.y, ssray, depth_tx, inverted, radius, dir_sample_count);

    ray.direction = -ray.direction;

    ssray = raytrace_screenspace_ray_create(ray, uniform_buf.ao.pixel_size);
    data.horizons[1 + i * 2] = -ambient_ambient_occlusion_search_horizon(
        vI, vP, noise.y, ssray, depth_tx, inverted, radius, dir_sample_count);

    /* Rotate 90 degrees. */
    dir = float2(-dir.y, dir.x);
  }

  return data;
}

float2 ambient_occlusion_clamp_horizons_to_hemisphere(float2 horizons,
                                                      float angle_N,
                                                      const float inverted)
{
  /* Add a little bias to fight self shadowing. */
  constexpr float max_angle = M_PI_2 - 0.05f;

  if (inverted != 0.0f) {
    horizons.x = max(horizons.x, angle_N + max_angle);
    horizons.y = min(horizons.y, angle_N - max_angle);
  }
  else {
    horizons.x = min(horizons.x, angle_N + max_angle);
    horizons.y = max(horizons.y, angle_N - max_angle);
  }
  return horizons;
}

void ambient_occlusion_eval(OcclusionData data,
                            int2 texel,
                            float3 V,
                            float3 N,
                            float3 Ng,
                            const float inverted,
                            out float visibility,
                            out float visibility_error,
                            out float3 bent_normal)
{
  /* No error by default. */
  visibility_error = 1.0f;

  bool early_out = (inverted != 0.0f) ? (reduce_max(abs(data.horizons)) == 0.0f) :
                                        (reduce_min(abs(data.horizons)) == float(M_PI));
  if (early_out) {
    visibility = saturate(dot(N, Ng) * 0.5f + 0.5f);
    visibility = min(visibility, data.custom_occlusion);

    if (AO_BENT_NORMALS) {
      bent_normal = safe_normalize(N + Ng);
    }
    else {
      bent_normal = N;
    }
    return;
  }

  float2 noise = ambient_occlusion_get_noise(texel);
  float2 dir = ambient_occlusion_get_dir(noise.x);

  visibility_error = 0.0f;
  visibility = 0.0f;
  bent_normal = N * 0.001f;

  for (int i = 0; i < 2; i++) {
    float3 T = drw_normal_view_to_world(float3(dir, 0.0f));
    /* Setup integration domain around V. */
    float3 B = normalize(cross(V, T));
    T = normalize(cross(B, V));

    float proj_N_len;
    float3 proj_N = normalize_and_get_length(N - B * dot(N, B), proj_N_len);
    float3 proj_Ng = normalize(Ng - B * dot(Ng, B));

    float2 h = (i == 0) ? data.horizons.xy : data.horizons.zw;

    float N_sin = dot(proj_N, T);
    float Ng_sin = dot(proj_Ng, T);
    float N_cos = saturate(dot(proj_N, V));
    float Ng_cos = saturate(dot(proj_Ng, V));
    /* Gamma, angle between normalized projected normal and view vector. */
    float angle_Ng = sign(Ng_sin) * acos_fast(Ng_cos);
    float angle_N = sign(N_sin) * acos_fast(N_cos);
    /* Clamp horizons to hemisphere around shading normal. */
    h = ambient_occlusion_clamp_horizons_to_hemisphere(h, angle_N, inverted);

    float bent_angle = (h.x + h.y) * 0.5f;
    /* NOTE: here we multiply z by 0.5 as it shows less difference with the geometric normal.
     * Also modulate by projected normal length to reduce issues with slanted surfaces.
     * All of this is ad-hoc and not really grounded. */
    bent_normal += proj_N_len * (T * sin(bent_angle) + V * 0.5f * cos(bent_angle));

    /* Clamp to geometric normal only for integral to keep smooth bent normal. */
    /* This is done to match Cycles ground truth but adds some computation. */
    h = ambient_occlusion_clamp_horizons_to_hemisphere(h, angle_Ng, inverted);

    /* Inner integral (Eq. 7). */
    float a = dot(-cos(2.0f * h - angle_N) + N_cos + 2.0f * h * N_sin, float2(0.25f));
    /* Correct normal not on plane (Eq. 8). */
    visibility += proj_N_len * a;
    /* Using a very low number of slices (2) leads to over-darkening of surfaces orthogonal to
     * the view. This is particularly annoying for sharp reflections occlusion. So we compute how
     * much the error is and correct the visibility later. */
    visibility_error += proj_N_len;

    /* Rotate 90 degrees. */
    dir = float2(-dir.y, dir.x);
  }
  /* We integrated 2 directions. */
  visibility *= 0.5f;
  visibility_error *= 0.5f;

  visibility = min(visibility, data.custom_occlusion);

  if (AO_BENT_NORMALS) {
    /* NOTE: using pow(visibility, 6.0) produces NaN (see #87369). */
    float tmp = saturate(pow6f(visibility));
    bent_normal = normalize(mix(bent_normal, N, tmp));
  }
  else {
    bent_normal = N;
  }
}

/* Multi-bounce approximation base on surface albedo.
 * Page 78 in the PDF version. */
float ambient_occlusion_multibounce(float visibility, float3 albedo)
{
  if (!AO_MULTI_BOUNCE) {
    return visibility;
  }

  /* Median luminance. Because Colored multibounce looks bad. */
  float lum = dot(albedo, float3(0.3333f));

  float a = 2.0404f * lum - 0.3324f;
  float b = -4.7951f * lum + 0.6417f;
  float c = 2.7552f * lum + 0.6903f;

  float x = visibility;
  return max(x, ((x * a + b) * x + c) * x);
}

float ambient_occlusion_diffuse(OcclusionData data, int2 texel, float3 V, float3 N, float3 Ng)
{
  float3 unused;
  float unused_error;
  float visibility;
  ambient_occlusion_eval(data, texel, V, N, Ng, 0.0f, visibility, unused_error, unused);

  return saturate(visibility);
}

float ambient_occlusion_diffuse(OcclusionData data,
                                int2 texel,
                                float3 V,
                                float3 N,
                                float3 Ng,
                                float3 albedo,
                                out float3 bent_normal)
{
  float visibility;
  float unused_error;
  ambient_occlusion_eval(data, texel, V, N, Ng, 0.0f, visibility, unused_error, bent_normal);

  visibility = ambient_occlusion_multibounce(visibility, albedo);

  return saturate(visibility);
}

/**
 * Approximate the area of intersection of two spherical caps
 * radius1 : First cap radius (arc length in radians)
 * radius2 : Second cap radius (in radians)
 * dist : Distance between caps (radians between centers of caps)
 * NOTE: Result is divided by pi to save one multiply.
 */
float ambient_occlusion_spherical_cap_intersection(float radius1, float radius2, float dist)
{
  /* From "Ambient Aperture Lighting" by Chris Oat
   * Slide 15. */
  float max_radius = max(radius1, radius2);
  float min_radius = min(radius1, radius2);
  float sum_radius = radius1 + radius2;
  float area;
  if (dist <= max_radius - min_radius) {
    /* One cap in completely inside the other */
    area = 1.0f - cos(min_radius);
  }
  else if (dist >= sum_radius) {
    /* No intersection exists */
    area = 0;
  }
  else {
    float diff = max_radius - min_radius;
    area = smoothstep(0.0f, 1.0f, 1.0f - saturate((dist - diff) / (sum_radius - diff)));
    area *= 1.0f - cos(min_radius);
  }
  return area;
}

float ambient_occlusion_specular(
    OcclusionData data, int2 texel, float3 V, float3 N, float roughness, inout float3 specular_dir)
{
  float3 visibility_dir;
  float visibility_error;
  float visibility;
  ambient_occlusion_eval(data, texel, V, N, N, 0.0f, visibility, visibility_error, visibility_dir);

  /* Correct visibility error for very sharp surfaces. */
  visibility *= mix(safe_rcp(visibility_error), 1.0f, roughness);

  specular_dir = normalize(mix(specular_dir, visibility_dir, roughness * (1.0f - visibility)));

  /* Visibility to cone angle (eq. 18). */
  float vis_angle = acos_fast(sqrt(1 - visibility));
  /* Roughness to cone angle (eq. 26). */
  /* A 0.001 min_angle can generate NaNs on Intel GPUs. See D12508. */
  constexpr float min_angle = 0.00990998744964599609375f;
  float spec_angle = max(min_angle, acos_fast(ambient_occlusion_cone_cosine(roughness)));
  /* Angle between cone axes. */
  float cone_cone_dist = acos_fast(saturate(dot(visibility_dir, specular_dir)));
  float cone_nor_dist = acos_fast(saturate(dot(N, specular_dir)));

  float isect_solid_angle = ambient_occlusion_spherical_cap_intersection(
      vis_angle, spec_angle, cone_cone_dist);
  float specular_solid_angle = ambient_occlusion_spherical_cap_intersection(
      M_PI_2, spec_angle, cone_nor_dist);
  float specular_occlusion = isect_solid_angle / specular_solid_angle;
  /* Mix because it is unstable in unoccluded areas. */
  float tmp = saturate(pow8f(visibility));
  visibility = mix(specular_occlusion, 1.0f, tmp);

  return saturate(visibility);
}
